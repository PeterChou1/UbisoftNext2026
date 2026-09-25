//---------------------------------------------------------------------------------
// Archive.h
//---------------------------------------------------------------------------------
//
// Binary archives used by the serialization system.
//
// Every serializable type provides a single free function
//
//     template <typename Archive>
//     void Serialize(Archive& ar, MyType& value) { ar(value.A, value.B); }
//
// which is used both for saving (OutputArchive) and loading (InputArchive).
// Writing the save and load path as one function makes it impossible for the
// two to drift out of sync. The function must live in the same namespace as
// the type (the global namespace for all engine/game types) so that it is found
// through argument dependent lookup.
//
// Encoding rules (portable across Windows / MacOS builds):
//   - All integers are written little-endian using sizeof(T) bytes
//   - Floating point values are written as their IEEE-754 bit pattern
//   - bool is written as a single byte (0 or 1)
//   - enums are written as a signed 32 bit integer
//   - containers are written as a u32 element count followed by the elements
//
// InputArchive never reads past the end of its buffer, every violation throws
// a SerializationError which the WorldSerializer converts into a failed load.
//
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

static_assert(std::numeric_limits<float>::is_iec559, "Serialization requires IEEE-754 floats");
static_assert(std::numeric_limits<double>::is_iec559, "Serialization requires IEEE-754 doubles");

namespace Serialization
{
    /**
     * \brief Thrown whenever data can not be serialized or deserialized
     */
    class SerializationError : public std::runtime_error
    {
      public:
        using std::runtime_error::runtime_error;
    };

    /**
     * \brief Valid values of an enum stored in save files. Every serialized
     *        enum must declare one (a compile error says so otherwise):
     *
     *            SERIALIZATION_ENUM_RANGE(ShapeType, CircleShape, PolygonShape)
     *
     *        Loading refuses values outside [Min, Max]: turning them into the
     *        enum would be undefined behaviour, and hand edited text saves or
     *        damaged files can contain anything
     */
    template <typename T>
    struct EnumRange
    {
        static constexpr bool Defined = false;
    };

    template <typename T>
    void CheckEnumValue(std::int32_t raw)
    {
        static_assert(EnumRange<T>::Defined,
                      "Declare the valid range of this enum with SERIALIZATION_ENUM_RANGE(Type, First, Last)");
        if (raw < EnumRange<T>::Min || raw > EnumRange<T>::Max)
        {
            throw SerializationError("Enum value " + std::to_string(raw) + " out of range [" +
                                     std::to_string(EnumRange<T>::Min) + ", " +
                                     std::to_string(EnumRange<T>::Max) + "]");
        }
    }

    // Forward declarations so the archives can dispatch to the overloads below
    template <typename Archive, typename T>
    void Dispatch(Archive& ar, T& value);

    /**
     * \brief Archive that writes values into a growable byte buffer
     */
    class OutputArchive
    {
      public:
        static constexpr bool IsLoading = false;
        static constexpr bool IsSaving = true;

        /**
         * \brief Serialize every argument in order
         */
        template <typename... Ts>
        OutputArchive& operator()(Ts&... values)
        {
            (Dispatch(*this, values), ...);
            return *this;
        }

        /**
         * \brief Version of the type currently being serialized.
         *        Serialize functions use this to support older save files
         */
        std::uint32_t Version() const { return m_Version; }

        void SetVersion(std::uint32_t version) { m_Version = version; }

        template <typename T>
        void WritePrimitive(T value)
        {
            if constexpr (std::is_same_v<T, bool>)
            {
                WriteUnsigned<std::uint8_t>(value ? 1u : 0u);
            }
            else if constexpr (std::is_enum_v<T>)
            {
                WriteUnsigned<std::uint32_t>(
                        static_cast<std::uint32_t>(static_cast<std::int32_t>(value)));
            }
            else if constexpr (std::is_floating_point_v<T>)
            {
                using Bits = std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;
                static_assert(sizeof(T) == sizeof(Bits), "Unsupported floating point size");
                Bits bits;
                std::memcpy(&bits, &value, sizeof(T));
                WriteUnsigned<Bits>(bits);
            }
            else
            {
                static_assert(std::is_integral_v<T>, "WritePrimitive requires a primitive type");
                using U = std::make_unsigned_t<T>;
                WriteUnsigned<U>(static_cast<U>(value));
            }
        }

        void WriteBytes(const void* data, std::size_t size)
        {
            const auto* bytes = static_cast<const std::uint8_t*>(data);
            m_Buffer.insert(m_Buffer.end(), bytes, bytes + size);
        }

        /**
         * \brief Write a container length, containers are limited to 2^32 - 1 elements
         */
        void WriteSize(std::size_t size)
        {
            if (size > std::numeric_limits<std::uint32_t>::max())
                throw SerializationError("Container too large to serialize");
            WriteUnsigned<std::uint32_t>(static_cast<std::uint32_t>(size));
        }

        /**
         * \brief Overwrite a previously written u32 (used to back-patch chunk sizes)
         */
        void PatchU32(std::size_t offset, std::uint32_t value)
        {
            if (offset + 4 > m_Buffer.size())
                throw SerializationError("PatchU32 out of range");
            for (std::size_t i = 0; i < 4; ++i)
                m_Buffer[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
        }

        /**
         * \brief Overwrite a previously written u64 (used to back-patch chunk sizes)
         */
        void PatchU64(std::size_t offset, std::uint64_t value)
        {
            if (offset + 8 > m_Buffer.size())
                throw SerializationError("PatchU64 out of range");
            for (std::size_t i = 0; i < 8; ++i)
                m_Buffer[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
        }

        std::size_t Size() const { return m_Buffer.size(); }

        const std::vector<std::uint8_t>& Buffer() const { return m_Buffer; }

        std::vector<std::uint8_t> TakeBuffer() { return std::move(m_Buffer); }

      private:
        template <typename U>
        void WriteUnsigned(U value)
        {
            for (std::size_t i = 0; i < sizeof(U); ++i)
                m_Buffer.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
        }

        std::vector<std::uint8_t> m_Buffer;
        std::uint32_t m_Version = 1;
    };

    /**
     * \brief Archive that reads values from a (non owning) byte range
     */
    class InputArchive
    {
      public:
        static constexpr bool IsLoading = true;
        static constexpr bool IsSaving = false;

        InputArchive(const std::uint8_t* data, std::size_t size)
            : m_Data(data)
            , m_Size(size)
        {
        }

        explicit InputArchive(const std::vector<std::uint8_t>& buffer)
            : InputArchive(buffer.data(), buffer.size())
        {
        }

        /**
         * \brief Deserialize every argument in order
         */
        template <typename... Ts>
        InputArchive& operator()(Ts&... values)
        {
            (Dispatch(*this, values), ...);
            return *this;
        }

        /**
         * \brief Version of the type that was written to the file
         */
        std::uint32_t Version() const { return m_Version; }

        void SetVersion(std::uint32_t version) { m_Version = version; }

        template <typename T>
        void ReadPrimitive(T& value)
        {
            if constexpr (std::is_same_v<T, bool>)
            {
                std::uint8_t byte = ReadUnsigned<std::uint8_t>();
                if (byte > 1)
                    throw SerializationError("Invalid boolean value in archive");
                value = byte == 1;
            }
            else if constexpr (std::is_enum_v<T>)
            {
                auto raw = static_cast<std::int32_t>(ReadUnsigned<std::uint32_t>());
                CheckEnumValue<T>(raw);
                value = static_cast<T>(raw);
            }
            else if constexpr (std::is_floating_point_v<T>)
            {
                using Bits = std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;
                Bits bits = ReadUnsigned<Bits>();
                std::memcpy(&value, &bits, sizeof(T));
            }
            else
            {
                static_assert(std::is_integral_v<T>, "ReadPrimitive requires a primitive type");
                using U = std::make_unsigned_t<T>;
                value = static_cast<T>(ReadUnsigned<U>());
            }
        }

        void ReadBytes(void* out, std::size_t size)
        {
            Require(size);
            std::memcpy(out, m_Data + m_Position, size);
            m_Position += size;
        }

        /**
         * \brief Read a container length and verify it can possibly fit in the
         *        remaining data. Every serialized element occupies at least
         *        one byte, which prevents huge allocations on corrupt data
         */
        std::size_t ReadSize()
        {
            std::size_t size = ReadUnsigned<std::uint32_t>();
            if (size > Remaining())
                throw SerializationError("Container length exceeds remaining data");
            return size;
        }

        /**
         * \brief Create an archive over the next `size` bytes and advance past them
         */
        InputArchive SubArchive(std::size_t size)
        {
            Require(size);
            InputArchive sub(m_Data + m_Position, size);
            sub.m_Version = m_Version;
            m_Position += size;
            return sub;
        }

        void Skip(std::size_t size)
        {
            Require(size);
            m_Position += size;
        }

        std::size_t Position() const { return m_Position; }

        std::size_t Remaining() const { return m_Size - m_Position; }

        bool AtEnd() const { return m_Position == m_Size; }

        const std::uint8_t* Data() const { return m_Data; }

      private:
        void Require(std::size_t size) const
        {
            if (size > Remaining())
                throw SerializationError("Unexpected end of archive");
        }

        template <typename U>
        U ReadUnsigned()
        {
            Require(sizeof(U));
            U value = 0;
            for (std::size_t i = 0; i < sizeof(U); ++i)
                value |= static_cast<U>(static_cast<U>(m_Data[m_Position + i]) << (8 * i));
            m_Position += sizeof(U);
            return value;
        }

        const std::uint8_t* m_Data = nullptr;
        std::size_t m_Size = 0;
        std::size_t m_Position = 0;
        std::uint32_t m_Version = 1;
    };

    //-----------------------------------------------------------------------------
    // Serialize overloads for primitives and standard library types
    //-----------------------------------------------------------------------------

    template <typename Archive, typename T>
    std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>> Serialize(Archive& ar, T& value)
    {
        if constexpr (Archive::IsSaving)
            ar.WritePrimitive(value);
        else
            ar.ReadPrimitive(value);
    }

    template <typename Archive>
    void Serialize(Archive& ar, std::string& value)
    {
        if constexpr (Archive::IsSaving)
        {
            ar.WriteSize(value.size());
            ar.WriteBytes(value.data(), value.size());
        }
        else
        {
            std::size_t size = ar.ReadSize();
            value.resize(size);
            if (size > 0)
                ar.ReadBytes(&value[0], size);
        }
    }

    template <typename Archive, typename A, typename B>
    void Serialize(Archive& ar, std::pair<A, B>& value)
    {
        Dispatch(ar, value.first);
        Dispatch(ar, value.second);
    }

    template <typename Archive, typename T, std::size_t N>
    void Serialize(Archive& ar, std::array<T, N>& value)
    {
        for (T& element : value)
            Dispatch(ar, element);
    }

    template <typename Archive, typename T, typename Alloc>
    void Serialize(Archive& ar, std::vector<T, Alloc>& value)
    {
        if constexpr (Archive::IsSaving)
        {
            ar.WriteSize(value.size());
            if constexpr (std::is_same_v<T, bool>)
            {
                for (bool element : value)
                    ar.WritePrimitive(element);
            }
            else
            {
                for (T& element : value)
                    Dispatch(ar, element);
            }
        }
        else
        {
            std::size_t size = ar.ReadSize();
            value.clear();
            value.resize(size);
            if constexpr (std::is_same_v<T, bool>)
            {
                for (std::size_t i = 0; i < size; ++i)
                {
                    bool element = false;
                    ar.ReadPrimitive(element);
                    value[i] = element;
                }
            }
            else
            {
                for (T& element : value)
                    Dispatch(ar, element);
            }
        }
    }

    template <typename Archive, typename T, typename Compare, typename Alloc>
    void Serialize(Archive& ar, std::set<T, Compare, Alloc>& value)
    {
        if constexpr (Archive::IsSaving)
        {
            ar.WriteSize(value.size());
            for (const T& element : value)
            {
                T copy = element;
                Dispatch(ar, copy);
            }
        }
        else
        {
            std::size_t size = ar.ReadSize();
            value.clear();
            for (std::size_t i = 0; i < size; ++i)
            {
                T element{};
                Dispatch(ar, element);
                if (!value.insert(std::move(element)).second)
                    throw SerializationError("Duplicate element in serialized set");
            }
        }
    }

    namespace Detail
    {
        template <typename Archive, typename MapT>
        void SerializeMap(Archive& ar, MapT& value)
        {
            using K = typename MapT::key_type;
            using V = typename MapT::mapped_type;
            if constexpr (Archive::IsSaving)
            {
                // Sort entries by key so that saving the same state always
                // produces the same bytes, whatever the hash map iteration order
                std::vector<std::pair<const K*, V*>> entries;
                entries.reserve(value.size());
                for (auto& kv : value)
                    entries.emplace_back(&kv.first, &kv.second);
                std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                    return *a.first < *b.first;
                });
                ar.WriteSize(entries.size());
                for (auto& entry : entries)
                {
                    K key = *entry.first;
                    Dispatch(ar, key);
                    Dispatch(ar, *entry.second);
                }
            }
            else
            {
                std::size_t size = ar.ReadSize();
                value.clear();
                for (std::size_t i = 0; i < size; ++i)
                {
                    K key{};
                    V mapped{};
                    Dispatch(ar, key);
                    Dispatch(ar, mapped);
                    if (!value.emplace(std::move(key), std::move(mapped)).second)
                        throw SerializationError("Duplicate key in serialized map");
                }
            }
        }
    } // namespace Detail

    template <typename Archive, typename K, typename V, typename Compare, typename Alloc>
    void Serialize(Archive& ar, std::map<K, V, Compare, Alloc>& value)
    {
        Detail::SerializeMap(ar, value);
    }

    template <typename Archive, typename K, typename V, typename Hash, typename Eq, typename Alloc>
    void Serialize(Archive& ar, std::unordered_map<K, V, Hash, Eq, Alloc>& value)
    {
        Detail::SerializeMap(ar, value);
    }

    /**
     * \brief Central dispatch point, defined after every overload above so that
     *        unqualified lookup sees them all. User types are found through ADL
     */
    template <typename Archive, typename T>
    void Dispatch(Archive& ar, T& value)
    {
        Serialize(ar, value);
    }

    /**
     * \brief Convenience helper: serialize a value into a fresh byte buffer
     */
    template <typename T>
    std::vector<std::uint8_t> ToBytes(T& value, std::uint32_t version = 1)
    {
        OutputArchive ar;
        ar.SetVersion(version);
        ar(value);
        return ar.TakeBuffer();
    }

    /**
     * \brief Convenience helper: deserialize a value, the whole buffer must be consumed
     */
    template <typename T>
    void FromBytes(const std::vector<std::uint8_t>& bytes, T& value, std::uint32_t version = 1)
    {
        InputArchive ar(bytes);
        ar.SetVersion(version);
        ar(value);
        if (!ar.AtEnd())
            throw SerializationError("Trailing data after deserialized value");
    }
} // namespace Serialization

/**
 * \brief Declare the valid range of a serialized enum (at global scope, after
 *        the enum), see Serialization::EnumRange
 */
#define SERIALIZATION_ENUM_RANGE(Type, First, Last)                                            \
    namespace Serialization                                                                    \
    {                                                                                          \
        template <>                                                                            \
        struct EnumRange<Type>                                                                 \
        {                                                                                      \
            static constexpr bool Defined = true;                                              \
            static constexpr std::int32_t Min = static_cast<std::int32_t>(First);              \
            static constexpr std::int32_t Max = static_cast<std::int32_t>(Last);               \
        };                                                                                     \
    }
