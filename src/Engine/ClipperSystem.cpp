#include "ClipperSystem.h"

#include "Concurrent.h"
#include "ECSManager.h"
#include "Triangle.h"
#include "stdafx.h"

#include <cassert>

extern ECSManager ECS;

namespace
{
    // A corner of the polygon being clipped: its clip space positions and its
    // weights of the triangle's three vertices
    struct Point
    {
        Vec4 position{};
        Vec4 shadowPosition{};
        Vec3 weights{};
    };

    constexpr uint8_t LEFT_PLANE = 1 << 0, RIGHT_PLANE = 1 << 1, DOWN_PLANE = 1 << 2,
                      UP_PLANE = 1 << 3, NEAR_PLANE = 1 << 4, FAR_PLANE = 1 << 5;
    // The order the planes are clipped against
    constexpr uint8_t CLIP_PLANES[] = {
            LEFT_PLANE, RIGHT_PLANE, DOWN_PLANE, UP_PLANE, NEAR_PLANE, FAR_PLANE};

    // Signed distance of v to a plane of the clip volume (inside when > 0)
    float Dot(uint8_t planeId, const Vec4& v)
    {
        switch (planeId)
        {
        case LEFT_PLANE:
            return v.X + v.W; /* v * (1 0 0 1) left */
        case RIGHT_PLANE:
            return -v.X + v.W; /* v * (-1 0 0 1) right */
        case DOWN_PLANE:
            return v.Y + v.W; /* v * (0 1 0 1) down*/
        case UP_PLANE:
            return -v.Y + v.W; /* v * (0 -1 0 1) up*/
        case FAR_PLANE:
            return v.Z + v.W; /* v * (0 0 1 1) far */
        case NEAR_PLANE:
            return -v.Z + v.W; /* v * (0 0 -1 1) near */
        default:
            assert(false && "unreachable code");
            return 0.0;
        }
    }

    // Where the segment a -> b crosses a plane (0 at a, 1 at b)
    float PointToPlane(uint8_t planeId, const Vec4& a, const Vec4& b)
    {
        const float alpha = Dot(planeId, a);
        const float beta = Dot(planeId, b);
        return alpha / (alpha - beta);
    }

    Point Lerp(const Point& a, const Point& b, float alpha)
    {
        float a1 = 1.0f - alpha;
        return {a.position * a1 + b.position * alpha,
                a.shadowPosition * a1 + b.shadowPosition * alpha,
                a.weights * a1 + b.weights * alpha};
    }

    // The planes v is outside of
    uint8_t OutCode(const Vec4& v)
    {
        uint8_t outcode = 0;
        for (uint8_t plane : CLIP_PLANES)
        {
            if (Dot(plane, v) < 0)
                outcode |= plane;
        }
        return outcode;
    }

    // Sutherland-Hodgman: the part of the polygon inside the plane
    std::vector<Point> ClipPlane(uint8_t planeId, const std::vector<Point>& points)
    {
        std::vector<Point> outPoints;
        for (size_t i = 0; i < points.size(); i++)
        {
            const Point& a = points[i];
            const Point& b = points[i + 1 == points.size() ? 0 : i + 1];
            const Point crossing = Lerp(a, b, PointToPlane(planeId, a.position, b.position));
            const bool isInsideA = Dot(planeId, a.position) > 0;
            const bool isInsideB = Dot(planeId, b.position) > 0;
            if (isInsideA)
                outPoints.push_back(isInsideB ? b : crossing);
            else if (isInsideB)
            {
                outPoints.push_back(crossing);
                outPoints.push_back(b);
            }
        }
        return outPoints;
    }

    /**
     * \brief The triangles of the part of `clip` inside the clip volume,
     *        perspective divided. A triangle entirely inside is `clip`
     *        itself, which is divided in place
     */
    std::vector<Triangle> ClipTriangle(Triangle& clip)
    {
        const uint8_t clipcode1 = OutCode(clip.verts[0].Projection);
        const uint8_t clipcode2 = OutCode(clip.verts[1].Projection);
        const uint8_t clipcode3 = OutCode(clip.verts[2].Projection);

        std::vector<Triangle> clipped;
        // trivial accept
        if (!(clipcode1 | clipcode2 | clipcode3))
        {
            clip.PerspectiveDivision();
            clipped.push_back(clip);
            return clipped;
        }
        // trivial reject: all outside of the same plane
        if (clipcode1 & clipcode2 & clipcode3)
            return clipped;

        const Vertex& v0 = clip.verts[0];
        const Vertex& v1 = clip.verts[1];
        const Vertex& v2 = clip.verts[2];
        std::vector<Point> points = {{v0.Projection, v0.ShadowProjection, {1, 0, 0}},
                                     {v1.Projection, v1.ShadowProjection, {0, 1, 0}},
                                     {v2.Projection, v2.ShadowProjection, {0, 0, 1}}};
        const uint8_t mask = clipcode1 | clipcode2 | clipcode3;
        for (uint8_t plane : CLIP_PLANES)
        {
            if (mask & plane)
                points = ClipPlane(plane, points);
        }

        auto toVertex = [&](const Point& p) {
            Vertex v = v0 * p.weights.X + v1 * p.weights.Y + v2 * p.weights.Z;
            v.Projection = p.position;
            v.ShadowProjection = p.shadowPosition;
            return v;
        };
        // triangulate the points (a fan around the first)
        for (size_t j = 2; j < points.size(); j++)
        {
            Triangle triangle(toVertex(points[0]), toVertex(points[j - 1]), toVertex(points[j]));
            triangle.PerspectiveDivision();
            clipped.push_back(triangle);
        }
        return clipped;
    }
} // namespace

ClipperSystem::ClipperSystem()
{
    m_DepthBuffer = ECS.GetResource<DepthBuffer>();
    m_RenderConstants = ECS.GetResource<RenderConstants>();
    m_VertexBuffer = ECS.GetResource<VertexBuffer>();
    m_IndexBuffer = ECS.GetResource<IndexBuffer>();
    m_ClippedTriangleBuffer = ECS.GetResource<ClippedTriangleBuffer>();
    m_Cam = ECS.GetResource<Camera>();
    m_Lighting = ECS.GetResource<Lighting>();
    m_GameOptions = ECS.GetResource<GameOptions>();
}

void ClipperSystem::Clip()
{
    const std::vector<std::uint32_t>& coreIds = m_RenderConstants->CoreIds;
    const int coreInterval = m_RenderConstants->CoreInterval;
    const std::vector<Vertex>& vertexBuffer = m_VertexBuffer->Buffer;
    const std::vector<std::uint32_t>& indexBuffer = m_IndexBuffer->Buffer;
    DirectionalLight& light = m_Lighting->GetDirectionalLight();
    const bool shadowMap = m_GameOptions->ShadowsOn();

    Concurrent::ForEach(coreIds.begin(), coreIds.end(), [&](unsigned int threadID) {
        // Clip, move to raster space with toRaster and keep the triangles
        // that cover a pixel in this thread's bin
        auto clipInto = [&](Triangle& triangle, std::vector<Triangle>& bin, auto toRaster) {
            for (Triangle& clip : ClipTriangle(triangle))
            {
                toRaster(clip.verts[0].Projection);
                toRaster(clip.verts[1].Projection);
                toRaster(clip.verts[2].Projection);
                if (clip.Setup(static_cast<int>(threadID), static_cast<int>(bin.size())))
                    bin.push_back(clip);
            }
        };
        const int start = static_cast<int>(threadID * coreInterval);
        const int end = static_cast<int>((threadID + 1) * coreInterval);
        for (int i = start; i < end; i++)
        {
            if (3 * i + 2 > static_cast<int>(indexBuffer.size()))
                break;

            assert(indexBuffer[3 * i] < vertexBuffer.size());
            assert(indexBuffer[3 * i + 1] < vertexBuffer.size());
            assert(indexBuffer[3 * i + 2] < vertexBuffer.size());

            const Vertex& v1 = vertexBuffer[indexBuffer[3 * i]];
            const Vertex& v2 = vertexBuffer[indexBuffer[3 * i + 1]];
            const Vertex& v3 = vertexBuffer[indexBuffer[3 * i + 2]];
            Triangle t(v1, v2, v3);
            Vec3 normal = (v1.Normal + v2.Normal + v3.Normal) / 3;
            // Back face culling, then clip the triangle from the camera
            if (normal.Dot(v1.Position - m_Cam->Position) < 0.0)
            {
                clipInto(t, m_ClippedTriangleBuffer->CameraClipBuffer[threadID], [&](Vec4& point) {
                    m_Cam->ToRasterSpace(point);
                });
            }
            if (!shadowMap)
                continue;

            // Clip from the light perspective (parallel light: the faces
            // turned towards it)
            const bool facesLight = light.lightType == SpotLight
                                            ? normal.Dot(v1.Position - light.Position) < 0.0
                                            : normal.Dot(light.Direction) < 0.0;
            if (facesLight)
            {
                t.verts[0].Projection = v1.ShadowProjection;
                t.verts[1].Projection = v2.ShadowProjection;
                t.verts[2].Projection = v3.ShadowProjection;
                clipInto(t, m_ClippedTriangleBuffer->LightClipBuffer[threadID], [&](Vec4& point) {
                    m_DepthBuffer->ToShadowSpace(point);
                });
            }
        }
    });
}
