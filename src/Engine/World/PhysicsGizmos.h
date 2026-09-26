//---------------------------------------------------------------------------------
// PhysicsGizmos.h
//---------------------------------------------------------------------------------
//
// Debug outlines of physics bodies: each collider as a wire prism (its 2D
// shape at the object's base and top, joined by vertical edges), coloured by
// what the body is:
//
//   static   green      dynamic  cyan
//   trigger  yellow     touching another body (while simulating)  red
//
// The lines are in world space; the editor projects and draws them (it can
// not be drawn here: the engine does not know the view). Headless code and
// tests read them directly.
//
#pragma once

#include "Entity.h"
#include "Vec3.h"

#include <vector>

namespace PhysicsGizmos
{
    struct Line
    {
        Vec3 A;
        Vec3 B;
    };

    struct GizmoColor
    {
        float R = 1.0f;
        float G = 1.0f;
        float B = 1.0f;
    };

    inline constexpr GizmoColor STATIC_COLOR = {0.35f, 0.95f, 0.45f};
    inline constexpr GizmoColor DYNAMIC_COLOR = {0.35f, 0.85f, 1.0f};
    inline constexpr GizmoColor TRIGGER_COLOR = {1.0f, 0.85f, 0.25f};
    inline constexpr GizmoColor TOUCHING_COLOR = {1.0f, 0.3f, 0.3f};

    /**
     * \brief Wire prism of the entity's collider (empty without a body).
     *        Circles use `circleSegments` sides and get a radius line showing
     *        how the body is turned
     */
    std::vector<Line> ColliderLines(Entity entity, int circleSegments = 24);

    /**
     * \brief Colour of the entity's collider. While `simulating`, bodies
     *        that overlapped another body in the last physics step are red
     *        (outside play the flag is left over from the last run)
     */
    GizmoColor ColorOf(Entity entity, bool simulating);
} // namespace PhysicsGizmos
