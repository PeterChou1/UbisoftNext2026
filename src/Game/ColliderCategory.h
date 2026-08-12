#pragma once

#include <utility>

enum ColliderCategory
{
    Default,
    UnitCollider,
    FallCollider,
    ExplosionCollider,
    GolfBallCollider,
    GoalPostCollider,
    MovingObstacleCollider,
    BulletCollider,
    PlayerCollider,
    EnemyCollider
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