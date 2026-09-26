//---------------------------------------------------------------------------------
// SceneCamera.h
//---------------------------------------------------------------------------------
//
// The game camera of a scene is an object like any other: an entity with a
// Transform and a GameCamera component ("Main Camera", tag "Camera"). It is
// listed in the editor's hierarchy, moved and rotated like the other objects,
// and its GameCamera fields are edited in the inspector.
//
//   Transform position   the point the camera looks at (target)
//   Transform yaw        the direction it looks along the ground (0 = +Z)
//   GameCamera.Distance  how far the eye is from the target
//   GameCamera.Pitch     how steeply it looks down (90 = straight down)
//   GameCamera.FieldOfView
//
//          eye
//           \   Distance
//      Pitch \
//   ----------* target ------>  forward (yaw)
//
// The scene editor has its own view (SceneEditorScene); it only uses the game
// camera while playing. Scenes saved before cameras were objects have none:
// the camera in their SceneSettings is used instead (FromSettings).
//
#pragma once

#include "../Entity.h"
#include "../Reflection/Reflection.h"
#include "../Vec3.h"

#include <string>

class Camera;
class SceneSettings;

/**
 * \brief Makes an object the scene's game camera (see above)
 */
struct GameCamera
{
    float Distance = 30.0f;
    // Degrees above the ground; the old fixed camera looked down at ~61.2
    float Pitch = 61.19f;
    float FieldOfView = 90.0f;
};

REFLECT(GameCamera)
{
    Field("Distance", &GameCamera::Distance)
            .Range(1.0f, 500.0f)
            .Step(1.0f)
            .Label("Dist.")
            .Tooltip("Distance from the target");
    Field("Pitch", &GameCamera::Pitch).Range(5.0f, 89.0f).Step(5.0f).Tooltip("Degrees looking down");
    Field("FieldOfView", &GameCamera::FieldOfView)
            .Range(20.0f, 150.0f)
            .Step(5.0f)
            .Label("FOV")
            .Tooltip("Vertical field of view (degrees)");
}

namespace SceneCamera
{
    constexpr const char* DEFAULT_NAME = "Main Camera";
    constexpr const char* TAG = "Camera";

    /**
     * \brief Everything that places a camera
     */
    struct View
    {
        Vec3 Target = {0.0f, 0.0f, 0.0f};
        float Yaw = 0.0f;      // degrees, 0 looks towards +Z
        float Pitch = 61.19f;  // degrees above the ground, 5..89
        float Distance = 30.0f;
        float FieldOfView = 90.0f;

        bool operator==(const View& rhs) const;
        bool operator!=(const View& rhs) const { return !(*this == rhs); }
    };

    /**
     * \brief Direction the view looks along the ground (unit, Y = 0)
     */
    Vec3 Forward(float yawDegrees);

    /**
     * \brief Right of the view along the ground (unit, Y = 0)
     */
    Vec3 Right(float yawDegrees);

    /**
     * \brief Position of the eye
     */
    Vec3 EyeOf(const View& view);

    /**
     * \brief Place the renderer's camera (and its field of view)
     */
    void Apply(Camera& camera, const View& view);

    /**
     * \brief The scene's game camera: the first object with a GameCamera
     *        and a Transform, NULL_ENTITY when there is none
     */
    Entity Find();

    /**
     * \brief View of a camera object (world position / yaw + its GameCamera)
     */
    View ViewOf(Entity camera);

    /**
     * \brief View described by the scene settings (older scenes)
     */
    View FromSettings(const SceneSettings& settings);

    /**
     * \brief The game camera's view: the camera object, else the settings,
     *        else the defaults
     */
    View Current();

    /**
     * \brief Create a camera object showing the view
     */
    Entity Create(const View& view, const std::string& name = DEFAULT_NAME);

    /**
     * \brief Move / rotate a camera object and set its GameCamera to the view
     */
    void SetView(Entity camera, const View& view);

    /**
     * \brief Keeps the renderer's camera on the game camera while a scene
     *        plays. It only re-applies the view when the camera object (or
     *        the settings) change, so a script that drives the renderer's
     *        camera itself is not overridden every frame.
     */
    class Follower
    {
      public:
        void Update(Camera& camera);
        // The next Update applies the view whether it changed or not
        void Reset() { m_Applied = false; }

      private:
        View m_Last;
        bool m_Applied = false;
    };
} // namespace SceneCamera
