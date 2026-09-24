#include "DebugPhysicsRenderer.h"

#include "Concurrent.h"
#include "ECSManager.h"
#include "FragShaderTag.h"
#include "Mesh.h"
#include "RigidBody.h"
#include "Shape.h"
#include "Transform.h"
#include "Vertex.h"
#include "app.h"
#include "stdafx.h"

extern ECSManager ECS;

void RenderAABB(std::vector<Vertex>& vertexBuffer, Vec2 min, Vec2 max, Vec3 color)
{
    Vec3 tR = Vec3(max.X, 0.0, max.Y);
    Vec3 tL = Vec3(min.X, 0.0, max.Y);
    Vec3 bR = Vec3(max.X, 0.0, min.Y);
    Vec3 bL = Vec3(min.X, 0.0, min.Y);
    Vertex topRight = tR;
    Vertex topLeft = tL;
    Vertex bottomRight = bR;
    Vertex bottomLeft = bL;

    topRight.Color = color;
    topLeft.Color = color;
    bottomRight.Color = color;
    bottomLeft.Color = color;

    // -- push back top right line --
    vertexBuffer.push_back(topRight);
    vertexBuffer.push_back(topLeft);

    vertexBuffer.push_back(topRight);
    vertexBuffer.push_back(bottomRight);

    vertexBuffer.push_back(bottomRight);
    vertexBuffer.push_back(bottomLeft);

    vertexBuffer.push_back(topLeft);
    vertexBuffer.push_back(bottomLeft);
}

void RenderCircle(std::vector<Vertex>& vertexBuffer, float radius, Vec2 pos, Vec3 color)
{
    const int segments = 36;                          // Increase for a smoother circle
    const float increment = 2.0f * 3.141f / segments; // Increment angle

    for (int i = 0; i < segments; ++i)
    {
        // Calculate the x and y coordinates for the current point
        float theta = static_cast<float>(i) * increment;
        Vec3 point1 = Vec3(radius * cosf(theta), 0.0f, radius * sinf(theta));
        point1.X += pos.X;
        point1.Z += pos.Y;
        // Calculate the x and y coordinates for the next point
        theta = (i + 1) * increment;
        Vec3 point2 = Vec3(radius * cosf(theta), 0.0f, radius * sinf(theta));
        point2.X += pos.X;
        point2.Z += pos.Y;

        Vertex p1 = point1;
        Vertex p2 = point2;

        p1.Color = color;
        p2.Color = color;

        // Transform and add the points to the vertex buffer
        vertexBuffer.push_back(p1);
        vertexBuffer.push_back(p2);
    }
}

void RenderPolygon(std::vector<Vertex>& vertexBuffer,
                   std::vector<Vec2>& polygonPoints,
                   std::vector<Vec2>& debugPoints,
                   Vec3& color)
{
    int polySize = static_cast<int>(polygonPoints.size());

    for (int i = 0; i < polySize; i++)
    {
        Vec2 pt1 = polygonPoints[i];
        Vec2 pt2 = polygonPoints[i + 1 == polySize ? 0 : i + 1];

        Vec3 point1 = Vec3(pt1.X, 0.0f, pt1.Y);
        Vec3 point2 = Vec3(pt2.X, 0.0f, pt2.Y);

        Vertex vertex1 = Vertex(point1);
        Vertex vertex2 = Vertex(point2);

        vertex1.Color = color;
        vertex2.Color = color;

        vertexBuffer.push_back(vertex1);
        vertexBuffer.push_back(vertex2);
    }

    for (Vec2& pt : debugPoints)
    {
        Vec3 point = Vec3(pt.X, 0, pt.Y);
        Vertex v = Vertex(point);
        v.Color = Vec3(1.0, 0.0, 0.0);
        vertexBuffer.push_back(v);
    }
}
void RenderCircle(std::vector<Vertex>& vertexBuffer, float radius, Vec3& pos, Vec3 color)
{
    // Number of line segments to approximate the circle
    const int segments = 36;
    // Angle (in radians) between each segment on the base
    const float increment = 2.0f * 3.14159f / segments;
    // We'll store the base vertices as we compute them, so we can:
    //   1) Draw the circle
    //   2) Connect the base vertices with the apex to form the cone sides
    std::vector<Vertex> baseVerts;
    baseVerts.reserve(segments);

    for (int i = 0; i < segments; ++i)
    {
        float theta = i * increment;

        // Point on the base circle using 'right' and 'upVec'
        Vec3 circlePoint = pos + Vec3(1, 0, 0) * (radius * cosf(theta)) +
                           Vec3(0, 0, 1) * (radius * sinf(theta));

        Vertex v = Vertex(circlePoint);
        v.Color = color;
        baseVerts.emplace_back(v);
    }
    // 1) Draw the base circle as connected line segments
    for (int i = 0; i < segments; ++i)
    {
        // current point and the "next" point (wrapping around at the end)
        int nextIndex = (i + 1) % segments;
        vertexBuffer.push_back(baseVerts[i]);
        vertexBuffer.push_back(baseVerts[nextIndex]);
    }
}

void RenderCircle(std::vector<Vertex>& vertexBuffer, float radius, Transform& pos, Vec3 color)
{
    float height = 1.0f;
    // Number of line segments to approximate the circle
    const int segments = 36;
    // Angle (in radians) between each segment on the base
    const float increment = 2.0f * 3.14159f / segments;

    // Get the apex position and normalized forward vector
    Vec3 apex = pos.GetWorldPosition();
    Vec3 forward = pos.GetForward().Normalize();

    Vertex v1 = Vertex(apex);
    v1.Color = {0, 1, 0};
    Vertex v2 = Vertex(apex + forward);
    v2.Color = {0, 1, 0};

    vertexBuffer.push_back(v1);
    vertexBuffer.push_back(v2);

    // "World up" used to compute two perpendicular vectors for the base plane
    Vec3 worldUp(0.0f, 1.0f, 0.0f);
    if (fabsf(forward.Dot(worldUp)) > 0.9f)
    {
        // If forward is almost parallel to worldUp, pick a different vector
        worldUp = Vec3(1.0f, 0.0f, 0.0f);
    }

    // Compute two vectors that span a plane orthogonal to 'forward':
    // right = forward x up, upVec = right x forward
    Vec3 right = forward.Cross(worldUp).Normalize();
    Vec3 upVec = right.Cross(forward).Normalize();

    // The center of the base circle is at apex + forward * height
    Vec3 baseCenter = apex + forward * height;

    // We'll store the base vertices as we compute them, so we can:
    //   1) Draw the circle
    //   2) Connect the base vertices with the apex to form the cone sides
    std::vector<Vertex> baseVerts;
    baseVerts.reserve(segments);

    for (int i = 0; i < segments; ++i)
    {
        float theta = i * increment;

        // Point on the base circle using 'right' and 'upVec'
        Vec3 circlePoint =
                baseCenter + right * (radius * cosf(theta)) + upVec * (radius * sinf(theta));

        Vertex v = Vertex(circlePoint);
        v.Color = color;
        baseVerts.emplace_back(v);
    }

    // 1) Draw the base circle as connected line segments
    for (int i = 0; i < segments; ++i)
    {
        // current point and the "next" point (wrapping around at the end)
        int nextIndex = (i + 1) % segments;

        vertexBuffer.push_back(baseVerts[i]);
        vertexBuffer.push_back(baseVerts[nextIndex]);
    }

    // 2) Connect each base vertex to the apex to form the cone sides
    //    We can draw lines from apex to each base vertex,
    //    or connect apex->p1->p2->apex for each segment if you want the "triangle" edges.
    Vertex apexVertex(apex);
    apexVertex.Color = color;
    for (int i = 0; i < segments; ++i)
    {
        // current point and the "next" point
        int nextIndex = (i + 1) % segments;
        Vertex& p1 = baseVerts[i];
        Vertex& p2 = baseVerts[nextIndex];

        // Option A: Just connect apex -> p1 and apex -> p2 for a wireframe
        vertexBuffer.push_back(apexVertex);
        vertexBuffer.push_back(p1);

        vertexBuffer.push_back(apexVertex);
        vertexBuffer.push_back(p2);
    }
}

DebugPhysicsRenderer::DebugPhysicsRenderer()
{
    m_Cam = ECS.GetResource<Camera>();
    m_light = ECS.GetResource<Lighting>();
}

void DebugPhysicsRenderer::Update(float deltaTime)
{
    accumulate += deltaTime / 1000.0f;
    // Used to test physics system
    if (accumulate > 0.100)
    {
        // if (App::IsKeyPressed('M'))
        // {
        //     float x = 0.0f, y = 0.0f;
        //     App::GetMousePos(x, y);
        //     Vec3 planePoint = Vec3(0, 0, 0);
        //     Vec3 planeNormal = Vec3(0, 0, -1);
        //     Vec3 point = m_Cam->ScreenSpaceToWorldPoint(x, y, planePoint,
        //     planeNormal); Entity meshEntity = ECS.CreateEntity(); auto
        //     modelTransform = Transform(point, Quat(Vec3(0, 0, 1), 0.0));
        //     ECS.AddComponent<Transform>(meshEntity, modelTransform);
        //     ECS.AddComponent<RigidBody>(meshEntity, RigidBody(1.0f));
        //     accumulate = 0.0f;
        // }
        // if (App::IsKeyPressed('N')) {
        //   float x = 0.0f, y = 0.0f;
        //   App::GetMousePos(x, y);
        //   Vec3 planePoint = Vec3(0, 0, 0);
        //   Vec3 planeNormal = Vec3(0, 0, -1);
        //   Vec3 point =
        //       m_Cam->ScreenSpaceToWorldPoint(x, y, planePoint, planeNormal);
        //   Entity meshEntity = ECS.CreateEntity();
        //   auto modelTransform = Transform(point, Quat(Vec3(0, 0, 1), 0.0));
        //   ECS.AddComponent<Transform>(meshEntity, modelTransform);
        //   ECS.AddComponent<RigidBody>(meshEntity, RigidBody(6.0f, 1.0f));
        //   ECS.AddComponent<Mesh>(meshEntity, Mesh(WoodCube));
        //   ECS.AddComponent<FragShaderTag>(meshEntity, FragShaderTag(UnlitShaderID));
        //
        //   accumulate = 0.0f;
        // }
    }
}

void DebugPhysicsRenderer::Render()
{
    std::vector<Vertex> debugVertexBuffer;
    std::vector<Vertex> debugPoints;
    DirectionalLight& Light = m_light->GetDirectionalLight();
    RenderCircle(debugVertexBuffer, 0.05f, Light.Position, {1, 0, 0});
    RenderCircle(debugVertexBuffer, 0.10f, Light.Position, {1, 0, 0});
    for (auto& e : ECS.Visit<RigidBody>())
    {
        RigidBody& r = ECS.GetComponent<RigidBody>(e);

        Shape& shape = r.Shape;

        RenderAABB(
                debugVertexBuffer, r.RigidBodyAABB.Min, r.RigidBodyAABB.Max, Vec3(0.0, 0.0, 1.0));

        switch (shape.GetShapeType())
        {
        case CircleShape:
            RenderCircle(debugVertexBuffer, shape.Radius, r.Position, r.Color);
            break;
        case PolygonShape:
            RenderPolygon(debugVertexBuffer, shape.PolygonPoints, shape.DebugPoints, r.Color);
            break;
        }

        for (Vec2& pts : shape.ContactPoints)
        {
            Vec3 point = Vec3(pts.X, 0.0, pts.Y);
            debugPoints.push_back(point);
        }
    }
    Concurrent::ForEach(debugVertexBuffer.begin(), debugVertexBuffer.end(), [&](Vertex& v) {
        v.Projection = m_Cam->Proj * Vec4(m_Cam->WorldToCamera(v.Position));
    });

    int lineSize = static_cast<int>(debugVertexBuffer.size() / 2);

    for (int i = 0; i < lineSize; i++)
    {
        int idx = i * 2;

        Vertex& start = debugVertexBuffer[idx];
        Vertex& end = debugVertexBuffer[idx + 1];

        start.PerspectiveDivision();
        end.PerspectiveDivision();

        m_Cam->ToRasterSpaceUnclamped(start.Projection);
        m_Cam->ToRasterSpaceUnclamped(end.Projection);

        float startX = start.Projection.X;
        float startY = start.Projection.Y;

        float endX = end.Projection.X;
        float endY = end.Projection.Y;

        Vec3 color = start.Color;

        App::DrawLine(startX, startY, endX, endY, color.X, color.Y, color.Z);
    }
}
