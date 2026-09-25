//---------------------------------------------------------------------------------
// HierarchyTests.cpp
//---------------------------------------------------------------------------------
//
// The Transform hierarchy (parent / children) and empty objects: world space
// math, SceneObjects::SetParent, repairs of broken files, rendering / picking /
// physics of children, and the editor's hierarchy operations
//
#include "AppStub.h"
#include "RenderConstants.h"
#include "RigidBody.h"
#include "VertexBuffer.h"
#include "SceneEditor.h"
#include "Scripts/Components/GameComponents.h"
#include "Serialization/TextArchive.h"
#include "WorldFixture.h"

#include <cmath>
#include <filesystem>

using Editor::ObjectKind;
using Editor::SceneEditor;
using SceneObjects::BodyType;

namespace
{
    bool Near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

    bool Near(const Vec3& a, const Vec3& b, float eps = 1e-4f)
    {
        return Near(a.X, b.X, eps) && Near(a.Y, b.Y, eps) && Near(a.Z, b.Z, eps);
    }

    Entity Box(const std::string& name, const Vec3& at, float yaw = 0.0f, BodyType body = BodyType::None)
    {
        SceneObjects::ShapeDesc desc = Fixture::ShapeOf(name, Shape2DType::Rectangle, at, body);
        desc.YawDegrees = yaw;
        return SceneObjects::CreateShape(desc);
    }

    Transform& T(Entity e) { return ECS.GetComponent<Transform>(e); }

    // Centre of the object's geometry in the renderer's vertex buffer (x, z)
    Vec3 MeshCentre(Entity e)
    {
        auto range = ECS.GetResource<RenderConstants>()->EntityToVertexRange.at(e);
        const auto& vertices = ECS.GetResource<VertexBuffer>()->Buffer;
        Vec3 sum(0, 0, 0);
        for (int i = range.first; i < range.second; ++i)
            sum += vertices[i].Position;
        sum = sum * (1.0f / static_cast<float>(range.second - range.first));
        return Vec3(sum.X, 0.0f, sum.Z);
    }

    // Editor on a fresh world holding just the field
    SceneEditor& NewEditor()
    {
        static SceneEditor editor;
        Fixture::FreshWorld();
        editor = SceneEditor{};
        editor.NewScene();
        return editor;
    }
} // namespace

//-----------------------------------------------------------------------------
// World space
//-----------------------------------------------------------------------------

TEST_CASE("Hierarchy: a child's world pose is its local pose through every parent")
{
    Fixture::FreshWorld();
    Entity a = Box("A", {3, 0, 8}, 90.0f);
    T(a).Scale(2.0f);
    Entity b = Box("B", {0, 0, 0});
    Entity c = Box("C", {0, 0, 0});
    // Link with the local values as they are (the low level call)
    T(b).SetParentEntity(a, b);
    T(c).SetParentEntity(b, c);
    T(b).SetLocalPose({1, 0, 0}, Quat(Vec3(0, 1, 0), 0.5f), {1, 1, 1});
    T(c).SetLocalPose({0, 0.5f, 2}, Quat(Vec3(0, 1, 0), -0.25f), {0.5f, 0.5f, 0.5f});

    // Same as multiplying the Affine matrices down the chain
    Vec3 viaMatrices = T(a).Affine * (T(b).Affine * (T(c).Affine * Vec3(0, 0, 0)));
    CHECK(Near(T(c).GetWorldPosition(), viaMatrices));
    Vec3 point(0.3f, 0.1f, -0.7f);
    Vec3 expected = T(a).Affine * (T(b).Affine * (T(c).Affine * point));
    CHECK(Near(T(c).GetWorldTransform().TransformVec3(point), expected));
    // Rotations add up (yaw 90 + 0.5 rad - 0.25 rad)
    float yaw = 90.0f + (0.5f - 0.25f) * 57.2957795f;
    CHECK(Near(SceneObjects::GetYaw(c), yaw, 0.02f));
    CHECK(Near(T(c).GetWorldTransform().LocalScale.X, 1.0f));

    // The parent's first child offset: A turned 90 degrees, scale 2
    CHECK(Near(SceneObjects::GetPosition(b), T(a).Affine * Vec3(1, 0, 0)));
    // A root is its own world transform
    CHECK(Near(SceneObjects::GetPosition(a), Vec3(3, 0, 8)));
}

TEST_CASE("Hierarchy: SetParent keeps the child where it is, parents carry children")
{
    Fixture::FreshWorld();
    Entity parent = Box("Parent", {2, 0, 1}, 90.0f);
    T(parent).Scale(2.0f);
    Entity child = Box("Child", {5, 0, 5}, 45.0f);

    REQUIRE(SceneObjects::SetParent(child, parent));
    CHECK_EQ(SceneObjects::GetParent(child), parent);
    CHECK(SceneObjects::GetChildren(parent) == std::vector<Entity>{child});
    CHECK(Near(SceneObjects::GetPosition(child), Vec3(5, 0, 5)));
    CHECK(Near(SceneObjects::GetYaw(child), 45.0f, 0.02f));
    CHECK(Near(T(child).GetWorldTransform().LocalScale.X, 1.0f));
    // Stored relative to the parent
    CHECK(Near(T(child).LocalScale.X, 0.5f));
    CHECK(!Near(T(child).LocalPosition, Vec3(5, 0, 5)));

    // Moving / turning the parent carries the child
    SceneObjects::SetPosition(parent, {4, 0, 1});
    CHECK(Near(SceneObjects::GetPosition(child), Vec3(7, 0, 5)));
    SceneObjects::SetYaw(parent, 180.0f);
    // The child turned 90 degrees around the parent: (3, 4) -> (4, -3)
    CHECK(Near(SceneObjects::GetPosition(child), Vec3(8, 0, -2), 1e-3f));
    CHECK(Near(SceneObjects::GetYaw(child), 135.0f, 0.02f));

    // World setters on the child
    SceneObjects::SetPosition(child, {1, 0, 1});
    CHECK(Near(SceneObjects::GetPosition(child), Vec3(1, 0, 1)));
    SceneObjects::SetYaw(child, 10.0f);
    CHECK(Near(SceneObjects::GetYaw(child), 10.0f, 0.02f));
    CHECK(Near(SceneObjects::GetPosition(child), Vec3(1, 0, 1)));

    // Back to the top level: same place in the world
    REQUIRE(SceneObjects::SetParent(child, NULL_ENTITY));
    CHECK_EQ(SceneObjects::GetParent(child), NULL_ENTITY);
    CHECK(SceneObjects::GetChildren(parent).empty());
    CHECK(Near(SceneObjects::GetPosition(child), Vec3(1, 0, 1)));
    CHECK(Near(T(child).LocalScale.X, 1.0f));
}

TEST_CASE("Hierarchy: loops and self parenting are refused")
{
    Fixture::FreshWorld();
    Entity a = Box("A", {0, 0, 0});
    Entity b = Box("B", {1, 0, 0});
    Entity c = Box("C", {2, 0, 0});
    REQUIRE(SceneObjects::SetParent(b, a));
    REQUIRE(SceneObjects::SetParent(c, b));
    CHECK(SceneObjects::IsAncestor(a, c));
    CHECK(!SceneObjects::IsAncestor(c, a));
    CHECK(!SceneObjects::SetParent(a, c));
    CHECK(!SceneObjects::SetParent(a, a));
    CHECK(!SceneObjects::SetParent(a, 4000));
    CHECK_EQ(SceneObjects::GetParent(a), NULL_ENTITY);
    // Moving a subtree under another branch
    Entity d = Box("D", {3, 0, 0});
    REQUIRE(SceneObjects::SetParent(b, d));
    CHECK(SceneObjects::GetChildren(a).empty());
    CHECK(SceneObjects::IsAncestor(d, c));
    CHECK(Near(SceneObjects::GetPosition(c), Vec3(2, 0, 0)));
}

TEST_CASE("Hierarchy: destroying detaches from the parent and takes the children")
{
    Fixture::FreshWorld();
    Entity a = Box("A", {0, 0, 0});
    Entity b = Box("B", {1, 0, 0});
    Entity c = Box("C", {2, 0, 0});
    Entity d = Box("D", {3, 0, 0});
    REQUIRE(SceneObjects::SetParent(b, a));
    REQUIRE(SceneObjects::SetParent(c, b));
    REQUIRE(SceneObjects::SetParent(d, a));
    SceneObjects::Destroy(d);
    CHECK(SceneObjects::GetChildren(a) == std::vector<Entity>{b});
    SceneObjects::Destroy(a);
    ECS.FlushECS();
    CHECK(!ECS.IsEntityAlive(a));
    CHECK(!ECS.IsEntityAlive(b));
    CHECK(!ECS.IsEntityAlive(c));
    // A child destroyed directly through the ECS does not break its parent
    Entity p = Box("P", {0, 0, 0});
    Entity q = Box("Q", {1, 0, 0});
    REQUIRE(SceneObjects::SetParent(q, p));
    ECS.DestroyEntity(q);
    ECS.FlushECS();
    SceneObjects::SetPosition(p, {2, 0, 2});
    CHECK(SceneObjects::GetChildren(p).empty());
    CHECK_EQ(SceneObjects::RepairHierarchy(), 1);
}

TEST_CASE("Hierarchy: broken links are repaired, loops in files never hang")
{
    Fixture::FreshWorld();
    Entity a = Box("A", {0, 0, 0});
    Entity b = Box("B", {1, 0, 0});
    Entity c = Box("C", {2, 0, 0});
    REQUIRE(SceneObjects::SetParent(b, a));
    CHECK_EQ(SceneObjects::RepairHierarchy(), 0);

    // A loop, a dead parent, a child listed twice / by the wrong parent
    T(a).Parent = b;
    T(b).Children.push_back(a);
    T(c).Parent = 4321;
    T(c).Children = {b, b};
    CHECK(SceneObjects::RepairHierarchy() > 0);
    CHECK_EQ(SceneObjects::RepairHierarchy(), 0);
    CHECK(!(SceneObjects::IsAncestor(a, b) && SceneObjects::IsAncestor(b, a)));
    CHECK_EQ(SceneObjects::GetParent(c), NULL_ENTITY);
    CHECK(SceneObjects::GetChildren(c).empty());

    // The same from a hand edited text scene: loaded, then repaired
    Fixture::FreshWorld();
    Entity p = Box("P", {0, 0, 0});
    Entity q = Box("Q", {1, 0, 0});
    REQUIRE(SceneObjects::SetParent(q, p));
    std::string text = Fixture::Serializer().SaveText(ECS, {});
    // P's record starts with its parent (0 = none): make Q its parent
    std::string from = std::to_string(p) + ": 0 [1] " + std::to_string(q) + " ";
    std::size_t at = text.find(from);
    REQUIRE(at != std::string::npos);
    text.replace(at, from.size(), std::to_string(p) + ": " + std::to_string(q) + " [1] " + std::to_string(q) + " ");
    Fixture::FreshWorld();
    Serialization::LoadResult loaded =
            Fixture::Serializer().Load(ECS, std::vector<std::uint8_t>(text.begin(), text.end()));
    REQUIRE(loaded.Success);
    CHECK(!(SceneObjects::IsAncestor(p, q) && SceneObjects::IsAncestor(q, p)));
    CHECK_EQ(SceneObjects::RepairHierarchy(), 0);
    TestEnvironment::RunFrame(16.0f);
}

TEST_CASE("Hierarchy: children are drawn, picked and simulated where they are in the world")
{
    Fixture::FreshWorld();
    Entity parent = Box("Parent", {4, 0, 2}, 90.0f);
    Entity child = Box("Child", {6, 0, 2}, 0.0f, BodyType::Static);
    REQUIRE(SceneObjects::SetParent(child, parent));
    SceneObjects::SetPosition(parent, {0, 0, 0});
    Vec3 world = SceneObjects::GetPosition(child);
    CHECK(Near(world, Vec3(2, 0, 0)));

    // Picking and outlines use the world frame
    CHECK(SceneObjects::Contains(child, world));
    CHECK(!SceneObjects::Contains(child, {6, 0, 2}));
    std::vector<Vec3> outline = SceneObjects::WorldOutline(child);
    REQUIRE(!outline.empty());
    Vec3 centre(0, 0, 0);
    for (const Vec3& p : outline)
        centre += p;
    centre = centre * (1.0f / outline.size());
    CHECK(Near(Vec3(centre.X, 0, centre.Z), Vec3(2, 0, 0), 1e-3f));

    // The physics body follows the world position
    TestEnvironment::RunFrame(16.0f);
    const RigidBody& body = ECS.GetComponent<RigidBody>(child);
    CHECK(Near(body.Position.X, 2.0f, 1e-3f));
    CHECK(Near(body.Position.Y, 0.0f, 1e-3f));
    // and so does the render mesh (world transform of the child)
    CHECK(Near(T(child).GetWorldTransform().TransformVec3({0, 0, 0}), Vec3(2, 0, 0)));
}

TEST_CASE("Hierarchy: empty objects are only a transform")
{
    Fixture::FreshWorld();
    Entity e = SceneObjects::CreateEmpty("Spawn", {1, 0, 2}, 30.0f);
    CHECK(SceneObjects::IsEmpty(e));
    CHECK(!SceneObjects::IsEmpty(Box("S", {0, 0, 0})));
    CHECK(ECS.HasComponent<Transform>(e));
    CHECK(ECS.HasComponent<SceneObject>(e));
    CHECK(!ECS.HasComponent<Shape2D>(e));
    CHECK(!ECS.HasComponent<Mesh>(e));
    CHECK(Near(SceneObjects::GetYaw(e), 30.0f, 0.02f));
    CHECK(SceneObjects::WorldOutline(e).empty());
    CHECK(SceneObjects::Contains(e, {1.3f, 0, 2}));
    CHECK(!SceneObjects::Contains(e, {2, 0, 2}));
    // Saved and loaded like any object, rendered as nothing
    Fixture::WorldImage before = Fixture::Capture();
    std::vector<std::uint8_t> bytes = Fixture::Serializer().Save(ECS, {});
    Fixture::FreshWorld();
    REQUIRE(Fixture::Serializer().Load(ECS, bytes).Success);
    CHECK_SAME_WORLD(before, Fixture::Capture());
    TestEnvironment::RunFrame(16.0f);
    CHECK(SceneObjects::IsEmpty(SceneObjects::FindByName("Spawn")));
}

//-----------------------------------------------------------------------------
// Editor
//-----------------------------------------------------------------------------

TEST_CASE("Editor hierarchy: parent objects, with undo, never the field")
{
    SceneEditor& editor = NewEditor();
    Entity field = editor.Objects()[0];
    Entity camera = editor.GameCameraObject();
    Entity light = editor.LightObject();
    Entity a = editor.Place(ObjectKind::Rectangle, {0, 0, 0});
    Entity b = editor.Place(ObjectKind::Circle, {3, 0, 0});
    Entity c = editor.Place(ObjectKind::Empty, {-3, 0, 2});
    CHECK(editor.KindOf(c) == ObjectKind::Empty);
    CHECK(editor.RootObjects() == (std::vector<Entity>{field, camera, light, a, b, c}));

    std::size_t undo = editor.UndoCount();
    REQUIRE(editor.SetParent(b, a));
    REQUIRE(editor.SetParent(c, b));
    CHECK_EQ(editor.UndoCount(), undo + 2);
    CHECK_EQ(editor.ParentOf(c), b);
    CHECK(editor.ChildrenOf(a) == std::vector<Entity>{b});
    CHECK(editor.RootObjects() == (std::vector<Entity>{field, camera, light, a}));
    // Same parent again: no change, no undo step
    REQUIRE(editor.SetParent(b, a));
    CHECK_EQ(editor.UndoCount(), undo + 2);
    // Refused: loops, the field on either side
    CHECK(!editor.SetParent(a, c));
    CHECK(!editor.SetParent(a, field));
    CHECK(!editor.SetParent(field, a));
    CHECK_EQ(editor.UndoCount(), undo + 2);

    // Components tab summary
    std::vector<Editor::ComponentView> ofB = editor.ComponentsOf(b);
    std::vector<Editor::ComponentView> ofA = editor.ComponentsOf(a);
    CHECK_EQ(ofB[0].Summary, "in " + editor.NameOf(a));
    CHECK_EQ(ofA[0].Summary, std::string("1 children"));

    // Moving the parent moves the children (world positions)
    REQUIRE(editor.Move(a, {2, 0, 0}));
    CHECK(Near(SceneObjects::GetPosition(b), Vec3(5, 0, 0)));
    CHECK(Near(SceneObjects::GetPosition(c), Vec3(-1, 0, 2)));
    // Moving a child moves it alone
    REQUIRE(editor.Move(c, {0, 0, 0}));
    CHECK(Near(SceneObjects::GetPosition(b), Vec3(5, 0, 0)));

    REQUIRE(editor.Undo());
    REQUIRE(editor.Undo());
    CHECK(Near(SceneObjects::GetPosition(b), Vec3(3, 0, 0)));
    REQUIRE(editor.Undo());
    CHECK_EQ(editor.ParentOf(c), NULL_ENTITY);
    REQUIRE(editor.Redo());
    CHECK_EQ(editor.ParentOf(c), b);
}

TEST_CASE("Editor hierarchy: delete and duplicate whole branches")
{
    SceneEditor& editor = NewEditor();
    Entity a = editor.Place(ObjectKind::Rectangle, {0, 0, 0});
    Entity b = editor.Place(ObjectKind::Circle, {2, 0, 0});
    Entity c = editor.AddEmpty({2, 0, 3}, b);
    REQUIRE(c != NULL_ENTITY);
    REQUIRE(editor.SetParent(b, a));
    ECS.AddComponent<Waypoint>(a, Waypoint{c, 0.0f});

    // Duplicate: the copy has copies of the children, laid out the same way
    Entity copy = editor.Duplicate(a);
    REQUIRE(copy != NULL_ENTITY);
    std::vector<Entity> copyChildren = editor.ChildrenOf(copy);
    REQUIRE(copyChildren.size() == 1u);
    Entity copyB = copyChildren[0];
    REQUIRE(editor.ChildrenOf(copyB).size() == 1u);
    Entity copyC = editor.ChildrenOf(copyB)[0];
    CHECK(editor.KindOf(copyC) == ObjectKind::Empty);
    Vec3 shift = SceneObjects::GetPosition(copy) - SceneObjects::GetPosition(a);
    CHECK(Near(SceneObjects::GetPosition(copyB) - SceneObjects::GetPosition(b), shift));
    CHECK(Near(SceneObjects::GetPosition(copyC) - SceneObjects::GetPosition(c), shift));
    // A duplicated child stays under the same parent
    Entity copyOfB = editor.Duplicate(b);
    CHECK_EQ(editor.ParentOf(copyOfB), a);

    // Delete: the branch goes, references to it are cleared, one undo step
    std::size_t objects = editor.Objects().size();
    editor.Select(c);
    REQUIRE(editor.Remove(b));
    CHECK(!ECS.IsEntityAlive(b));
    CHECK(!ECS.IsEntityAlive(c));
    CHECK_EQ(editor.Selected(), NULL_ENTITY);
    CHECK_EQ(ECS.GetComponent<Waypoint>(a).Next, NULL_ENTITY);
    CHECK_EQ(editor.Objects().size(), objects - 2);
    REQUIRE(editor.Undo());
    CHECK_EQ(editor.Objects().size(), objects);
    CHECK_EQ(editor.ParentOf(c), b);
}

TEST_CASE("Editor hierarchy: empties are picked, scaled and saved with their children")
{
    SceneEditor& editor = NewEditor();
    Entity big = editor.Place(ObjectKind::Rectangle, {0, 0, 0});
    REQUIRE(editor.SetSize(big, 6.0f, 6.0f));
    std::size_t undo = editor.UndoCount();
    Entity group = editor.AddEmpty({1, 0, 1});
    CHECK_EQ(editor.UndoCount(), undo + 1);
    CHECK_EQ(editor.Selected(), group);
    // The small cross wins over the large shape around it
    CHECK_EQ(editor.Pick({1.1f, 0, 1}), group);
    CHECK_EQ(editor.Pick({2.5f, 0, 2.5f}), big);

    // Scaling a group scales its children around it
    Entity child = editor.Place(ObjectKind::Circle, {3, 0, 1});
    REQUIRE(editor.SetParent(child, group));
    REQUIRE(editor.SetSize(group, 2.0f, 2.0f));
    CHECK(Near(SceneObjects::GetPosition(child), Vec3(5, 0, 1)));

    // One undo step removes an empty added under an object
    std::size_t before = editor.UndoCount();
    Entity marker = editor.AddEmpty({0, 0, 0}, child);
    CHECK_EQ(editor.ParentOf(marker), child);
    CHECK_EQ(editor.UndoCount(), before + 1);
    REQUIRE(editor.Undo());
    CHECK(!ECS.IsEntityAlive(marker) || !editor.IsObject(marker));
    REQUIRE(editor.Redo());

    // Saved and loaded with the hierarchy
    std::string path = (std::filesystem::temp_directory_path() / "ubisoft_next_hierarchy.ubsave").string();
    REQUIRE(editor.SaveScene(path, "hierarchy").Success);
    Fixture::WorldImage saved = Fixture::Capture();
    SceneEditor& other = NewEditor();
    REQUIRE(other.LoadScene(path).Success);
    CHECK_SAME_WORLD(saved, Fixture::Capture());
    Entity loadedGroup = SceneObjects::FindByName(editor.NameOf(group));
    REQUIRE(loadedGroup != NULL_ENTITY);
    CHECK(other.KindOf(loadedGroup) == ObjectKind::Empty);
    REQUIRE(other.ChildrenOf(loadedGroup).size() == 1u);
    CHECK(Near(SceneObjects::GetPosition(other.ChildrenOf(loadedGroup)[0]), Vec3(5, 0, 1)));
    std::filesystem::remove(path);
}

TEST_CASE("Hierarchy: the sandbox scene's Orbit carries its moons around")
{
    std::vector<std::uint8_t> bytes;
    std::string error;
    REQUIRE(Serialization::WorldSerializer::ReadFile(GameManager::ScenePath("sandbox"), bytes, error));
    Fixture::FreshWorld();
    AppStub::Reset();
    REQUIRE(Fixture::Serializer().Load(ECS, bytes).Success);
    Entity orbit = SceneObjects::FindByName("Orbit");
    REQUIRE(orbit != NULL_ENTITY);
    CHECK(SceneObjects::IsEmpty(orbit));
    std::vector<Entity> moons = SceneObjects::GetChildren(orbit);
    REQUIRE(moons.size() == 2u);
    Vec3 centre = SceneObjects::GetPosition(orbit);
    Vec3 start = SceneObjects::GetPosition(moons[0]);

    TestEnvironment::RunFrames(50, 20.0f); // 1 s at 60 degrees per second
    Vec3 now = SceneObjects::GetPosition(moons[0]);
    CHECK(!Near(now, start, 0.1f));
    // Still 2 units from the centre, and the two moons opposite each other
    Vec3 d = now - centre;
    CHECK(Near(std::sqrt(d.X * d.X + d.Z * d.Z), 2.0f, 1e-3f));
    Vec3 other = SceneObjects::GetPosition(moons[1]) - centre;
    CHECK(Near(other, d * -1.0f, 1e-3f));
    CHECK(Near(SceneObjects::GetYaw(orbit), 60.0f, 1.5f));
    // The renderer draws the moons where they are
    for (Entity moon : moons)
    {
        Vec3 p = SceneObjects::GetPosition(moon);
        CHECK(Near(MeshCentre(moon), Vec3(p.X, 0.0f, p.Z), 0.02f));
    }
}
