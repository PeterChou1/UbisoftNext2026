//---------------------------------------------------------------------------------
// CameraTests.cpp
//---------------------------------------------------------------------------------
//
// The game camera is a scene object (Transform + GameCamera, SceneCamera.h):
// its view math, finding it, following it while a scene plays, and the
// editor's operations on it. Scenes without one use their settings' camera
//
#include "Camera.h"
#include "SceneEditor.h"
#include "WorldFixture.h"
#include "World/SceneCamera.h"

#include <cmath>
#include <filesystem>

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
} // namespace

TEST_CASE("Camera: the view math matches the old fixed camera and turns with the yaw")
{
    Fixture::FreshWorld();
    auto camera = ECS.GetResource<Camera>();

    // Default view = where SceneObjects::ApplyCamera puts the camera
    SceneCamera::View view;
    view.Target = {2, 0, -3};
    view.Distance = 25.0f;
    SceneObjects::ApplyCamera(*camera, view.Target, view.Distance);
    Vec3 old = camera->Position;
    CHECK(Near(SceneCamera::EyeOf(view), old, 0.01f));

    // Forward / right along the ground
    CHECK(Near(SceneCamera::Forward(0.0f), Vec3(0, 0, 1)));
    CHECK(Near(SceneCamera::Forward(90.0f), Vec3(1, 0, 0)));
    CHECK(Near(SceneCamera::Right(0.0f), Vec3(-1, 0, 0)));
    CHECK(Near(SceneCamera::Right(90.0f), Vec3(0, 0, 1)));

    // The eye is behind the target (opposite the forward direction), up by
    // the pitch, `Distance` away
    view.Target = {0, 1, 0};
    view.Yaw = 90.0f;
    view.Pitch = 30.0f;
    view.Distance = 10.0f;
    Vec3 eye = SceneCamera::EyeOf(view);
    CHECK(Near(eye, Vec3(-10.0f * std::cos(0.5236f), 1.0f + 5.0f, 0.0f), 0.01f));
    Vec3 offset = eye - view.Target;
    CHECK(Near(std::sqrt(offset.Dot(offset)), 10.0f));
    // Straight down
    view.Pitch = 89.0f;
    CHECK(SceneCamera::EyeOf(view).Y > 10.9f - 0.1f);

    // Apply places the renderer's camera and its field of view
    view.FieldOfView = 60.0f;
    SceneCamera::Apply(*camera, view);
    CHECK(Near(camera->Position, SceneCamera::EyeOf(view)));
    CHECK_EQ(camera->Fov, 60.0f);
    // The target is projected at the centre of the screen
    Vec2 center = camera->WorldPointToScreenSpace(view.Target);
    CHECK(Near(center.X, APP_VIRTUAL_WIDTH * 0.5f, 1.0f));
    CHECK(Near(center.Y, APP_VIRTUAL_HEIGHT * 0.5f, 1.0f));
    view.FieldOfView = 90.0f;
    SceneCamera::Apply(*camera, view);
}

TEST_CASE("Camera: the scene's camera object, or the settings when there is none")
{
    Fixture::FreshWorld();
    auto settings = ECS.GetResource<SceneSettings>();
    settings->CameraTarget = {1, 0, 2};
    settings->CameraDistance = 18.0f;
    // No camera object (scenes saved before cameras were objects)
    CHECK_EQ(SceneCamera::Find(), NULL_ENTITY);
    SceneCamera::View fallback = SceneCamera::Current();
    CHECK(Near(fallback.Target, Vec3(1, 0, 2)));
    CHECK_EQ(fallback.Distance, 18.0f);

    SceneCamera::View view;
    view.Target = {3, 0, -1};
    view.Yaw = 45.0f;
    view.Pitch = 40.0f;
    view.Distance = 12.0f;
    view.FieldOfView = 70.0f;
    Entity camera = SceneCamera::Create(view);
    CHECK_EQ(SceneCamera::Find(), camera);
    CHECK_EQ(ECS.GetComponent<SceneObject>(camera).Name, std::string("Main Camera"));
    CHECK_EQ(ECS.GetComponent<SceneObject>(camera).Tag, std::string("Camera"));
    SceneCamera::View read = SceneCamera::Current();
    CHECK(read == view);

    // Moving / rotating the object moves the view; as a child it follows
    // its parent
    Entity rig = SceneObjects::CreateEmpty("Rig", {10, 0, 0});
    SceneObjects::SetParent(camera, rig);
    SceneObjects::SetPosition(rig, {12, 0, 0});
    CHECK(Near(SceneCamera::Current().Target, Vec3(5, 0, -1)));
    SceneObjects::SetYaw(camera, 90.0f);
    CHECK(Near(SceneCamera::Current().Yaw, 90.0f));

    // SetView clamps the GameCamera's fields to their ranges
    view.Pitch = 120.0f;
    view.Distance = 0.0f;
    SceneCamera::SetView(camera, view);
    CHECK_EQ(ECS.GetComponent<GameCamera>(camera).Pitch, 89.0f);
    CHECK_EQ(ECS.GetComponent<GameCamera>(camera).Distance, 1.0f);

    // The first camera (lowest id) is the game camera
    Entity second = SceneCamera::Create(SceneCamera::View{}, "Other");
    CHECK_EQ(SceneCamera::Find(), camera);
    SceneObjects::Destroy(camera);
    ECS.FlushECS();
    CHECK_EQ(SceneCamera::Find(), second);
}

TEST_CASE("Camera: the follower re-applies the view only when the camera object changes")
{
    Fixture::FreshWorld();
    auto camera = ECS.GetResource<Camera>();
    Entity object = SceneCamera::Create(SceneCamera::View{});
    SceneCamera::Follower follower;
    follower.Update(*camera);
    Vec3 eye = SceneCamera::EyeOf(SceneCamera::Current());
    CHECK(Near(camera->Position, eye));

    // A script drives the renderer's camera itself: not overridden
    camera->SetPositionAndOrientation({50, 50, 50}, {0, 0, 0}, {0, 1, 0});
    follower.Update(*camera);
    CHECK(Near(camera->Position, Vec3(50, 50, 50)));

    // The camera object moves: the view follows
    SceneObjects::SetPosition(object, {4, 0, 4});
    follower.Update(*camera);
    CHECK(Near(camera->Position, SceneCamera::EyeOf(SceneCamera::Current())));
    ECS.GetComponent<GameCamera>(object).Distance = 40.0f;
    follower.Update(*camera);
    CHECK(Near(camera->Position, SceneCamera::EyeOf(SceneCamera::Current())));

    // Reset: applied again even without a change
    camera->SetPositionAndOrientation({50, 50, 50}, {0, 0, 0}, {0, 1, 0});
    follower.Reset();
    follower.Update(*camera);
    CHECK(Near(camera->Position, SceneCamera::EyeOf(SceneCamera::Current())));
}

TEST_CASE("Camera: a played scene looks through its camera object")
{
    Fixture::FreshWorld();
    SceneCamera::View view;
    view.Target = {-3, 0, 2};
    view.Yaw = 180.0f;
    view.Distance = 15.0f;
    SceneCamera::Create(view);
    TestEnvironment::Player().OnWorldRestored();
    auto camera = ECS.GetResource<Camera>();
    CHECK(Near(camera->Position, SceneCamera::EyeOf(view)));
    // Looking towards -Z: the eye is on the +Z side of the target
    CHECK(camera->Position.Z > view.Target.Z);

    // A script moving the camera object while playing moves the view
    Entity object = SceneCamera::Find();
    SceneObjects::SetPosition(object, {0, 0, 0});
    TestEnvironment::RunFrame(16.0f);
    view.Target = {0, 0, 0};
    CHECK(Near(camera->Position, SceneCamera::EyeOf(view)));
}

TEST_CASE("Camera: the editor's game camera operations")
{
    SceneEditor& editor = NewEditor();
    Entity camera = editor.GameCameraObject();
    REQUIRE(camera != NULL_ENTITY);
    CHECK(editor.IsCamera(camera));
    CHECK(!editor.IsCamera(editor.Objects()[0]));
    CHECK(SceneObjects::IsEmpty(camera));

    // "Game camera = view": one undo step; the settings follow for programs
    // that still read them
    std::size_t undo = editor.UndoCount();
    SceneCamera::View view;
    view.Target = {100, 0, 3};
    view.Yaw = 30.0f;
    view.Pitch = 50.0f;
    view.Distance = 22.0f;
    CHECK_EQ(editor.SetGameCamera(view), camera);
    CHECK_EQ(editor.UndoCount(), undo + 1);
    SceneCamera::View now = SceneCamera::Current();
    // Kept on the field
    CHECK_EQ(now.Target.X, 20.0f);
    CHECK(Near(now.Yaw, 30.0f));
    CHECK_EQ(now.Pitch, 50.0f);
    CHECK_EQ(ECS.GetResource<SceneSettings>()->CameraDistance, 22.0f);
    CHECK(editor.IsDirty());

    // SetCameraView on a given camera, one undo step
    view.Target = {1, 0, 1};
    REQUIRE(editor.SetCameraView(camera, view));
    CHECK(Near(SceneObjects::GetPosition(camera), Vec3(1, 0, 1)));
    CHECK(!editor.SetCameraView(editor.Objects()[0], view));
    REQUIRE(editor.Undo());
    CHECK_EQ(SceneObjects::GetPosition(editor.GameCameraObject()).X, 20.0f);

    // Without a camera, SetGameCamera creates one
    editor.Remove(editor.GameCameraObject());
    ECS.FlushECS();
    CHECK_EQ(editor.GameCameraObject(), NULL_ENTITY);
    Entity created = editor.SetGameCamera(view);
    CHECK(editor.IsCamera(created));
    CHECK_EQ(editor.NameOf(created), std::string("Main Camera"));

    // AddCamera: selected, named uniquely once there is a main camera
    Entity extra = editor.AddCamera({2, 0, 2});
    CHECK_EQ(editor.Selected(), extra);
    CHECK_EQ(editor.NameOf(extra), std::string("Camera"));
    CHECK_EQ(editor.GameCameraObject(), created);
    // Duplicating a camera copies its GameCamera
    ECS.GetComponent<GameCamera>(extra).FieldOfView = 55.0f;
    Entity copy = editor.Duplicate(extra);
    REQUIRE(copy != NULL_ENTITY);
    CHECK_EQ(ECS.GetComponent<GameCamera>(copy).FieldOfView, 55.0f);

    // Its fields are edited like any reflected component
    REQUIRE(editor.SetField(created, "GameCamera", "Pitch", 45.0));
    CHECK_EQ(ECS.GetComponent<GameCamera>(created).Pitch, 45.0f);
    REQUIRE(editor.SetField(created, "GameCamera", "Pitch", 200.0));
    CHECK_EQ(ECS.GetComponent<GameCamera>(created).Pitch, 89.0f);

    // Prefabs never take the scene's camera
    Entity box = editor.Place(ObjectKind::Rectangle, {-4, 0, -4});
    editor.Rename(box, "Box");
    Prefab::Data stage = editor.CaptureStage("group");
    for (const Prefab::Object& o : stage.Objects)
        CHECK(o.Tag != std::string("Camera"));
}

TEST_CASE("Camera: picking prefers objects to a camera's target, and its eye marker to anything")
{
    SceneEditor& editor = NewEditor();
    Entity camera = editor.GameCameraObject();
    Entity box = editor.Place(ObjectKind::Rectangle, {0, 0, 0});
    // Same spot as the camera's target: the box wins
    CHECK_EQ(editor.Pick({0, 0, 0}), box);
    auto ground = [](float) { return Vec3(0, 0, 0); };
    CHECK_EQ(editor.PickRay(ground), box);
    // Only the camera there: picked
    CHECK_EQ(editor.Pick({8, 0, 8}) == camera, false);
    editor.Move(camera, {8, 0, 8});
    CHECK_EQ(editor.Pick({8, 0, 8}), camera);

    // A ray through the eye marker picks the camera at the eye's height
    Vec3 eye = SceneCamera::EyeOf(SceneCamera::ViewOf(camera));
    auto throughEye = [&](float height) { return Vec3(eye.X, height, eye.Z); };
    float hitHeight = 0.0f;
    CHECK_EQ(editor.PickRay(throughEye, &hitHeight), camera);
    CHECK(Near(hitHeight, eye.Y));
}

TEST_CASE("Camera: saved and loaded with the scene (binary and text)")
{
    SceneEditor& editor = NewEditor();
    SceneCamera::View view;
    view.Target = {2, 1.5f, -3};
    view.Yaw = 135.0f;
    view.Pitch = 35.0f;
    view.Distance = 17.0f;
    view.FieldOfView = 75.0f;
    editor.SetGameCamera(view);
    for (auto format : {Serialization::SaveFormat::Binary, Serialization::SaveFormat::Text})
    {
        Serialization::WorldSerializer::SetFileFormat(format);
        std::string path = (std::filesystem::temp_directory_path() / "camera_scene.ubsave").string();
        REQUIRE(editor.SaveScene(path, "camera").Success);
        editor.NewScene();
        REQUIRE(editor.LoadScene(path).Success);
        REQUIRE(editor.GameCameraObject() != NULL_ENTITY);
        SceneCamera::View loaded = SceneCamera::Current();
        CHECK(Near(loaded.Target, view.Target));
        CHECK(Near(loaded.Yaw, view.Yaw, 0.01f));
        CHECK(Near(loaded.Pitch, view.Pitch));
        CHECK(Near(loaded.Distance, view.Distance));
        CHECK(Near(loaded.FieldOfView, view.FieldOfView));
        std::filesystem::remove(path);
    }
    Serialization::WorldSerializer::SetFileFormat(Serialization::SaveFormat::Binary);
}
