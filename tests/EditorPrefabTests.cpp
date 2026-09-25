//---------------------------------------------------------------------------------
// EditorPrefabTests.cpp
//---------------------------------------------------------------------------------
//
// Editor core for the context menus and the prefab editor: creating objects
// as children, placing / linking / resetting / updating / unpacking prefab
// instances, capturing a prefab stage, and suspending a scene while a prefab
// is edited
//
#include "SceneEditor.h"
#include "Scripts/Components/GameComponents.h"
#include "WorldFixture.h"

#include <cmath>

using Editor::ObjectKind;
using Editor::SceneEditor;

namespace
{
    bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

    bool Near(const Vec3& a, const Vec3& b, float eps = 1e-3f)
    {
        return Near(a.X, b.X, eps) && Near(a.Y, b.Y, eps) && Near(a.Z, b.Z, eps);
    }

    SceneEditor& NewEditor()
    {
        static SceneEditor editor;
        Fixture::FreshWorld();
        editor = SceneEditor{};
        editor.NewScene();
        return editor;
    }

    // A small prefab: root empty > block (rectangle) > dot (circle)
    Prefab::Data BlockPrefab(SceneEditor& editor)
    {
        editor.NewScene();
        Entity root = editor.AddEmpty({0, 0, 0});
        editor.Rename(root, "Block");
        Entity block = editor.Create(ObjectKind::Rectangle, {1, 0, 0}, root);
        editor.Rename(block, "Body");
        Entity dot = editor.Create(ObjectKind::Circle, {1, 0, 1}, block);
        editor.Rename(dot, "Dot");
        return editor.CaptureStage("block");
    }
} // namespace

TEST_CASE("Editor prefabs: create objects, as children when asked, one undo step each")
{
    SceneEditor& editor = NewEditor();
    Entity field = editor.Objects()[0];
    Entity parent = editor.Create(ObjectKind::Rectangle, {2, 0, 2});
    REQUIRE(parent != NULL_ENTITY);
    CHECK_EQ(editor.Selected(), parent);
    std::size_t undo = editor.UndoCount();
    Entity child = editor.Create(ObjectKind::Circle, {3, 0, 2}, parent);
    REQUIRE(child != NULL_ENTITY);
    CHECK_EQ(editor.UndoCount(), undo + 1);
    CHECK_EQ(editor.ParentOf(child), parent);
    CHECK(Near(SceneObjects::GetPosition(child), Vec3(3, 0, 2)));
    CHECK_EQ(editor.Selected(), child);
    // Every kind; models need a model; the field is never a parent
    CHECK(editor.Create(ObjectKind::Empty, {0, 0, 0}, child) != NULL_ENTITY);
    CHECK(editor.Create(ObjectKind::Model, {0, 0, 0}) == NULL_ENTITY);
    Editor::PlaceSettings box;
    box.Model = "Box";
    CHECK(editor.KindOf(editor.Create(ObjectKind::Model, {0, 0, 0}, parent, box)) == ObjectKind::Model);
    CHECK(editor.Create(ObjectKind::Circle, {0, 0, 0}, field) == NULL_ENTITY);
    REQUIRE(editor.Undo());
    REQUIRE(editor.Undo());
    CHECK(editor.ChildrenOf(child).empty());
}

TEST_CASE("Editor prefabs: instances are placed, linked, unpacked and reset")
{
    SceneEditor& editor = NewEditor();
    Prefab::Data prefab = BlockPrefab(editor);
    REQUIRE(prefab.Objects.size() == 3u);
    editor.NewScene();

    Entity holder = editor.Create(ObjectKind::Empty, {-5, 0, 0});
    Entity a = editor.PlacePrefab(prefab, {4, 0, 4});
    Entity b = editor.PlacePrefab(prefab, {-4, 0, 4}, holder);
    REQUIRE(a != NULL_ENTITY);
    REQUIRE(b != NULL_ENTITY);
    CHECK_EQ(editor.Selected(), b);
    CHECK_EQ(editor.PrefabOf(a), std::string("block"));
    CHECK_EQ(editor.ParentOf(b), holder);
    CHECK_EQ(editor.PrefabOf(holder), std::string());
    REQUIRE(editor.ChildrenOf(a).size() == 1u);

    // Reset: local changes to the instance go, its place stays
    REQUIRE(editor.SetYaw(a, 90.0f));
    Entity body = editor.ChildrenOf(a)[0];
    REQUIRE(editor.Move(body, {9, 0, 9}));
    REQUIRE(editor.Rename(a, "Special"));
    Vec3 where = SceneObjects::GetPosition(a);
    std::size_t undo = editor.UndoCount();
    Entity fresh = editor.ResetToPrefab(a, prefab);
    REQUIRE(fresh != NULL_ENTITY);
    CHECK_EQ(editor.UndoCount(), undo + 1);
    CHECK(Near(SceneObjects::GetPosition(fresh), where));
    CHECK(Near(SceneObjects::GetYaw(fresh), 90.0f, 0.05f));
    CHECK_EQ(editor.NameOf(fresh), std::string("Special"));
    Entity freshBody = editor.ChildrenOf(fresh)[0];
    // Body is 1 unit from the root again, turned with it (90 degrees)
    Vec3 d = SceneObjects::GetPosition(freshBody) - where;
    CHECK(Near(std::sqrt(d.X * d.X + d.Z * d.Z), 1.0f));
    CHECK(editor.ResetToPrefab(holder, prefab) == NULL_ENTITY);

    // Unpack: ordinary objects; link: an instance again
    REQUIRE(editor.UnpackPrefab(b));
    CHECK_EQ(editor.PrefabOf(b), std::string());
    CHECK(!editor.UnpackPrefab(b));
    REQUIRE(editor.LinkPrefab(b, "block"));
    CHECK_EQ(editor.PrefabOf(b), std::string("block"));
    REQUIRE(editor.Undo());
    CHECK_EQ(editor.PrefabOf(b), std::string());
}

TEST_CASE("Editor prefabs: editing a prefab updates every instance in one undo step")
{
    SceneEditor& editor = NewEditor();
    Prefab::Data prefab = BlockPrefab(editor);
    editor.NewScene();
    Entity a = editor.PlacePrefab(prefab, {4, 0, 4});
    Entity b = editor.PlacePrefab(prefab, {-4, 0, -4});
    Entity other = editor.Create(ObjectKind::Circle, {0, 0, 6});
    REQUIRE(editor.AddComponent(other, "Waypoint"));
    REQUIRE(editor.SetField(other, "Waypoint", "Next", static_cast<std::int64_t>(editor.ChildrenOf(a)[0])));

    // The prefab gets a third child
    Prefab::Data edited = prefab;
    Prefab::Object extra = edited.Objects[2];
    extra.Name = "Extra";
    extra.Parent = 0;
    edited.Objects.push_back(extra);

    std::size_t undo = editor.UndoCount();
    std::size_t objects = editor.Objects().size();
    CHECK_EQ(editor.UpdatePrefabInstances(edited), 2);
    CHECK_EQ(editor.UndoCount(), undo + 1);
    CHECK_EQ(editor.Objects().size(), objects + 2);
    for (Entity e : editor.Objects())
    {
        if (editor.PrefabOf(e) == "block")
            CHECK_EQ(editor.ChildrenOf(e).size(), size_t(2));
    }
    // A reference into a replaced instance is cleared, not left dangling
    CHECK_EQ(ECS.GetComponent<Waypoint>(other).Next, NULL_ENTITY);
    Prefab::Data unused;
    unused.Name = "nothing";
    unused.Objects.push_back(Prefab::Object{});
    CHECK_EQ(editor.UpdatePrefabInstances(unused), 0);
    REQUIRE(editor.Undo());
    CHECK_EQ(editor.Objects().size(), objects);
    CHECK(!ECS.IsEntityAlive(b) || editor.IsObject(b));
}

TEST_CASE("Editor prefabs: a stage with several top level objects is grouped under one root")
{
    SceneEditor& editor = NewEditor();
    Entity a = editor.Create(ObjectKind::Rectangle, {2, 0, 0});
    editor.Rename(a, "Left");
    Entity b = editor.Create(ObjectKind::Circle, {-2, 0, 3});
    editor.Rename(b, "Right");
    editor.Create(ObjectKind::Empty, {-2, 0, 3}, b);
    Prefab::Data group = editor.CaptureStage("pair");
    REQUIRE(group.Objects.size() == 4u);
    CHECK_EQ(group.Objects[0].Name, std::string("pair"));
    CHECK(group.Objects[0].Type == Prefab::ObjectType::Empty);
    CHECK_EQ(group.Objects[1].Parent, 0);
    CHECK(Near(group.Objects[1].Position, Vec3(2, 0, 0)));
    // Placed elsewhere: the layout around the root is kept
    Entity copy = editor.PlacePrefab(group, {10, 0, 10});
    std::vector<Entity> parts = editor.ChildrenOf(copy);
    REQUIRE(parts.size() == 2u);
    CHECK(Near(SceneObjects::GetPosition(parts[0]), Vec3(12, 0, 10)));
    CHECK(Near(SceneObjects::GetPosition(parts[1]), Vec3(8, 0, 13)));
}

TEST_CASE("Editor prefabs: a suspended scene comes back with its history")
{
    SceneEditor& editor = NewEditor();
    Entity a = editor.Create(ObjectKind::Rectangle, {2, 0, 0});
    editor.Create(ObjectKind::Circle, {4, 0, 0}, a);
    editor.Select(a);
    std::size_t undo = editor.UndoCount();
    Fixture::WorldImage before = Fixture::Capture();

    SceneEditor::Session session = editor.Suspend();
    // The prefab stage: anything happens here
    BlockPrefab(editor);
    editor.Create(ObjectKind::Triangle, {1, 0, 1});
    editor.Resume(session);

    CHECK_SAME_WORLD(before, Fixture::Capture());
    CHECK_EQ(editor.UndoCount(), undo);
    CHECK(editor.IsDirty());
    CHECK_EQ(editor.Selected(), a);
    REQUIRE(editor.Undo());
    CHECK(editor.ChildrenOf(a).empty());
}
