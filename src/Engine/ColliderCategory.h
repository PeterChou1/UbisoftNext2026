#pragma once

#include <cstddef>
#include <utility>

// Collision categories used to register category pair callbacks
// (ColliderCallbackSystem). Scripts usually use per object contact events
// instead, see ContactEvent. Stored in scene files (RigidBody::Category)
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

using CollisionPair = std::pair<ColliderCategory, ColliderCategory>;

struct CollisionPairHash
{
    std::size_t operator()(const CollisionPair& pair) const
    {
        return static_cast<std::size_t>(pair.first) ^ (static_cast<std::size_t>(pair.second) << 1);
    }
};
