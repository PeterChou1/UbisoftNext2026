//---------------------------------------------------------------------------------
// Crc32.h
//---------------------------------------------------------------------------------
//
// CRC-32 (IEEE 802.3, the same polynomial used by zip/png) used to detect
// corrupted or truncated save files before any game state is modified
//
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Serialization
{
    namespace Detail
    {
        inline const std::array<std::uint32_t, 256>& Crc32Table()
        {
            static const std::array<std::uint32_t, 256> table = [] {
                std::array<std::uint32_t, 256> t{};
                for (std::uint32_t i = 0; i < 256; ++i)
                {
                    std::uint32_t c = i;
                    for (int k = 0; k < 8; ++k)
                        c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                    t[i] = c;
                }
                return t;
            }();
            return table;
        }
    } // namespace Detail

    /**
     * \brief Compute the CRC-32 checksum of a block of memory
     */
    inline std::uint32_t Crc32(const std::uint8_t* data, std::size_t size)
    {
        const auto& table = Detail::Crc32Table();
        std::uint32_t crc = 0xFFFFFFFFu;
        for (std::size_t i = 0; i < size; ++i)
            crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
        return crc ^ 0xFFFFFFFFu;
    }
} // namespace Serialization
