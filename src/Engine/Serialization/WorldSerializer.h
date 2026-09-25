//---------------------------------------------------------------------------------
// WorldSerializer.h
//---------------------------------------------------------------------------------
//
// Saves and restores the complete state of the ECS world: every living Entity,
// every registered Component attached to them, the Entity allocator state and
// every registered Resource.
//
// File layout (all integers little-endian):
//
//   Header   : "UBSV" magic | u32 format version | u32 flags
//   Chunks   : u32 chunk id | u64 payload size | payload       (repeated)
//                META - key/value metadata (scene name, ...)
//                ENTS - living entity ids + free entity queue
//                COMP - components grouped by type name
//                RSRC - resources by type name
//                END  - terminator (empty payload)
//   Footer   : u32 CRC-32 of every preceding byte
//
// Every component / resource record carries its own byte size, so a loader can
// skip types it does not know about (a save made by a newer build) and can
// detect a Serialize function that reads the wrong amount of data.
//
// Plain text files (SaveFormat::Text) hold the same data readably, one record
// per line, values in the order the type's Serialize function writes them:
//
//   UBSV-TEXT 1
//   meta [2] "Name" "level_1" "Scene" "Play"
//   entities 5000 [3] 0 1 2 [0]
//   component "SceneObject" 1 [3]
//   0: "Field" "Field"
//   ...
//   resource "SceneSettings" 1
//   : "level_1" "CollectGame" [2] "Level" 1 "Lives" 3 24 18 0 0 -1 24
//   end
//
// Text files have no checksum, so they can be read and edited by hand. Loading
// turns a text file back into the binary form and parses that, so both
// formats get exactly the same validation. Load / Parse / LoadFromFile
// recognise the format by its first bytes.
//
// Loading happens in two phases:
//   1. Parse  - validates the whole file and stages every change. The ECS is
//               not touched, a corrupt file can never leave a half loaded world
//   2. Apply  - clears the world, restores entities with their exact ids,
//               adds components, fills resources, runs post load callbacks
//
#pragma once

#include "SerializationRegistry.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Serialization
{
    using SaveMetadata = std::map<std::string, std::string>;

    enum class SaveFormat
    {
        Binary, // compact, checksummed (default)
        Text    // plain text, readable / diffable / hand editable
    };

    /**
     * \brief Result of parsing/loading a save
     */
    struct LoadResult
    {
        bool Success = false;
        std::string Error;
        std::vector<std::string> Warnings;
        SaveMetadata Metadata;

        explicit operator bool() const { return Success; }
    };

    /**
     * \brief Result of writing a save to disk
     */
    struct SaveResult
    {
        bool Success = false;
        std::string Error;
        std::size_t BytesWritten = 0;

        explicit operator bool() const { return Success; }
    };

    /**
     * \brief Fully validated save file content ready to be applied to an ECS
     */
    struct WorldSnapshot
    {
        struct StagedResource
        {
            std::string Name;
            std::uint32_t Version = 1;
            std::vector<std::uint8_t> Bytes;
        };

        SaveMetadata Metadata;
        std::vector<Entity> LivingEntities;
        std::vector<Entity> AvailableEntities;
        std::vector<StagedAction> ComponentActions;
        std::vector<StagedResource> Resources;
        std::vector<std::string> Warnings;
    };

    class WorldSerializer
    {
      public:
        static constexpr std::uint32_t FORMAT_VERSION = 1;

        explicit WorldSerializer(const SerializationRegistry& registry)
            : m_Registry(registry)
        {
        }

        /**
         * \brief Serialize the whole world into a byte buffer
         * \throws SerializationError if a value can not be serialized
         */
        std::vector<std::uint8_t> Save(ECSManager& ecs, const SaveMetadata& metadata = {}) const;

        /**
         * \brief Serialize the whole world in the given format (the bytes of a
         *        text save are its UTF-8 text)
         */
        std::vector<std::uint8_t> Save(ECSManager& ecs, const SaveMetadata& metadata, SaveFormat format) const;

        /**
         * \brief Plain text version of the world (see the file comment)
         */
        std::string SaveText(ECSManager& ecs, const SaveMetadata& metadata = {}) const;

        /**
         * \brief Convert a text save into the equivalent binary save
         * \throws SerializationError on malformed text
         */
        std::vector<std::uint8_t> TextToBinary(const std::string& text, std::vector<std::string>& warnings) const;

        static bool IsTextSave(const std::vector<std::uint8_t>& bytes);

        /**
         * \brief Format used by SaveToFile when none is given (a program wide
         *        toggle: the editor's "Plain text files" option, the game's saves)
         */
        static void SetFileFormat(SaveFormat format);
        static SaveFormat FileFormat();

        /**
         * \brief Validate a save and stage its content without touching any ECS
         */
        LoadResult Parse(const std::vector<std::uint8_t>& bytes, WorldSnapshot& snapshot) const;

        /**
         * \brief Replace the world held by the ECS with the snapshot content
         * \return warnings produced while applying (e.g. unregistered resource)
         */
        std::vector<std::string> Apply(ECSManager& ecs, const WorldSnapshot& snapshot) const;

        /**
         * \brief Parse + Apply. On failure the ECS is left untouched
         */
        LoadResult Load(ECSManager& ecs, const std::vector<std::uint8_t>& bytes) const;

        /**
         * \brief Save the world into a file (written atomically through a temp file)
         */
        SaveResult SaveToFile(ECSManager& ecs,
                              const std::string& path,
                              const SaveMetadata& metadata = {}) const;
        SaveResult SaveToFile(ECSManager& ecs,
                              const std::string& path,
                              const SaveMetadata& metadata,
                              SaveFormat format) const;

        /**
         * \brief Load the world from a file. On failure the ECS is left untouched
         */
        LoadResult LoadFromFile(ECSManager& ecs, const std::string& path) const;

        static bool WriteFile(const std::string& path,
                              const std::vector<std::uint8_t>& bytes,
                              std::string& error);

        static bool ReadFile(const std::string& path,
                             std::vector<std::uint8_t>& bytes,
                             std::string& error);

      private:
        LoadResult ParseBinary(const std::vector<std::uint8_t>& bytes, WorldSnapshot& snapshot) const;
        void ParseEntities(InputArchive& ar, WorldSnapshot& snapshot) const;
        void ParseComponents(InputArchive& ar, WorldSnapshot& snapshot) const;
        void ParseResources(InputArchive& ar, WorldSnapshot& snapshot) const;

        const SerializationRegistry& m_Registry;
    };
} // namespace Serialization
