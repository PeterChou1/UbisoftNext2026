#include "Camera.h"

#include "Utils.h"
#include "stdafx.h"

#include <iostream>

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
    point.X = static_cast<float>(static_cast<int>((point.X + 1) * 0.5 * ScreenWidth));
    point.Y = static_cast<float>(static_cast<int>((point.Y + 1) * 0.5 * ScreenHeight));
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

/**
 * \brief Helper Function for Ray Casting
 */
Vec3 RayCastPlane(Vec3 rayPoint, Vec3 rayDirection, Vec3& planePt, Vec3& planeNormal)
{
    // Calculate the dot product of the plane normal and the ray direction
    const float dotProduct = planeNormal.Dot(rayDirection);

    // Check for zero (or near-zero) dot product, indicating the ray is parallel
    // to the plane
    if (std::abs(dotProduct) < std::numeric_limits<float>::epsilon())
    {
        // No intersection, or line lies within the plane
        return {0, 0, 0};
    }

    // Calculate the distance from the rayPoint to the intersection point on the
    // plane
    const float t = (planeNormal.Dot(planePt - rayPoint)) / dotProduct;

    // If t is negative, the intersection point is behind the ray's starting point
    if (t < 0)
    {
        return {0, 0, 0};
    }

    // Calculate the intersection point
    Vec3 intersection = rayPoint + rayDirection * t;

    return intersection;
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
    Vec3 nearCamera(nearPoint.X / nearPoint.W, nearPoint.Y / nearPoint.W, nearPoint.Z / nearPoint.W);
    Vec3 farCamera(farPoint.X / farPoint.W, farPoint.Y / farPoint.W, farPoint.Z / farPoint.W);
    Vec3 nearWorld = CameraToWorld(nearCamera);
    Vec3 farWorld = CameraToWorld(farCamera);

    Vec3 cameraRayDir = (farWorld - nearWorld).Normalize();

    return RayCastPlane(nearWorld, cameraRayDir, planePt, planeNormal);
}

Vec2 Camera::WorldPointToScreenSpace(Vec3 point)
{
    Vec4 projected = Proj * Vec4(CamTransform.Inverse * point);
    // perspective division
    float InverseW = 1 / projected.W;
    projected *= InverseW;
    ToRasterSpaceUnclamped(projected);
    return {projected.X, projected.Y};
}
