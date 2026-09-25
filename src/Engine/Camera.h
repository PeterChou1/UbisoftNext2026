//---------------------------------------------------------------------------------
// Camera.h
//---------------------------------------------------------------------------------
//
// The Camera controls the viewpoint of the player in the game
//
#pragma once

#include "AppSettings.h"
#include "Mat4.h"
#include "Resource.h"
#include "Transform.h"
#include "Vec2.h"

class Camera : public Resource
{
  public:
    // Projection matrix used by the render pipeline
    Mat4 Proj{};
    // Camera position and orientation
    Transform CamTransform{};
    Vec3 Position{};
    // Unit vector pointing away from where the camera looks
    Vec3 Backward{};
    // What the camera considers "up"
    Vec3 Up{};

    // Perspective projection parameters (Fov in degrees)
    float Nearplane = 0.1f;
    float Farplane = 1000.0f;
    float Fov = 90.0;
    float ScreenHeight = APP_VIRTUAL_HEIGHT;
    float ScreenWidth = APP_VIRTUAL_WIDTH;
    float AspectRatio = APP_VIRTUAL_WIDTH / APP_VIRTUAL_HEIGHT;

    Camera() = default;

    /// Rebuild Proj from Fov, AspectRatio and the near / far planes
    void SetProjectionPerspective();

    void SetPositionAndOrientation(Vec3 camPos, Vec3 camTarget, Vec3 camUp);

    /// Clip space point (after perspective division) to whole screen
    /// pixels, clamped to the screen
    void ToRasterSpace(Vec4& point);

    /// ToRasterSpace without the clamping
    void ToRasterSpaceUnclamped(Vec4& point);

    void ResetResource() override {}

    Vec3 CameraToWorld(const Vec3& point);
    Vec3 WorldToCamera(const Vec3& point);

    /// Where the ray through screen point (x, y) hits the plane through
    /// planePt with normal planeNormal. Zero vector when it misses (parallel,
    /// or the plane is behind the camera)
    Vec3 ScreenSpaceToWorldPoint(float x, float y, Vec3& planePt, Vec3& planeNormal);

    /// Screen point of a world point (the inverse of ScreenSpaceToWorldPoint)
    Vec2 WorldPointToScreenSpace(Vec3 point);
};
