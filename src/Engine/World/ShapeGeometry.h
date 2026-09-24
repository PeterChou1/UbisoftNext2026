//---------------------------------------------------------------------------------
// ShapeGeometry.h
//---------------------------------------------------------------------------------
//
// Geometry of a Shape2D, shared by every system so they always agree:
//   - Outline:   the footprint polygon (local XZ, as Vec2(x, z)), used by the
//                physics body, the editor's picking and selection overlay
//   - BuildMesh: the render mesh (outline extruded upwards by Thickness)
//
#pragma once

#include "../Vec2.h"
#include "../Vertex.h"
#include "SceneComponents.h"

#include <cstdint>
#include <vector>

namespace ShapeGeometry
{
    // Segments used to approximate a circle
    constexpr int CIRCLE_SEGMENTS = 24;
    constexpr int MIN_POLYGON_SIDES = 3;
    constexpr int MAX_POLYGON_SIDES = 12;

    /**
     * \brief Convex footprint of the shape in local space, counter clockwise
     *        when seen from above (+Y), centred on the origin
     */
    std::vector<Vec2> Outline(const Shape2D& shape);

    struct MeshData
    {
        std::vector<Vertex> Vertices;
        std::vector<std::uint32_t> Indices;
    };

    /**
     * \brief Render mesh: top face at y = Thickness plus the side walls, every
     *        vertex coloured with the shape colour
     */
    MeshData BuildMesh(const Shape2D& shape);

    /**
     * \brief True if the local point (x, z) lies inside the footprint
     */
    bool Contains(const Shape2D& shape, const Vec2& localPoint, float margin = 0.0f);
} // namespace ShapeGeometry
