#include "ShapeGeometry.h"

#include <algorithm>
#include <cmath>

namespace ShapeGeometry
{
    namespace
    {
        constexpr float TWO_PI = 6.28318530718f;

        std::vector<Vec2> RegularPolygon(int sides, float radius)
        {
            std::vector<Vec2> points;
            points.reserve(sides);
            for (int i = 0; i < sides; ++i)
            {
                // Start at +Z so polygons point "forward"
                float angle = TWO_PI * static_cast<float>(i) / static_cast<float>(sides);
                points.emplace_back(radius * std::sin(angle), radius * std::cos(angle));
            }
            // (sin, cos) with a growing angle is counter clockwise around +Y
            return points;
        }

        Vertex MakeVertex(const Vec3& position, const Vec3& normal, const Vec3& color)
        {
            Vertex v(position, normal, Vec2(0.0f, 0.0f));
            v.Color = color;
            // No .obj material: the renderer falls back to its default material
            v.TextureID = -1;
            return v;
        }

        // Append a triangle, flipping it if needed so it winds counter clockwise
        // around `normal` (OBJ convention: front faces are CCW)
        void AddTriangle(std::vector<std::uint32_t>& indices,
                         const std::vector<Vertex>& vertices,
                         std::uint32_t a,
                         std::uint32_t b,
                         std::uint32_t c,
                         const Vec3& normal)
        {
            Vec3 ab = vertices[b].LocalPosition - vertices[a].LocalPosition;
            Vec3 ac = vertices[c].LocalPosition - vertices[a].LocalPosition;
            if (ab.Cross(ac).Dot(normal) < 0.0f)
                std::swap(b, c);
            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(c);
        }
    } // namespace

    std::vector<Vec2> Outline(const Shape2D& shape)
    {
        float w = std::max(shape.Width, 0.01f);
        float h = std::max(shape.Height, 0.01f);
        switch (shape.Type)
        {
        case Shape2DType::Circle:
            return RegularPolygon(CIRCLE_SEGMENTS, w * 0.5f);
        case Shape2DType::Polygon:
            return RegularPolygon(
                    std::clamp(shape.Sides, MIN_POLYGON_SIDES, MAX_POLYGON_SIDES), w * 0.5f);
        case Shape2DType::Triangle:
            // Isosceles, pointing towards +Z, CCW seen from above
            return {Vec2(-w * 0.5f, -h * 0.5f), Vec2(0.0f, h * 0.5f), Vec2(w * 0.5f, -h * 0.5f)};
        case Shape2DType::Rectangle:
        default:
            return {Vec2(-w * 0.5f, -h * 0.5f),
                    Vec2(-w * 0.5f, h * 0.5f),
                    Vec2(w * 0.5f, h * 0.5f),
                    Vec2(w * 0.5f, -h * 0.5f)};
        }
    }

    MeshData BuildMesh(const Shape2D& shape)
    {
        MeshData mesh;
        std::vector<Vec2> outline = Outline(shape);
        const float top = std::max(shape.Thickness, 0.0f);
        const Vec3 up(0.0f, 1.0f, 0.0f);
        const size_t n = outline.size();

        // Top face: triangle fan around the centre (the outline is convex)
        std::uint32_t centre = static_cast<std::uint32_t>(mesh.Vertices.size());
        mesh.Vertices.push_back(MakeVertex(Vec3(0.0f, top, 0.0f), up, shape.Color));
        for (const Vec2& p : outline)
            mesh.Vertices.push_back(MakeVertex(Vec3(p.X, top, p.Y), up, shape.Color));
        for (size_t i = 0; i < n; ++i)
        {
            std::uint32_t a = centre + 1 + static_cast<std::uint32_t>(i);
            std::uint32_t b = centre + 1 + static_cast<std::uint32_t>((i + 1) % n);
            AddTriangle(mesh.Indices, mesh.Vertices, centre, a, b, up);
        }

        if (top <= 0.0f)
            return mesh;

        // Side walls: one quad per edge with its own outward normal
        for (size_t i = 0; i < n; ++i)
        {
            const Vec2& p0 = outline[i];
            const Vec2& p1 = outline[(i + 1) % n];
            Vec2 edge = p1 - p0;
            Vec3 normal(edge.Y, 0.0f, -edge.X);
            // Make sure it points away from the centre
            Vec3 mid((p0.X + p1.X) * 0.5f, 0.0f, (p0.Y + p1.Y) * 0.5f);
            if (normal.Dot(mid) < 0.0f)
                normal = normal * -1.0f;
            normal.Normalize();

            std::uint32_t base = static_cast<std::uint32_t>(mesh.Vertices.size());
            mesh.Vertices.push_back(MakeVertex(Vec3(p0.X, 0.0f, p0.Y), normal, shape.Color));
            mesh.Vertices.push_back(MakeVertex(Vec3(p1.X, 0.0f, p1.Y), normal, shape.Color));
            mesh.Vertices.push_back(MakeVertex(Vec3(p1.X, top, p1.Y), normal, shape.Color));
            mesh.Vertices.push_back(MakeVertex(Vec3(p0.X, top, p0.Y), normal, shape.Color));
            AddTriangle(mesh.Indices, mesh.Vertices, base, base + 1, base + 2, normal);
            AddTriangle(mesh.Indices, mesh.Vertices, base, base + 2, base + 3, normal);
        }
        return mesh;
    }

    bool Contains(const Shape2D& shape, const Vec2& p, float margin)
    {
        std::vector<Vec2> outline = Outline(shape);
        const size_t n = outline.size();
        // Convex polygon, CCW: inside if left of (or within margin of) every edge
        for (size_t i = 0; i < n; ++i)
        {
            Vec2 a = outline[i];
            Vec2 b = outline[(i + 1) % n];
            Vec2 edge = b - a;
            float length = edge.GetMagnitude();
            if (length <= 0.0f)
                continue;
            // Signed distance, positive on the outside for a CCW (x, z) polygon
            float outside = (edge.X * (p.Y - a.Y) - edge.Y * (p.X - a.X)) / length;
            if (outside > margin)
                return false;
        }
        return true;
    }
} // namespace ShapeGeometry
