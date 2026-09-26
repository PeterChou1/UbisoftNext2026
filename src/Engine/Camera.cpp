#include "Camera.h"

#include "Utils.h"

#include <cmath>
#include <limits>

namespace
{
    /// Where the ray hits the plane, zero vector if it doesn't (the ray is
    /// parallel to the plane or points away from it)
    Vec3 RayCastPlane(Vec3 rayPoint, Vec3 rayDirection, Vec3& planePt, Vec3& planeNormal)
    {
        const float dotProduct = planeNormal.Dot(rayDirection);
        if (std::abs(dotProduct) < std::numeric_limits<float>::epsilon())
            return {0, 0, 0};

        // Distance along the ray
        const float t = (planeNormal.Dot(planePt - rayPoint)) / dotProduct;
        if (t < 0)
            return {0, 0, 0};

        return rayPoint + rayDirection * t;
    }
} // namespace

void Camera::SetProjectionPerspective()
{
    Proj.PerspectiveOpenGL(Fov, AspectRatio, Nearplane, Farplane);
}

void Camera::SetPositionAndOrientation(Vec3 camPos, Vec3 camTarget, Vec3 camUp)
{
    Position = camPos;
    Up = camUp;
    Backward = (camTarget - Position).Normalize() * -1;
    CamTransform = Transform(Position, camTarget, Up);
}

void Camera::ToRasterSpace(Vec4& point)
{
    ToRasterSpaceUnclamped(point);
    point.X = Utils::Clamp(point.X, 0, ScreenWidth);
    point.Y = Utils::Clamp(point.Y, 0, ScreenHeight);
}

void Camera::ToRasterSpaceUnclamped(Vec4& point)
{
    point.X = static_cast<float>(static_cast<int>((point.X + 1) * 0.5 * ScreenWidth));
    point.Y = static_cast<float>(static_cast<int>((point.Y + 1) * 0.5 * ScreenHeight));
}

Vec3 Camera::CameraToWorld(const Vec3& point)
{
    return CamTransform.Affine * point;
}

Vec3 Camera::WorldToCamera(const Vec3& point)
{
    return CamTransform.Inverse * point;
}

Vec3 Camera::ScreenSpaceToWorldPoint(float x, float y, Vec3& planePt, Vec3& planeNormal)
{
    // Exact inverse of WorldPointToScreenSpace: un-project two points of the
    // pixel's ray (near and far plane) through the same projection matrix and
    // camera transform the renderer uses, then intersect the ray with the plane
    float ndcX = x / ScreenWidth * 2.0f - 1.0f;
    float ndcY = y / ScreenHeight * 2.0f - 1.0f;
    Mat4 inverseProj = Proj.Inverse();
    Vec4 nearPoint = inverseProj * Vec4(ndcX, ndcY, -1.0f, 1.0f);
    Vec4 farPoint = inverseProj * Vec4(ndcX, ndcY, 1.0f, 1.0f);
    Vec3 nearCamera(
            nearPoint.X / nearPoint.W, nearPoint.Y / nearPoint.W, nearPoint.Z / nearPoint.W);
    Vec3 farCamera(farPoint.X / farPoint.W, farPoint.Y / farPoint.W, farPoint.Z / farPoint.W);
    Vec3 nearWorld = CameraToWorld(nearCamera);
    Vec3 farWorld = CameraToWorld(farCamera);

    Vec3 cameraRayDir = (farWorld - nearWorld).Normalize();

    return RayCastPlane(nearWorld, cameraRayDir, planePt, planeNormal);
}

Vec2 Camera::WorldPointToScreenSpace(Vec3 point)
{
    Vec4 projected = Proj * Vec4(CamTransform.Inverse * point);
    // Perspective division
    projected *= 1 / projected.W;
    ToRasterSpaceUnclamped(projected);
    return {projected.X, projected.Y};
}
