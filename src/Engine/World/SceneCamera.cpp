#include "SceneCamera.h"

#include "../Camera.h"
#include "../ECSManager.h"
#include "../Transform.h"
#include "../Utils.h"
#include "SceneComponents.h"
#include "SceneObjects.h"

#include <algorithm>
#include <cmath>

extern ECSManager ECS;

namespace SceneCamera
{
    namespace
    {
        constexpr float TO_RADIANS = 3.14159265358979f / 180.0f;

        bool Near(float a, float b) { return std::fabs(a - b) <= 1e-5f; }
    } // namespace

    bool View::operator==(const View& rhs) const
    {
        return Near(Target.X, rhs.Target.X) && Near(Target.Y, rhs.Target.Y) && Near(Target.Z, rhs.Target.Z) &&
               Near(Yaw, rhs.Yaw) && Near(Pitch, rhs.Pitch) && Near(Distance, rhs.Distance) &&
               Near(FieldOfView, rhs.FieldOfView);
    }

    Vec3 Forward(float yawDegrees)
    {
        float yaw = yawDegrees * TO_RADIANS;
        return {std::sin(yaw), 0.0f, std::cos(yaw)};
    }

    Vec3 Right(float yawDegrees)
    {
        // Looking towards +Z with Y up, the right hand side is -X
        float yaw = yawDegrees * TO_RADIANS;
        return {-std::cos(yaw), 0.0f, std::sin(yaw)};
    }

    Vec3 EyeOf(const View& view)
    {
        float pitch = std::clamp(view.Pitch, 1.0f, 89.9f) * TO_RADIANS;
        Vec3 back = Forward(view.Yaw) * -1.0f;
        Vec3 offset = back * std::cos(pitch) + Vec3(0.0f, std::sin(pitch), 0.0f);
        return view.Target + offset * std::max(view.Distance, 0.01f);
    }

    void Apply(Camera& camera, const View& view)
    {
        float fov = std::clamp(view.FieldOfView, 1.0f, 179.0f);
        if (!Near(camera.Fov, fov))
        {
            camera.Fov = fov;
            camera.SetProjectionPerspective();
        }
        camera.SetPositionAndOrientation(EyeOf(view), view.Target, {0.0f, 1.0f, 0.0f});
    }

    Entity Find()
    {
        Entity found = NULL_ENTITY;
        for (Entity e : ECS.Visit<GameCamera, Transform>())
        {
            // The lowest id: the same camera whatever the visit order
            if (found == NULL_ENTITY || e < found)
                found = e;
        }
        return found;
    }

    View ViewOf(Entity camera)
    {
        View view;
        view.Target = SceneObjects::GetPosition(camera);
        view.Yaw = SceneObjects::GetYaw(camera);
        const GameCamera& settings = ECS.GetComponent<GameCamera>(camera);
        view.Distance = settings.Distance;
        view.Pitch = settings.Pitch;
        view.FieldOfView = settings.FieldOfView;
        return view;
    }

    View FromSettings(const SceneSettings& settings)
    {
        View view;
        view.Target = settings.CameraTarget;
        view.Distance = settings.CameraDistance;
        return view;
    }

    View Current()
    {
        Entity camera = Find();
        if (camera != NULL_ENTITY)
            return ViewOf(camera);
        if (ECS.HasResource<SceneSettings>())
            return FromSettings(*ECS.GetResource<SceneSettings>());
        return View{};
    }

    Entity Create(const View& view, const std::string& name)
    {
        Entity e = SceneObjects::CreateEmpty(name, view.Target, 0.0f);
        ECS.GetComponent<SceneObject>(e).Tag = TAG;
        ECS.AddComponent<GameCamera>(e, GameCamera{});
        SetView(e, view);
        return e;
    }

    void SetView(Entity camera, const View& view)
    {
        SceneObjects::SetPosition(camera, view.Target);
        SceneObjects::SetYaw(camera, view.Yaw);
        if (!ECS.HasComponent<GameCamera>(camera))
            ECS.AddComponent<GameCamera>(camera, GameCamera{});
        GameCamera& settings = ECS.GetComponent<GameCamera>(camera);
        settings.Distance = std::clamp(view.Distance, 1.0f, 500.0f);
        settings.Pitch = std::clamp(view.Pitch, 5.0f, 89.0f);
        settings.FieldOfView = std::clamp(view.FieldOfView, 20.0f, 150.0f);
    }

    void Follower::Update(Camera& camera)
    {
        View view = Current();
        if (m_Applied && view == m_Last)
            return;
        Apply(camera, view);
        m_Last = view;
        m_Applied = true;
    }
} // namespace SceneCamera
