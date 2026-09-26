//---------------------------------------------------------------------------------
// ColliderTests.cpp
//---------------------------------------------------------------------------------
//
// Collider shapes (ColliderShape: the physics body as a box, circle or the
// object's outline, scaled) and the physics debug outlines (PhysicsGizmos):
// bodies are built from the choice, it survives copies, prefabs, files and
// undo, and the outline drawn is where the physics system puts the body
//
#include "PhysicsSystem.h"
#include "SceneEditor.h"
#include "World/PhysicsGizmos.h"
#include "WorldFixture.h"

#include <cmath>
#include <filesystem>

using Editor::ObjectKind;
using Editor::PlaceSettings;
using Editor::SceneEditor;
using SceneObjects::BodyType;

namespace
{
    SceneEditor& NewEditor()
    {
        static SceneEditor editor;
        Fixture::FreshWorld();
        editor = SceneEditor{};
        editor.NewScene();
        return editor;
    }

    PlaceSettings Brush(float w, float h, BodyType body)
    {
        PlaceSettings s;
        s.Width = w;
        s.Height = h;
        s.Body = body;
        return s;
    }

    const RigidBody& BodyOf(Entity e) { return ECS.GetComponent<RigidBody>(e); }

    bool IsCircle(Entity e) { return BodyOf(e).Shape.GetShapeType() == CircleShape; }

    // Local size of a polygon body (unrotated): max |x| and |y| of its points
    Vec2 HalfExtent(Entity e)
    {
        Vec2 half(0.0f, 0.0f);
        for (const Vec2& p : BodyOf(e).Shape.LocalSpacePoints)
            half = Vec2(std::max(half.X, std::fabs(p.X)), std::max(half.Y, std::fabs(p.Y)));
        return half;
    }

    bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }
} // namespace

TEST_CASE("Colliders: Auto follows the object, other shapes are built from its footprint")
{
    SceneEditor& editor = NewEditor();
    Entity box = editor.Place(ObjectKind::Rectangle, {0, 0, 0}, Brush(4.0f, 2.0f, BodyType::Dynamic));
    Entity ball = editor.Place(ObjectKind::Circle, {6, 0, 0}, Brush(2.0f, 2.0f, BodyType::Static));
    Entity hex = editor.Place(ObjectKind::Polygon, {-6, 0, 0}, Brush(2.0f, 2.0f, BodyType::Dynamic));
    // Auto: as before
    CHECK(!IsCircle(box));
    CHECK_EQ(BodyOf(box).Shape.LocalSpacePoints.size(), 4u);
    CHECK(IsCircle(ball));
    CHECK(Near(BodyOf(ball).Shape.Radius, 1.0f));
    CHECK_EQ(BodyOf(hex).Shape.LocalSpacePoints.size(), 6u);
    CHECK(SceneObjects::EffectiveColliderShape(box) == ColliderShapeType::Box);
    CHECK(SceneObjects::EffectiveColliderShape(ball) == ColliderShapeType::Circle);
    CHECK(SceneObjects::EffectiveColliderShape(hex) == ColliderShapeType::Polygon);

    // The rectangle as a circle around it, the ball as a box, bigger
    REQUIRE(editor.SetColliderShape(box, ColliderShapeType::Circle, 1.0f));
    CHECK(IsCircle(box));
    CHECK(Near(BodyOf(box).Shape.Radius, 2.0f));
    REQUIRE(editor.SetColliderShape(ball, ColliderShapeType::Box, 1.5f));
    CHECK(!IsCircle(ball));
    CHECK(Near(HalfExtent(ball).X, 1.5f));
    CHECK(Near(HalfExtent(ball).Y, 1.5f));
    // The body type is kept
    CHECK(BodyOf(ball).IsStatic());
    CHECK(!BodyOf(box).IsStatic());
    // A circle's outline as a polygon
    REQUIRE(editor.SetColliderShape(ball, ColliderShapeType::Polygon, 1.0f));
    CHECK(BodyOf(ball).Shape.LocalSpacePoints.size() > 8u);
    // Half size
    REQUIRE(editor.SetColliderShape(hex, ColliderShapeType::Auto, 0.5f));
    CHECK_EQ(BodyOf(hex).Shape.LocalSpacePoints.size(), 6u);
    CHECK(HalfExtent(hex).X <= 0.5f + 1e-3f);
    // Scale is kept in range, bad values are refused
    REQUIRE(editor.SetColliderShape(hex, ColliderShapeType::Auto, 100.0f));
    CHECK(Near(SceneObjects::ColliderShapeOf(hex).Scale, 5.0f));
    CHECK(!editor.SetColliderShape(hex, ColliderShapeType::Box, std::nanf("")));

    // Models: a circle by default, a box of the same size when asked
    PlaceSettings model = Brush(2.0f, 2.0f, BodyType::Dynamic);
    model.Model = "Box";
    Entity crate = editor.Place(ObjectKind::Model, {0, 0, 6}, model);
    REQUIRE(crate != NULL_ENTITY);
    CHECK(IsCircle(crate));
    float radius = BodyOf(crate).Shape.Radius;
    REQUIRE(editor.SetColliderShape(crate, ColliderShapeType::Box, 1.0f));
    CHECK(Near(HalfExtent(crate).X, radius));

    // Changing the object's size rebuilds the body with the chosen shape
    REQUIRE(editor.SetSize(box, 6.0f, 2.0f));
    CHECK(IsCircle(box));
    CHECK(Near(BodyOf(box).Shape.Radius, 3.0f));
}

TEST_CASE("Colliders: the choice needs a body, is edited in the RigidBody section and undone")
{
    SceneEditor& editor = NewEditor();
    Entity plain = editor.Place(ObjectKind::Rectangle, {0, 0, 0}, Brush(2.0f, 1.0f, BodyType::None));
    CHECK(!editor.SetColliderShape(plain, ColliderShapeType::Circle, 1.0f));
    CHECK(!ECS.HasComponent<ColliderShape>(plain));

    Entity e = editor.Place(ObjectKind::Rectangle, {3, 0, 0}, Brush(2.0f, 1.0f, BodyType::Dynamic));
    REQUIRE(editor.SetColliderShape(e, ColliderShapeType::Circle, 2.0f));
    // Not a component of its own: part of the RigidBody
    for (const auto& view : editor.ComponentsOf(e))
        CHECK(view.Name != std::string(SceneEditor::COMPONENT_COLLIDER));
    for (const std::string& name : editor.AddableComponents(e))
        CHECK(name != std::string(SceneEditor::COMPONENT_COLLIDER));
    CHECK(!editor.AddComponent(e, SceneEditor::COMPONENT_COLLIDER));

    // One undo step, the body back as it was
    REQUIRE(editor.Undo());
    CHECK(!IsCircle(e));
    CHECK(!ECS.HasComponent<ColliderShape>(e));
    REQUIRE(editor.Redo());
    CHECK(IsCircle(e));
    CHECK(Near(BodyOf(e).Shape.Radius, 2.0f));
    // Setting the same again is not an edit
    CHECK(editor.SetColliderShape(e, ColliderShapeType::Circle, 2.0f));
    REQUIRE(editor.Undo());
    CHECK(!IsCircle(e));
    REQUIRE(editor.Redo());

    // Edited as a reflected field (scripts / the generic API): the body follows
    REQUIRE(editor.SetField(e, "Collider", "Shape", Reflection::FieldValue(std::int64_t(1))));
    CHECK(!IsCircle(e));
    CHECK(Near(HalfExtent(e).X, 2.0f));

    // Removing the body removes its collider shape
    REQUIRE(editor.RemoveComponent(e, SceneEditor::COMPONENT_RIGIDBODY));
    CHECK(!ECS.HasComponent<ColliderShape>(e));
    REQUIRE(editor.AddComponent(e, SceneEditor::COMPONENT_RIGIDBODY));
    CHECK(!IsCircle(e));
    CHECK(Near(HalfExtent(e).X, 1.0f));
}

TEST_CASE("Colliders: copies, prefabs and scene files keep the collider shape")
{
    SceneEditor& editor = NewEditor();
    Entity e = editor.Place(ObjectKind::Rectangle, {0, 0, 0}, Brush(2.0f, 1.0f, BodyType::Dynamic));
    REQUIRE(editor.SetColliderShape(e, ColliderShapeType::Circle, 1.5f));
    const float radius = BodyOf(e).Shape.Radius;

    Entity copy = editor.Duplicate(e);
    REQUIRE(copy != NULL_ENTITY);
    CHECK(IsCircle(copy));
    CHECK(Near(BodyOf(copy).Shape.Radius, radius));
    CHECK(SceneObjects::ColliderShapeOf(copy).Type == ColliderShapeType::Circle);

    Prefab::Data prefab = Prefab::Capture(e, "Round");
    Entity instance = editor.PlacePrefab(prefab, {5, 0, 5});
    REQUIRE(instance != NULL_ENTITY);
    CHECK(IsCircle(instance));
    CHECK(Near(BodyOf(instance).Shape.Radius, radius));

    std::string name = editor.NameOf(e);
    std::string path = (std::filesystem::temp_directory_path() / "ubisoft_next_colliders.scene").string();
    REQUIRE(static_cast<bool>(editor.SaveScene(path, "Colliders")));
    NewEditor();
    REQUIRE(static_cast<bool>(editor.LoadScene(path)));
    Entity loaded = SceneObjects::FindByName(name);
    REQUIRE(loaded != NULL_ENTITY);
    CHECK(IsCircle(loaded));
    CHECK(Near(BodyOf(loaded).Shape.Radius, radius));
    CHECK(Near(SceneObjects::ColliderShapeOf(loaded).Scale, 1.5f));
    std::filesystem::remove(path);
}

TEST_CASE("Collider gizmos: a wire prism where the physics system puts the body")
{
    SceneEditor& editor = NewEditor();
    PlaceSettings brush = Brush(4.0f, 2.0f, BodyType::Dynamic);
    brush.YawDegrees = 30.0f;
    brush.Thickness = 0.5f;
    Entity box = editor.Place(ObjectKind::Rectangle, {2, 0, 3}, brush);
    Entity ball = editor.Place(ObjectKind::Circle, {-4, 0, 0}, Brush(2.0f, 2.0f, BodyType::Static));
    Entity trigger = editor.Place(ObjectKind::Circle, {-8, 0, 0}, Brush(2.0f, 2.0f, BodyType::Trigger));
    Entity plain = editor.Place(ObjectKind::Rectangle, {8, 0, 8}, Brush(1.0f, 1.0f, BodyType::None));
    CHECK(PhysicsGizmos::ColliderLines(plain).empty());

    // A box: 4 edges at the base, 4 at the top, 4 upright
    std::vector<PhysicsGizmos::Line> lines = PhysicsGizmos::ColliderLines(box);
    REQUIRE(lines.size() == 12u);
    float low = 1e9f, high = -1e9f;
    for (const auto& line : lines)
    {
        low = std::min({low, line.A.Y, line.B.Y});
        high = std::max({high, line.A.Y, line.B.Y});
    }
    CHECK(Near(high - low, 0.5f));
    // A circle: 24 sides twice, 4 upright, one radius showing its turn
    CHECK_EQ(PhysicsGizmos::ColliderLines(ball, 24).size(), 24u * 2u + 4u + 1u);

    // The base corners are the body's corners after a physics step
    ECS.FlushECS();
    PhysicsSystem physics;
    physics.Update(16.5f);
    REQUIRE(Near(BodyOf(box).Position.X, 2.0f));
    std::vector<Vec3> outline = SceneObjects::ColliderOutline(box);
    const RigidBody& body = BodyOf(box);
    REQUIRE(outline.size() == body.Shape.PolygonPoints.size());
    for (std::size_t i = 0; i < outline.size(); ++i)
    {
        CHECK(Near(outline[i].X, body.Shape.PolygonPoints[i].X));
        CHECK(Near(outline[i].Z, body.Shape.PolygonPoints[i].Y));
    }
    // Turned with the object (30 degrees: not axis aligned)
    CHECK(!Near(outline[0].X, outline[1].X) && !Near(outline[0].Z, outline[1].Z));

    // Colours: what the body is, red when touching while simulating
    auto same = [](PhysicsGizmos::GizmoColor a, PhysicsGizmos::GizmoColor b) {
        return a.R == b.R && a.G == b.G && a.B == b.B;
    };
    CHECK(same(PhysicsGizmos::ColorOf(box, false), PhysicsGizmos::DYNAMIC_COLOR));
    CHECK(same(PhysicsGizmos::ColorOf(ball, false), PhysicsGizmos::STATIC_COLOR));
    CHECK(same(PhysicsGizmos::ColorOf(trigger, false), PhysicsGizmos::TRIGGER_COLOR));
    ECS.GetComponent<RigidBody>(box).IsIntersecting = true;
    CHECK(same(PhysicsGizmos::ColorOf(box, true), PhysicsGizmos::TOUCHING_COLOR));
    CHECK(same(PhysicsGizmos::ColorOf(box, false), PhysicsGizmos::DYNAMIC_COLOR));
}
