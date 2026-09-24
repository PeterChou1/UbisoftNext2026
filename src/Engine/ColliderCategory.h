#pragma once

#include <utility>

// Collision categories used to register category pair callbacks
// (ColliderCallbackSystem). Scripts usually use per object contact events
// instead, see ContactEvent
enum ColliderCategory
{
    Default,
    Category1,
    Category2,
    Category3,
    Category4,
    Category5,
    Category6,
    Category7,
    Category8
};

namespace std {
    template <>
    struct hash<ColliderCategory>
    {
        std::size_t operator()(ColliderCategory c) const noexcept
        {
            return static_cast<std::size_t>(c);
        }
    };
}

using CollisionPair = std::pair<ColliderCategory, ColliderCategory>;

struct CollisionPairHash
{
    std::size_t operator()(const CollisionPair& pair) const
    {
        return std::hash<ColliderCategory>()(pair.first) ^
               (std::hash<ColliderCategory>()(pair.second) << 1);
    }
};