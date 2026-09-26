//---------------------------------------------------------------------------------
// ShapeGeometryTests.cpp
//---------------------------------------------------------------------------------
//
// The 2D shapes drawn by the 3D renderer and simulated by the 2D physics must
// agree on orientation: outlines are counter clockwise seen from above (+Y),
// render triangles face outwards, bodies have positive mass and objects point
// the way scripts think they point
//
#include "WorldFixture.h"
#include "World/ShapeGeometry.h"

#include <cmath>

namespace
{
    // Twice the signed area of an (x, z) outline, measured around +Y
    // (positive = counter clockwise seen from above)
    float SignedAreaAroundUp(const std::vector<Vec2>& outline)
    {
        float sum = 0.0f;
        for (size_t i = 0; i < outline.size(); ++i)
        {
            const Vec2& a = outline[i];
            const Vec2& b = outline[(i + 1) % outline.size()];
            // y component of (a.x, 0, a.z) x (b.x, 0, b.z)
            sum += a.Y * b.X - a.X * b.Y;
        }
        return sum;
    }

    Shape2D MakeShape(Shape2DType type, float w = 2.0f, float h = 1.0f, int sides = 6)
    {
        Shape2D shape;
        shape.Type = type;
        shape.Width = w;
        shape.Height = h;
        shape.Sides = sides;
        shape.Thickness = 0.5f;
        return shape;
    }

    bool Near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }
} // namespace

TEST_CASE("ShapeGeometry: every outline is counter clockwise seen from above")
{
    for (int t = 0; t < static_cast<int>(Shape2DType::Count); ++t)
    {
        for (int sides = 3; sides <= 12; ++sides)
        {
            std::vector<Vec2> outline = ShapeGeometry::Outline(MakeShape(static_cast<Shape2DType>(t), 2.0f, 1.0f, sides));
            REQUIRE(outline.size() >= 3);
            CHECK(SignedAreaAroundUp(outline) > 0.0f);
        }
    }
}

TEST_CASE("ShapeGeometry: outline sizes follow the shape settings")
{
    CHECK_EQ(ShapeGeometry::Outline(MakeShape(Shape2DType::Rectangle)).size(), size_t(4));
    CHECK_EQ(ShapeGeometry::Outline(MakeShape(Shape2DType::Triangle)).size(), size_t(3));
    CHECK_EQ(ShapeGeometry::Outline(MakeShape(Shape2DType::Polygon, 2, 1, 7)).size(), size_t(7));
    // Sides are clamped to [3, 12]
    CHECK_EQ(ShapeGeometry::Outline(MakeShape(Shape2DType::Polygon, 2, 1, 1)).size(), size_t(3));
    CHECK_EQ(ShapeGeometry::Outline(MakeShape(Shape2DType::Polygon, 2, 1, 40)).size(), size_t(12));

    // Rectangle area = W x H, circle radius = W / 2
    CHECK(Near(SignedAreaAroundUp(ShapeGeometry::Outline(MakeShape(Shape2DType::Rectangle, 3, 2))) * 0.5f, 6.0f));
    for (const Vec2& p : ShapeGeometry::Outline(MakeShape(Shape2DType::Circle, 3, 1)))
        CHECK(Near(p.GetMagnitude(), 1.5f));
}

TEST_CASE("ShapeGeometry: render triangles face outwards (counter clockwise about their normal)")
{
    for (int t = 0; t < static_cast<int>(Shape2DType::Count); ++t)
    {
        Shape2D shape = MakeShape(static_cast<Shape2DType>(t), 2.0f, 1.5f, 5);
        shape.Color = Vec3(0.2f, 0.4f, 0.6f);
        ShapeGeometry::MeshData mesh = ShapeGeometry::BuildMesh(shape);
        REQUIRE(!mesh.Indices.empty());
        REQUIRE(mesh.Indices.size() % 3 == 0);
        size_t outline = ShapeGeometry::Outline(shape).size();
        // Top fan + two triangles per side wall
        CHECK_EQ(mesh.Indices.size() / 3, outline * 3);

        Vec3 centre(0.0f, shape.Thickness * 0.5f, 0.0f);
        for (size_t i = 0; i < mesh.Indices.size(); i += 3)
        {
            const Vertex& a = mesh.Vertices[mesh.Indices[i]];
            const Vertex& b = mesh.Vertices[mesh.Indices[i + 1]];
            const Vertex& c = mesh.Vertices[mesh.Indices[i + 2]];
            Vec3 faceNormal = (b.LocalPosition - a.LocalPosition).Cross(c.LocalPosition - a.LocalPosition);
            // Winding agrees with the stored normal...
            CHECK(faceNormal.Dot(a.LocalNormal) > 0.0f);
            // ...and the normal points away from the inside of the prism
            Vec3 faceCentre = (a.LocalPosition + b.LocalPosition + c.LocalPosition) * (1.0f / 3.0f);
            CHECK(a.LocalNormal.Dot(faceCentre - centre) > 0.0f);
        }
        for (const Vertex& v : mesh.Vertices)
        {
            CHECK(Fixture::Same(v.Color, shape.Color));
            CHECK_EQ(v.TextureID, -1);
        }
    }
}

TEST_CASE("ShapeGeometry: a flat shape only has its top face")
{
    Shape2D shape = MakeShape(Shape2DType::Rectangle);
    shape.Thickness = 0.0f;
    ShapeGeometry::MeshData mesh = ShapeGeometry::BuildMesh(shape);
    CHECK_EQ(mesh.Indices.size(), size_t(4 * 3));
}

TEST_CASE("ShapeGeometry: point containment with margins")
{
    Shape2D rect = MakeShape(Shape2DType::Rectangle, 4.0f, 2.0f);
    CHECK(ShapeGeometry::Contains(rect, Vec2(0, 0)));
    CHECK(ShapeGeometry::Contains(rect, Vec2(1.9f, 0.9f)));
    CHECK(!ShapeGeometry::Contains(rect, Vec2(2.1f, 0)));
    CHECK(!ShapeGeometry::Contains(rect, Vec2(0, 1.1f)));
    CHECK(ShapeGeometry::Contains(rect, Vec2(2.1f, 0), 0.2f));

    Shape2D tri = MakeShape(Shape2DType::Triangle, 2.0f, 2.0f);
    CHECK(ShapeGeometry::Contains(tri, Vec2(0, 0.9f)));   // near the tip (+Z)
    CHECK(!ShapeGeometry::Contains(tri, Vec2(0.9f, 0.9f))); // beside the tip
    CHECK(!ShapeGeometry::Contains(tri, Vec2(0, -1.1f)));

    Shape2D circle = MakeShape(Shape2DType::Circle, 2.0f);
    CHECK(ShapeGeometry::Contains(circle, Vec2(0.6f, 0.6f)));
    CHECK(!ShapeGeometry::Contains(circle, Vec2(0.8f, 0.8f)));
}

TEST_CASE("SceneObjects: containment follows the object's position, rotation and scale")
{
    Fixture::FreshWorld();
    SceneObjects::ShapeDesc desc = Fixture::ShapeOf("Bar", Shape2DType::Rectangle, {5, 0, 5});
    desc.Shape.Width = 4.0f;
    desc.Shape.Height = 1.0f;
    Entity bar = SceneObjects::CreateShape(desc);
    CHECK(SceneObjects::Contains(bar, {6.5f, 0, 5}));
    CHECK(!SceneObjects::Contains(bar, {5, 0, 6.5f}));

    SceneObjects::SetYaw(bar, 90.0f);
    CHECK_EQ(SceneObjects::GetYaw(bar), 90.0f);
    CHECK(!SceneObjects::Contains(bar, {6.5f, 0, 5}));
    CHECK(SceneObjects::Contains(bar, {5, 0, 6.5f}));

    // Models are picked as a circle of radius 0.5 x scale
    Entity crate = SceneObjects::CreateModel("Crate", "Box", {-5, 0, 0}, 30.0f, 2.0f);
    CHECK(SceneObjects::Contains(crate, {-5.8f, 0, 0}));
    CHECK(SceneObjects::Contains(crate, {-5, 0, 0.9f}));
    CHECK(!SceneObjects::Contains(crate, {-6.2f, 0, 0}));
    CHECK(!SceneObjects::Contains(crate, {-3.8f, 0, 0}));
    CHECK(SceneObjects::Contains(crate, {-3.8f, 0, 0}, 0.3f));

    // Scaled shapes too
    ECS.GetComponent<Transform>(bar).Scale(2.0f);
    CHECK(SceneObjects::Contains(bar, {5, 0, 8.5f}));
    CHECK(!SceneObjects::Contains(bar, {5, 0, 9.5f}));
}

TEST_CASE("SceneObjects: yaw turns +Z the same way scripts assume (sin, 0, cos)")
{
    Fixture::FreshWorld();
    SceneObjects::ShapeDesc desc = Fixture::ShapeOf("Arrow", Shape2DType::Triangle, {0, 0, 0});
    desc.Shape.Width = 1.0f;
    desc.Shape.Height = 2.0f;
    Entity arrow = SceneObjects::CreateShape(desc);
    for (float yaw : {0.0f, 30.0f, 90.0f, 200.0f, 315.0f})
    {
        SceneObjects::SetYaw(arrow, yaw);
        CHECK(Near(SceneObjects::GetYaw(arrow), yaw, 0.01f));
        // The tip of the triangle is its local +Z point (0, 1)
        float rad = yaw * 3.14159265f / 180.0f;
        Vec3 expectedTip(std::sin(rad) * 1.0f, 0.0f, std::cos(rad) * 1.0f);
        std::vector<Vec3> outline = SceneObjects::WorldOutline(arrow);
        REQUIRE(outline.size() == 3);
        CHECK(Near(outline[1].X, expectedTip.X, 1e-3f));
        CHECK(Near(outline[1].Z, expectedTip.Z, 1e-3f));
    }
}

TEST_CASE("SceneObjects: bodies match the shape and body type")
{
    Fixture::FreshWorld();
    using SceneObjects::BodyType;
    for (int t = 0; t < static_cast<int>(Shape2DType::Count); ++t)
    {
        SceneObjects::ShapeDesc desc = Fixture::ShapeOf("Body", static_cast<Shape2DType>(t), {1, 0, 2}, BodyType::Dynamic);
        desc.Shape.Width = 2.0f;
        desc.Shape.Height = 1.0f;
        desc.Shape.Sides = 5;
        Entity e = SceneObjects::CreateShape(desc);
        REQUIRE(ECS.HasComponent<RigidBody>(e));
        const RigidBody& body = ECS.GetComponent<RigidBody>(e);
        // Positive mass and inertia: the polygon winding was accepted
        CHECK(body.InvMass() > 0.0f);
        CHECK(body.InvInertia() > 0.0f);
        CHECK(std::isfinite(body.InvMass()));
        CHECK(std::isfinite(body.InvInertia()));
        CHECK(body.Collidable);
        CHECK(SceneObjects::GetBodyType(e) == BodyType::Dynamic);

        SceneObjects::SetBodyType(e, BodyType::Static);
        CHECK(ECS.GetComponent<RigidBody>(e).IsStatic());
        CHECK(SceneObjects::GetBodyType(e) == BodyType::Static);

        SceneObjects::SetBodyType(e, BodyType::Trigger);
        CHECK(!ECS.GetComponent<RigidBody>(e).Collidable);
        CHECK(SceneObjects::GetBodyType(e) == BodyType::Trigger);

        SceneObjects::SetBodyType(e, BodyType::None);
        CHECK(!ECS.HasComponent<RigidBody>(e));
        CHECK(SceneObjects::GetBodyType(e) == BodyType::None);
    }
}

TEST_CASE("RigidBody: polygon mass matches a rectangle of the same size")
{
    RigidBody rect(2.0f, 1.0f);
    RigidBody poly(std::vector<Vec2>{Vec2(-1.0f, -0.5f), Vec2(1.0f, -0.5f), Vec2(1.0f, 0.5f), Vec2(-1.0f, 0.5f)});
    CHECK(Near(rect.InvMass(), poly.InvMass()));
    CHECK(Near(rect.InvInertia(), poly.InvInertia(), 1e-3f));
    // Clockwise points give the same body
    RigidBody reversed(std::vector<Vec2>{Vec2(-1.0f, 0.5f), Vec2(1.0f, 0.5f), Vec2(1.0f, -0.5f), Vec2(-1.0f, -0.5f)});
    CHECK(Near(reversed.InvMass(), poly.InvMass()));
    CHECK(Near(reversed.InvInertia(), poly.InvInertia(), 1e-3f));
}

TEST_CASE("SceneObjects: lookups by name and tag, unique names, recursive destroy")
{
    Fixture::SampleWorld w = Fixture::BuildSampleWorld();
    CHECK_EQ(SceneObjects::FindByName("Wall"), w.Wall);
    CHECK_EQ(SceneObjects::FindByName("Nope"), NULL_ENTITY);
    CHECK(SceneObjects::FindByTag("Pickup") == std::vector<Entity>({w.Pickup}));
    CHECK_EQ(SceneObjects::UniqueName("Wall"), std::string("Wall 2"));
    CHECK_EQ(SceneObjects::UniqueName("Fresh"), std::string("Fresh"));

    SceneObjects::Destroy(w.Parent);
    ECS.FlushECS();
    CHECK(!ECS.IsEntityAlive(w.Parent));
    CHECK(!ECS.IsEntityAlive(w.Child));
}
