#include "WorldSerializer.h"

#include "Crc32.h"
#include "TextArchive.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace Serialization
{
    namespace
    {
        constexpr std::uint8_t MAGIC[4] = {'U', 'B', 'S', 'V'};
        // First line of a plain text save
        constexpr const char* TEXT_MAGIC = "UBSV-TEXT";
        constexpr std::uint32_t TEXT_FORMAT_VERSION = 1;

        SaveFormat g_FileFormat = SaveFormat::Binary;

        // Living entities that have the serializer's component
        std::vector<Entity> Owners(ECSManager& ecs, const ComponentSerializer& serializer, const std::vector<Entity>& living)
        {
            std::vector<Entity> owners;
            for (Entity e : living)
            {
                if (serializer.Has(ecs, e))
                    owners.push_back(e);
            }
            return owners;
        }

        // Entity id lists in text: [count] then ids, runs of consecutive ids
        // written as first..last (the free id queue is mostly one long run)
        void WriteEntityList(TextOutputArchive& ar, const std::vector<Entity>& ids)
        {
            ar.WriteSize(ids.size());
            std::size_t i = 0;
            while (i < ids.size())
            {
                std::size_t j = i;
                while (j + 1 < ids.size() && ids[j + 1] == ids[j] + 1)
                    ++j;
                if (j - i >= 2)
                    ar.Raw(" " + std::to_string(ids[i]) + ".." + std::to_string(ids[j]));
                else
                {
                    for (std::size_t k = i; k <= j; ++k)
                        ar.Raw(" " + std::to_string(ids[k]));
                }
                i = j + 1;
            }
        }

        std::vector<Entity> ReadEntityList(TextInputArchive& ar)
        {
            std::string countToken = ar.NextToken("[count]");
            if (countToken.size() < 3 || countToken.front() != '[' || countToken.back() != ']')
                ar.Fail("expected a [count] of entities, got '" + countToken + "'");
            unsigned long count = std::strtoul(countToken.c_str() + 1, nullptr, 10);
            if (count > MAX_ENTITIES)
                ar.Fail("more entities than MAX_ENTITIES");
            std::vector<Entity> ids;
            ids.reserve(count);
            while (ids.size() < count)
            {
                std::string token = ar.NextToken("an entity id");
                std::size_t dots = token.find("..");
                char* end = nullptr;
                unsigned long first = std::strtoul(token.c_str(), &end, 10);
                unsigned long last = first;
                if (dots != std::string::npos)
                {
                    if (end != token.c_str() + dots)
                        ar.Fail("bad entity range '" + token + "'");
                    last = std::strtoul(token.c_str() + dots + 2, &end, 10);
                }
                if (end == token.c_str() || *end != '\0' || last < first || last >= MAX_ENTITIES ||
                    ids.size() + (last - first + 1) > count)
                    ar.Fail("bad entity id or range '" + token + "'");
                for (unsigned long id = first; id <= last; ++id)
                    ids.push_back(static_cast<Entity>(id));
            }
            return ids;
        }

        constexpr std::uint32_t MakeChunkID(char a, char b, char c, char d)
        {
            return static_cast<std::uint32_t>(static_cast<std::uint8_t>(a)) |
                   (static_cast<std::uint32_t>(static_cast<std::uint8_t>(b)) << 8) |
                   (static_cast<std::uint32_t>(static_cast<std::uint8_t>(c)) << 16) |
                   (static_cast<std::uint32_t>(static_cast<std::uint8_t>(d)) << 24);
        }

        constexpr std::uint32_t CHUNK_META = MakeChunkID('M', 'E', 'T', 'A');
        constexpr std::uint32_t CHUNK_ENTITIES = MakeChunkID('E', 'N', 'T', 'S');
        constexpr std::uint32_t CHUNK_COMPONENTS = MakeChunkID('C', 'O', 'M', 'P');
        constexpr std::uint32_t CHUNK_RESOURCES = MakeChunkID('R', 'S', 'R', 'C');
        constexpr std::uint32_t CHUNK_END = MakeChunkID('E', 'N', 'D', ' ');

        /**
         * \brief Writes a chunk header and back-patches its size once the payload
         *        has been written
         */
        class ChunkWriter
        {
          public:
            ChunkWriter(OutputArchive& ar, std::uint32_t id)
                : m_Ar(ar)
            {
                m_Ar.WritePrimitive(id);
                m_SizeOffset = m_Ar.Size();
                m_Ar.WritePrimitive(std::uint64_t{0});
            }

            ~ChunkWriter()
            {
                std::uint64_t payloadSize = m_Ar.Size() - m_SizeOffset - sizeof(std::uint64_t);
                m_Ar.PatchU64(m_SizeOffset, payloadSize);
            }

          private:
            OutputArchive& m_Ar;
            std::size_t m_SizeOffset;
        };

        /**
         * \brief Writes a u32 byte size placeholder and back-patches it
         */
        class SizedRecordWriter
        {
          public:
            explicit SizedRecordWriter(OutputArchive& ar)
                : m_Ar(ar)
            {
                m_SizeOffset = m_Ar.Size();
                m_Ar.WritePrimitive(std::uint32_t{0});
            }

            ~SizedRecordWriter()
            {
                std::size_t size = m_Ar.Size() - m_SizeOffset - sizeof(std::uint32_t);
                m_Ar.PatchU32(m_SizeOffset, static_cast<std::uint32_t>(size));
            }

          private:
            OutputArchive& m_Ar;
            std::size_t m_SizeOffset;
        };

        std::string ChunkName(std::uint32_t id)
        {
            std::string name;
            for (int i = 0; i < 4; ++i)
            {
                char c = static_cast<char>((id >> (8 * i)) & 0xFF);
                name += (c >= 32 && c < 127) ? c : '?';
            }
            return name;
        }
    } // namespace

    //-----------------------------------------------------------------------------
    // Saving
    //-----------------------------------------------------------------------------

    std::vector<std::uint8_t> WorldSerializer::Save(ECSManager& ecs,
                                                    const SaveMetadata& metadata) const
    {
        OutputArchive ar;
        ar.WriteBytes(MAGIC, sizeof(MAGIC));
        ar.WritePrimitive(FORMAT_VERSION);
        ar.WritePrimitive(std::uint32_t{0}); // flags, reserved

        // -- Metadata --------------------------------------------------------
        {
            ChunkWriter chunk(ar, CHUNK_META);
            SaveMetadata copy = metadata;
            ar(copy);
        }

        // -- Entities --------------------------------------------------------
        std::vector<Entity> living = ecs.GetLivingEntities();
        {
            ChunkWriter chunk(ar, CHUNK_ENTITIES);
            std::vector<Entity> available = ecs.GetAvailableEntities();
            std::uint32_t maxEntities = MAX_ENTITIES;
            ar(maxEntities, living, available);
        }

        // -- Components ------------------------------------------------------
        {
            ChunkWriter chunk(ar, CHUNK_COMPONENTS);
            // Types no entity has are left out: a file only depends on the
            // components it uses, not on everything the program registers
            std::vector<std::pair<const ComponentSerializer*, std::vector<Entity>>> used;
            for (const ComponentSerializer& serializer : m_Registry.Components())
            {
                std::vector<Entity> owners = Owners(ecs, serializer, living);
                if (!owners.empty())
                    used.emplace_back(&serializer, std::move(owners));
            }
            ar.WriteSize(used.size());
            for (auto& [serializerPtr, owners] : used)
            {
                const ComponentSerializer& serializer = *serializerPtr;
                std::string name = serializer.Name;
                std::uint32_t version = serializer.Version;
                ar(name, version);
                ar.WriteSize(owners.size());
                ar.SetVersion(version);
                for (Entity e : owners)
                {
                    ar.WritePrimitive(e);
                    SizedRecordWriter record(ar);
                    serializer.Save(ecs, e, ar);
                }
            }
        }

        // -- Resources -------------------------------------------------------
        {
            ChunkWriter chunk(ar, CHUNK_RESOURCES);
            std::vector<const ResourceSerializer*> present;
            for (const ResourceSerializer& serializer : m_Registry.Resources())
            {
                if (serializer.Has(ecs))
                    present.push_back(&serializer);
            }
            ar.WriteSize(present.size());
            for (const ResourceSerializer* serializer : present)
            {
                std::string name = serializer->Name;
                std::uint32_t version = serializer->Version;
                ar(name, version);
                ar.SetVersion(version);
                SizedRecordWriter record(ar);
                serializer->Save(ecs, ar);
            }
        }

        {
            ChunkWriter chunk(ar, CHUNK_END);
        }

        std::uint32_t crc = Crc32(ar.Buffer().data(), ar.Size());
        ar.WritePrimitive(crc);
        return ar.TakeBuffer();
    }

    //-----------------------------------------------------------------------------
    // Loading
    //-----------------------------------------------------------------------------

    LoadResult WorldSerializer::Parse(const std::vector<std::uint8_t>& bytes,
                                      WorldSnapshot& snapshot) const
    {
        if (!IsTextSave(bytes))
            return ParseBinary(bytes, snapshot);

        // Text saves are converted to binary and parsed like any binary save
        std::vector<std::string> warnings;
        std::vector<std::uint8_t> binary;
        try
        {
            binary = TextToBinary(std::string(bytes.begin(), bytes.end()), warnings);
        }
        catch (const std::exception& e)
        {
            snapshot = WorldSnapshot{};
            LoadResult failed;
            failed.Error = std::string("Text save: ") + e.what();
            return failed;
        }
        LoadResult result = ParseBinary(binary, snapshot);
        if (result)
        {
            snapshot.Warnings.insert(snapshot.Warnings.begin(), warnings.begin(), warnings.end());
            result.Warnings = snapshot.Warnings;
        }
        return result;
    }

    LoadResult WorldSerializer::ParseBinary(const std::vector<std::uint8_t>& bytes,
                                            WorldSnapshot& snapshot) const
    {
        LoadResult result;
        snapshot = WorldSnapshot{};
        try
        {
            constexpr std::size_t headerSize = sizeof(MAGIC) + 2 * sizeof(std::uint32_t);
            constexpr std::size_t footerSize = sizeof(std::uint32_t);
            if (bytes.size() < headerSize + footerSize)
                throw SerializationError("File too small to be a save file");
            if (std::memcmp(bytes.data(), MAGIC, sizeof(MAGIC)) != 0)
                throw SerializationError("Not a save file (bad magic)");

            // Verify the checksum before interpreting anything else
            std::size_t bodySize = bytes.size() - footerSize;
            InputArchive footer(bytes.data() + bodySize, footerSize);
            std::uint32_t storedCrc = 0;
            footer.ReadPrimitive(storedCrc);
            if (Crc32(bytes.data(), bodySize) != storedCrc)
                throw SerializationError("Save file is corrupted (checksum mismatch)");

            InputArchive ar(bytes.data(), bodySize);
            ar.Skip(sizeof(MAGIC));
            std::uint32_t formatVersion = 0;
            std::uint32_t flags = 0;
            ar.ReadPrimitive(formatVersion);
            ar.ReadPrimitive(flags);
            if (formatVersion == 0 || formatVersion > FORMAT_VERSION)
            {
                throw SerializationError("Unsupported save format version " +
                                         std::to_string(formatVersion));
            }

            std::unordered_set<std::uint32_t> seenChunks;
            bool reachedEnd = false;
            while (!reachedEnd)
            {
                std::uint32_t chunkID = 0;
                std::uint64_t chunkSize = 0;
                ar.ReadPrimitive(chunkID);
                ar.ReadPrimitive(chunkSize);
                if (chunkSize > ar.Remaining())
                    throw SerializationError("Chunk " + ChunkName(chunkID) + " is truncated");
                if (!seenChunks.insert(chunkID).second)
                    throw SerializationError("Duplicate chunk " + ChunkName(chunkID));

                InputArchive chunk = ar.SubArchive(static_cast<std::size_t>(chunkSize));
                switch (chunkID)
                {
                case CHUNK_META:
                    chunk(snapshot.Metadata);
                    break;
                case CHUNK_ENTITIES:
                    ParseEntities(chunk, snapshot);
                    break;
                case CHUNK_COMPONENTS:
                    if (seenChunks.count(CHUNK_ENTITIES) == 0)
                        throw SerializationError("Component chunk found before entity chunk");
                    ParseComponents(chunk, snapshot);
                    break;
                case CHUNK_RESOURCES:
                    ParseResources(chunk, snapshot);
                    break;
                case CHUNK_END:
                    reachedEnd = true;
                    break;
                default:
                    // Written by a newer version of the game, ignore it
                    snapshot.Warnings.push_back("Skipped unknown chunk " + ChunkName(chunkID));
                    chunk.Skip(chunk.Remaining());
                    break;
                }
                if (!chunk.AtEnd())
                    throw SerializationError("Chunk " + ChunkName(chunkID) + " has trailing data");
            }

            if (!ar.AtEnd())
                throw SerializationError("Unexpected data after end chunk");
            if (seenChunks.count(CHUNK_ENTITIES) == 0)
                throw SerializationError("Save file has no entity chunk");
        }
        catch (const std::exception& e)
        {
            snapshot = WorldSnapshot{};
            result.Success = false;
            result.Error = e.what();
            return result;
        }

        result.Success = true;
        result.Metadata = snapshot.Metadata;
        result.Warnings = snapshot.Warnings;
        return result;
    }

    void WorldSerializer::ParseEntities(InputArchive& ar, WorldSnapshot& snapshot) const
    {
        std::uint32_t maxEntities = 0;
        ar(maxEntities, snapshot.LivingEntities, snapshot.AvailableEntities);
        if (maxEntities != MAX_ENTITIES)
        {
            throw SerializationError("Save was made with MAX_ENTITIES = " +
                                     std::to_string(maxEntities) + ", this build uses " +
                                     std::to_string(MAX_ENTITIES));
        }
        std::string error;
        if (!EntityManager::ValidateState(
                    snapshot.LivingEntities, snapshot.AvailableEntities, error))
        {
            throw SerializationError(error);
        }
    }

    void WorldSerializer::ParseComponents(InputArchive& ar, WorldSnapshot& snapshot) const
    {
        std::vector<bool> alive(MAX_ENTITIES, false);
        for (Entity e : snapshot.LivingEntities)
            alive[e] = true;

        std::unordered_set<std::string> seenTypes;
        std::size_t typeCount = ar.ReadSize();
        for (std::size_t t = 0; t < typeCount; ++t)
        {
            std::string name;
            std::uint32_t version = 0;
            ar(name, version);
            if (!seenTypes.insert(name).second)
                throw SerializationError("Component type listed twice: " + name);
            if (version == 0)
                throw SerializationError("Component " + name + " has invalid version 0");

            const ComponentSerializer* serializer = m_Registry.FindComponent(name);
            if (serializer != nullptr && version > serializer->Version)
            {
                throw SerializationError("Component " + name + " was saved with version " +
                                         std::to_string(version) + " but this build only knows " +
                                         std::to_string(serializer->Version));
            }

            std::vector<bool> seenOwner(MAX_ENTITIES, false);
            std::size_t count = ar.ReadSize();
            for (std::size_t i = 0; i < count; ++i)
            {
                Entity e = NULL_ENTITY;
                std::uint32_t size = 0;
                ar.ReadPrimitive(e);
                ar.ReadPrimitive(size);
                if (e >= MAX_ENTITIES || !alive[e])
                {
                    throw SerializationError("Component " + name + " attached to dead entity " +
                                             std::to_string(e));
                }
                if (seenOwner[e])
                {
                    throw SerializationError("Component " + name +
                                             " attached twice to entity " + std::to_string(e));
                }
                seenOwner[e] = true;

                InputArchive record = ar.SubArchive(size);
                if (serializer == nullptr)
                    continue;
                record.SetVersion(version);
                snapshot.ComponentActions.push_back(serializer->Load(e, record));
                if (!record.AtEnd())
                {
                    throw SerializationError("Component " + name + " on entity " +
                                             std::to_string(e) +
                                             " did not consume its data (layout mismatch)");
                }
            }

            if (serializer == nullptr)
            {
                snapshot.Warnings.push_back("Skipped unknown component type '" + name + "' (" +
                                            std::to_string(count) + " instances)");
            }
        }
    }

    void WorldSerializer::ParseResources(InputArchive& ar, WorldSnapshot& snapshot) const
    {
        std::unordered_set<std::string> seen;
        std::size_t count = ar.ReadSize();
        for (std::size_t i = 0; i < count; ++i)
        {
            std::string name;
            std::uint32_t version = 0;
            std::uint32_t size = 0;
            ar(name, version);
            ar.ReadPrimitive(size);
            if (!seen.insert(name).second)
                throw SerializationError("Resource listed twice: " + name);
            if (version == 0)
                throw SerializationError("Resource " + name + " has invalid version 0");

            InputArchive record = ar.SubArchive(size);
            const ResourceSerializer* serializer = m_Registry.FindResource(name);
            if (serializer == nullptr)
            {
                snapshot.Warnings.push_back("Skipped unknown resource type '" + name + "'");
                continue;
            }
            if (version > serializer->Version)
            {
                throw SerializationError("Resource " + name + " was saved with version " +
                                         std::to_string(version) + " but this build only knows " +
                                         std::to_string(serializer->Version));
            }

            // Dry run to make sure Apply can not fail later on
            InputArchive validation(record.Data(), size);
            validation.SetVersion(version);
            serializer->Validate(validation);
            if (!validation.AtEnd())
            {
                throw SerializationError("Resource " + name +
                                         " did not consume its data (layout mismatch)");
            }

            WorldSnapshot::StagedResource staged;
            staged.Name = name;
            staged.Version = version;
            staged.Bytes.assign(record.Data(), record.Data() + size);
            snapshot.Resources.push_back(std::move(staged));
        }
    }

    std::vector<std::string> WorldSerializer::Apply(ECSManager& ecs,
                                                    const WorldSnapshot& snapshot) const
    {
        std::vector<std::string> warnings;

        ecs.RestoreEntities(snapshot.LivingEntities, snapshot.AvailableEntities);

        for (const StagedAction& action : snapshot.ComponentActions)
            action(ecs);

        for (const auto& staged : snapshot.Resources)
        {
            const ResourceSerializer* serializer = m_Registry.FindResource(staged.Name);
            if (serializer == nullptr || !serializer->Has(ecs))
            {
                warnings.push_back("Resource '" + staged.Name +
                                   "' is not registered in the ECS, skipped");
                continue;
            }
            InputArchive ar(staged.Bytes);
            ar.SetVersion(staged.Version);
            serializer->Apply(ecs, ar);
        }

        for (const auto& callback : m_Registry.PostLoadCallbacks())
            callback(ecs);

        return warnings;
    }

    LoadResult WorldSerializer::Load(ECSManager& ecs, const std::vector<std::uint8_t>& bytes) const
    {
        WorldSnapshot snapshot;
        LoadResult result = Parse(bytes, snapshot);
        if (!result)
            return result;
        std::vector<std::string> warnings = Apply(ecs, snapshot);
        result.Warnings.insert(result.Warnings.end(), warnings.begin(), warnings.end());
        return result;
    }

    //-----------------------------------------------------------------------------
    // Plain text
    //-----------------------------------------------------------------------------

    void WorldSerializer::SetFileFormat(SaveFormat format)
    {
        g_FileFormat = format;
    }

    SaveFormat WorldSerializer::FileFormat()
    {
        return g_FileFormat;
    }

    bool WorldSerializer::IsTextSave(const std::vector<std::uint8_t>& bytes)
    {
        std::size_t length = std::strlen(TEXT_MAGIC);
        return bytes.size() >= length && std::memcmp(bytes.data(), TEXT_MAGIC, length) == 0;
    }

    std::vector<std::uint8_t> WorldSerializer::Save(ECSManager& ecs,
                                                    const SaveMetadata& metadata,
                                                    SaveFormat format) const
    {
        if (format == SaveFormat::Binary)
            return Save(ecs, metadata);
        std::string text = SaveText(ecs, metadata);
        return std::vector<std::uint8_t>(text.begin(), text.end());
    }

    std::string WorldSerializer::SaveText(ECSManager& ecs, const SaveMetadata& metadata) const
    {
        // Same content and order as the binary Save, one record per line
        TextOutputArchive ar;
        ar.Raw(std::string(TEXT_MAGIC) + " " + std::to_string(TEXT_FORMAT_VERSION) + "\n");
        ar.Raw("# Plain text save. Each record lists its values in the order of the type's\n"
               "# Serialize function, [n] = number of elements that follow. No checksum:\n"
               "# the file may be edited by hand, it is fully validated when loaded.\n");

        ar.Raw("meta");
        SaveMetadata copy = metadata;
        ar(copy);
        ar.NewLine();

        std::vector<Entity> living = ecs.GetLivingEntities();
        std::vector<Entity> available = ecs.GetAvailableEntities();
        std::uint32_t maxEntities = MAX_ENTITIES;
        ar.Raw("entities");
        ar(maxEntities);
        WriteEntityList(ar, living);
        WriteEntityList(ar, available);
        ar.NewLine();

        for (const ComponentSerializer& serializer : m_Registry.Components())
        {
            std::vector<Entity> owners = Owners(ecs, serializer, living);
            if (owners.empty())
                continue;
            ar.Raw("component");
            std::string name = serializer.Name;
            std::uint32_t version = serializer.Version;
            ar(name, version);
            ar.WriteSize(owners.size());
            ar.NewLine();
            ar.SetVersion(version);
            for (Entity e : owners)
            {
                ar.Raw(std::to_string(e) + ":");
                serializer.SaveText(ecs, e, ar);
                ar.NewLine();
            }
        }

        for (const ResourceSerializer& serializer : m_Registry.Resources())
        {
            if (!serializer.Has(ecs))
                continue;
            ar.Raw("resource");
            std::string name = serializer.Name;
            std::uint32_t version = serializer.Version;
            ar(name, version);
            ar.NewLine();
            ar.SetVersion(version);
            ar.Raw(":");
            serializer.SaveText(ecs, ar);
            ar.NewLine();
        }
        ar.Raw("end\n");
        return ar.Text();
    }

    std::vector<std::uint8_t> WorldSerializer::TextToBinary(const std::string& text,
                                                            std::vector<std::string>& warnings) const
    {
        // -- Split into significant lines (blank lines and # comments skipped) --
        struct Line
        {
            std::size_t Number;
            std::string Text;
        };
        std::vector<Line> lines;
        {
            std::istringstream stream(text);
            std::string line;
            std::size_t number = 0;
            while (std::getline(stream, line))
            {
                ++number;
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                std::size_t first = line.find_first_not_of(" \t");
                if (first == std::string::npos || line[first] == '#')
                    continue;
                lines.push_back({number, line.substr(first)});
            }
        }
        std::size_t next = 0;
        auto fail = [](std::size_t number, const std::string& message) -> SerializationError {
            return SerializationError("line " + std::to_string(number) + ": " + message);
        };

        // -- Header ----------------------------------------------------------------
        if (lines.empty())
            throw SerializationError("empty text save");
        {
            TextInputArchive header(lines[0].Text, lines[0].Number);
            std::uint32_t version = 0;
            if (header.NextToken("header") != TEXT_MAGIC)
                throw fail(lines[0].Number, std::string("expected ") + TEXT_MAGIC);
            header.ReadPrimitive(version);
            if (version == 0 || version > TEXT_FORMAT_VERSION)
                throw fail(lines[0].Number, "unsupported text format version " + std::to_string(version));
            next = 1;
        }

        struct Record
        {
            Entity Owner = NULL_ENTITY;
            std::vector<std::uint8_t> Bytes;
        };
        struct Type
        {
            std::string Name;
            std::uint32_t Version = 1;
            std::vector<Record> Records;
        };
        SaveMetadata metadata;
        std::uint32_t maxEntities = 0;
        std::vector<Entity> living, available;
        bool hasMeta = false, hasEntities = false, ended = false;
        std::vector<Type> components;
        std::vector<Type> resources;

        auto finishRecord = [&](TextInputArchive& in, std::size_t number, const std::string& what) {
            if (!in.AtEnd())
                throw fail(number, what + " has more values than its type reads (layout mismatch): '" +
                                           in.Rest().substr(0, 40) + "'");
        };

        while (next < lines.size() && !ended)
        {
            const Line& line = lines[next++];
            TextInputArchive in(line.Text, line.Number);
            std::string keyword = in.NextToken("a keyword");
            if (keyword == "meta")
            {
                if (hasMeta)
                    throw fail(line.Number, "meta listed twice");
                hasMeta = true;
                in(metadata);
                finishRecord(in, line.Number, "meta");
            }
            else if (keyword == "entities")
            {
                if (hasEntities)
                    throw fail(line.Number, "entities listed twice");
                hasEntities = true;
                in(maxEntities);
                living = ReadEntityList(in);
                available = ReadEntityList(in);
                finishRecord(in, line.Number, "entities");
            }
            else if (keyword == "component")
            {
                Type type;
                in(type.Name, type.Version);
                // The count is the number of record lines that follow
                std::string countToken = in.NextToken("[count]");
                if (countToken.size() < 3 || countToken.front() != '[' || countToken.back() != ']')
                    throw fail(line.Number, "expected a [count] of records, got '" + countToken + "'");
                char* countEnd = nullptr;
                std::size_t count = std::strtoul(countToken.c_str() + 1, &countEnd, 10);
                if (countEnd != countToken.c_str() + countToken.size() - 1)
                    throw fail(line.Number, "bad record count '" + countToken + "'");
                finishRecord(in, line.Number, "component header");
                const ComponentSerializer* serializer = m_Registry.FindComponent(type.Name);
                if (serializer != nullptr && type.Version > serializer->Version)
                {
                    throw fail(line.Number, "Component " + type.Name + " was saved with version " +
                                                    std::to_string(type.Version) + " but this build only knows " +
                                                    std::to_string(serializer->Version));
                }
                if (count > lines.size() - next)
                    throw fail(line.Number, "component " + type.Name + " announces " + std::to_string(count) +
                                                    " records, the file ends before");
                for (std::size_t i = 0; i < count; ++i)
                {
                    const Line& record = lines[next++];
                    std::size_t colon = record.Text.find(':');
                    if (colon == std::string::npos)
                        throw fail(record.Number, "expected '<entity>: values' for component " + type.Name);
                    TextInputArchive owner(record.Text.substr(0, colon), record.Number);
                    Record parsed;
                    owner.ReadPrimitive(parsed.Owner);
                    finishRecord(owner, record.Number, "entity id");
                    if (serializer == nullptr)
                        continue;
                    TextInputArchive values(record.Text.substr(colon + 1), record.Number);
                    values.SetVersion(type.Version);
                    OutputArchive bytes;
                    bytes.SetVersion(type.Version);
                    serializer->TextToBinary(values, bytes);
                    finishRecord(values, record.Number,
                                 "component " + type.Name + " on entity " + std::to_string(parsed.Owner));
                    parsed.Bytes = bytes.TakeBuffer();
                    type.Records.push_back(std::move(parsed));
                }
                if (serializer == nullptr)
                {
                    warnings.push_back("Skipped unknown component type '" + type.Name + "' (" +
                                       std::to_string(count) + " instances)");
                    continue;
                }
                components.push_back(std::move(type));
            }
            else if (keyword == "resource")
            {
                Type type;
                in(type.Name, type.Version);
                finishRecord(in, line.Number, "resource header");
                if (next >= lines.size() || lines[next].Text.empty() || lines[next].Text[0] != ':')
                    throw fail(line.Number, "resource " + type.Name + " must be followed by a ': values' line");
                const Line& record = lines[next++];
                const ResourceSerializer* serializer = m_Registry.FindResource(type.Name);
                if (serializer == nullptr)
                {
                    warnings.push_back("Skipped unknown resource type '" + type.Name + "'");
                    continue;
                }
                if (type.Version > serializer->Version)
                {
                    throw fail(line.Number, "Resource " + type.Name + " was saved with version " +
                                                    std::to_string(type.Version) + " but this build only knows " +
                                                    std::to_string(serializer->Version));
                }
                TextInputArchive values(record.Text.substr(1), record.Number);
                values.SetVersion(type.Version);
                OutputArchive bytes;
                bytes.SetVersion(type.Version);
                serializer->TextToBinary(values, bytes);
                finishRecord(values, record.Number, "resource " + type.Name);
                Record parsed;
                parsed.Bytes = bytes.TakeBuffer();
                type.Records.push_back(std::move(parsed));
                resources.push_back(std::move(type));
            }
            else if (keyword == "end")
            {
                finishRecord(in, line.Number, "end");
                ended = true;
            }
            else
                throw fail(line.Number, "unknown keyword '" + keyword + "'");
        }
        if (!ended)
            throw SerializationError("the text save has no 'end' line (truncated?)");
        if (next < lines.size())
            throw fail(lines[next].Number, "unexpected text after 'end'");
        if (!hasEntities)
            throw SerializationError("the text save has no 'entities' line");

        // -- Binary save with the same content (exactly what Save would write) --
        OutputArchive ar;
        ar.WriteBytes(MAGIC, sizeof(MAGIC));
        ar.WritePrimitive(FORMAT_VERSION);
        ar.WritePrimitive(std::uint32_t{0});
        {
            ChunkWriter chunk(ar, CHUNK_META);
            ar(metadata);
        }
        {
            ChunkWriter chunk(ar, CHUNK_ENTITIES);
            ar(maxEntities, living, available);
        }
        {
            ChunkWriter chunk(ar, CHUNK_COMPONENTS);
            ar.WriteSize(components.size());
            for (Type& type : components)
            {
                ar(type.Name, type.Version);
                ar.WriteSize(type.Records.size());
                for (const Record& record : type.Records)
                {
                    ar.WritePrimitive(record.Owner);
                    SizedRecordWriter sized(ar);
                    ar.WriteBytes(record.Bytes.data(), record.Bytes.size());
                }
            }
        }
        {
            ChunkWriter chunk(ar, CHUNK_RESOURCES);
            ar.WriteSize(resources.size());
            for (Type& type : resources)
            {
                ar(type.Name, type.Version);
                SizedRecordWriter sized(ar);
                ar.WriteBytes(type.Records[0].Bytes.data(), type.Records[0].Bytes.size());
            }
        }
        {
            ChunkWriter chunk(ar, CHUNK_END);
        }
        std::uint32_t crc = Crc32(ar.Buffer().data(), ar.Size());
        ar.WritePrimitive(crc);
        return ar.TakeBuffer();
    }

    //-----------------------------------------------------------------------------
    // Files
    //-----------------------------------------------------------------------------

    SaveResult WorldSerializer::SaveToFile(ECSManager& ecs,
                                           const std::string& path,
                                           const SaveMetadata& metadata) const
    {
        return SaveToFile(ecs, path, metadata, g_FileFormat);
    }

    SaveResult WorldSerializer::SaveToFile(ECSManager& ecs,
                                           const std::string& path,
                                           const SaveMetadata& metadata,
                                           SaveFormat format) const
    {
        SaveResult result;
        try
        {
            std::vector<std::uint8_t> bytes = Save(ecs, metadata, format);
            if (!WriteFile(path, bytes, result.Error))
                return result;
            result.BytesWritten = bytes.size();
            result.Success = true;
        }
        catch (const std::exception& e)
        {
            result.Error = e.what();
        }
        return result;
    }

    LoadResult WorldSerializer::LoadFromFile(ECSManager& ecs, const std::string& path) const
    {
        std::vector<std::uint8_t> bytes;
        LoadResult result;
        if (!ReadFile(path, bytes, result.Error))
            return result;
        return Load(ecs, bytes);
    }

    bool WorldSerializer::WriteFile(const std::string& path,
                                    const std::vector<std::uint8_t>& bytes,
                                    std::string& error)
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path target(path);
        if (target.has_parent_path())
        {
            fs::create_directories(target.parent_path(), ec);
            if (ec)
            {
                error = "Could not create directory " + target.parent_path().string() + ": " +
                        ec.message();
                return false;
            }
        }

        // Write to a temporary file first so a crash mid-write never destroys
        // the previous save
        fs::path temp = target;
        temp += ".tmp";
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                error = "Could not open " + temp.string() + " for writing";
                return false;
            }
            out.write(reinterpret_cast<const char*>(bytes.data()),
                      static_cast<std::streamsize>(bytes.size()));
            out.flush();
            if (!out)
            {
                error = "Failed while writing " + temp.string();
                return false;
            }
        }

        fs::rename(temp, target, ec);
        if (ec)
        {
            fs::remove(temp, ec);
            error = "Could not replace " + target.string();
            return false;
        }
        return true;
    }

    bool WorldSerializer::ReadFile(const std::string& path,
                                   std::vector<std::uint8_t>& bytes,
                                   std::string& error)
    {
        std::ifstream in(std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!in)
        {
            error = "Could not open save file " + path;
            return false;
        }
        std::streamoff size = in.tellg();
        if (size < 0)
        {
            error = "Could not read size of " + path;
            return false;
        }
        bytes.resize(static_cast<std::size_t>(size));
        in.seekg(0, std::ios::beg);
        if (size > 0 && !in.read(reinterpret_cast<char*>(bytes.data()), size))
        {
            error = "Failed while reading " + path;
            return false;
        }
        return true;
    }
} // namespace Serialization
