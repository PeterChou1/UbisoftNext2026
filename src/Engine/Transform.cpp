#include "Transform.h"

#include "ECSManager.h"
#include "stdafx.h"

#include <cmath>

extern ECSManager ECS;

void UpdateChild(std::vector<Entity> children)
{

    for (Entity child : children)
    {
        // A child destroyed without detaching it first is skipped
        if (!ECS.IsEntityAlive(child) || !ECS.HasComponent<Transform>(child))
            continue;
        Transform& T = ECS.GetComponent<Transform>(child);
        T.IsDirty = true;
        UpdateChild(T.Children);
    }
}

namespace
{
    // Deepest hierarchy followed (guards against cycles in damaged data)
    constexpr int MAX_DEPTH = 256;

    Vec3 MulScale(const Vec3& a, const Vec3& b) { return Vec3(a.X * b.X, a.Y * b.Y, a.Z * b.Z); }

    /**
     * \brief Apply the ancestors starting at `parent` to a local pose:
     *        each ancestor scales, rotates, then translates (its Affine)
     */
    void ApplyAncestors(Entity parent, Vec3& position, Quat& rotation, Vec3& scale)
    {
        Entity it = parent;
        for (int depth = 0; it != NULL_ENTITY && depth < MAX_DEPTH; ++depth)
        {
            if (!ECS.IsEntityAlive(it) || !ECS.HasComponent<Transform>(it))
                break;
            const Transform& P = ECS.GetComponent<Transform>(it);
            position = P.LocalRotation.RotatePoint(MulScale(P.LocalScale, position)) + P.LocalPosition;
            rotation = P.LocalRotation * rotation;
            scale = MulScale(P.LocalScale, scale);
            it = P.Parent;
        }
    }
} // namespace

Transform::Pose Transform::ParentPose() const
{
    Pose pose{Vec3(0, 0, 0), Quat(0, 0, 0, 1), Vec3(1, 1, 1)};
    ApplyAncestors(Parent, pose.Position, pose.Rotation, pose.Scale);
    return pose;
}

Transform::Transform()
    : LocalPosition(Vec3(0, 0, 0))
    , LocalRotation(Quat(0, 0, 0, 1))
{
    Affine.Rows[0] = {1, 0, 0, 0};
    Affine.Rows[1] = {0, 1, 0, 0};
    Affine.Rows[2] = {0, 0, 1, 0};
    Affine.Rows[3] = {0, 0, 0, 1};
    Inverse = Affine.AffineInverse();
    LocalScale = Vec3(1, 1, 1);
}

Transform::Transform(const Vec3& pos)
    : LocalPosition(pos)
    , LocalRotation(Quat(0, 0, 0, 1))
{
    Affine.Rows[0] = {1, 0, 0, pos.X};
    Affine.Rows[1] = {0, 1, 0, pos.Y};
    Affine.Rows[2] = {0, 0, 1, pos.Z};
    Affine.Rows[3] = {0, 0, 0, 1};
    LocalScale = Vec3(1, 1, 1);
    Inverse = Affine.AffineInverse();
}

Transform::Transform(const Vec3& pos, const Quat& rot)
    : LocalPosition(pos)
    , LocalRotation(rot)
{
    Mat3 r = Mat3::FromQuat(rot);
    Affine.Rows[0] = {r[0][0], r[0][1], r[0][2], pos.X};
    Affine.Rows[1] = {r[1][0], r[1][1], r[1][2], pos.Y};
    Affine.Rows[2] = {r[2][0], r[2][1], r[2][2], pos.Z};
    Affine.Rows[3] = {0.0, 0.0, 0.0, 1.0};
    LocalScale = Vec3(1, 1, 1);
    LocalRotation = rot;
    Inverse = Affine.AffineInverse();
}

Transform::Transform(const Vec3& pos, const Quat& rot, const Vec3& scale)
    : LocalPosition(pos)
{
    Mat3 r = Mat3::FromQuat(rot);
    Affine.Rows[0] = {scale.X * r[0][0], scale.Y * r[0][1], scale.Z * r[0][2], pos.X};
    Affine.Rows[1] = {scale.X * r[1][0], scale.Y * r[1][1], scale.Z * r[1][2], pos.Y};
    Affine.Rows[2] = {scale.X * r[2][0], scale.Y * r[2][1], scale.Z * r[2][2], pos.Z};
    Affine.Rows[3] = {0.0, 0.0, 0.0, 1.0};
    LocalScale = scale;
    LocalRotation = rot;
    Inverse = Affine.AffineInverse();
}

void Transform::Scale(float scale)
{
    Affine.Rows[0] = {scale * Affine.Rows[0][0],
                      scale * Affine.Rows[0][1],
                      scale * Affine.Rows[0][2],
                      LocalPosition.X};
    Affine.Rows[1] = {scale * Affine.Rows[1][0],
                      scale * Affine.Rows[1][1],
                      scale * Affine.Rows[1][2],
                      LocalPosition.Y};
    Affine.Rows[2] = {scale * Affine.Rows[2][0],
                      scale * Affine.Rows[2][1],
                      scale * Affine.Rows[2][2],
                      LocalPosition.Z};
    Affine.Rows[3] = {0.0, 0.0, 0.0, 1.0};
    Inverse = Affine.AffineInverse();
    IsDirty = true;
    LocalScale = LocalScale * scale;
    UpdateChild(Children);
}

Transform::Transform(const Vec3& pos, const Vec3& target, const Vec3& up)
{
    Vec3 forward = (target - pos).Normalize() * -1;
    Vec3 right = up.Cross(forward).Normalize();
    Vec3 camUp = forward.Cross(right);
    Mat3 r;
    r.Rows[0] = {right.X, camUp.X, forward.X};
    r.Rows[1] = {right.Y, camUp.Y, forward.Y};
    r.Rows[2] = {right.Z, camUp.Z, forward.Z};
    LocalPosition = pos;
    LocalRotation = Quat::FromRotationMatrix(r);
    LocalScale = Vec3(1, 1, 1);
    Affine.Rows[0] = {right.X, camUp.X, forward.X, pos.X};
    Affine.Rows[1] = {right.Y, camUp.Y, forward.Y, pos.Y};
    Affine.Rows[2] = {right.Z, camUp.Z, forward.Z, pos.Z};
    Affine.Rows[3] = {0.0, 0.0, 0.0, 1.0};
    Inverse = Affine.AffineInverse();
}

// The world pose of a child is its local pose with every ancestor applied
// (scale, rotate, translate). These used to follow the wrong entity after the
// first parent and rotate the offset by the child's rotation instead of the
// parent's

Vec3 Transform::GetWorldPosition()
{
    if (Parent == NULL_ENTITY)
        return LocalPosition;
    Vec3 position = LocalPosition;
    Quat rotation = LocalRotation;
    Vec3 scale = LocalScale;
    ApplyAncestors(Parent, position, rotation, scale);
    return position;
}

Quat Transform::GetWorldRotation()
{
    if (Parent == NULL_ENTITY)
        return LocalRotation;
    Vec3 position = LocalPosition;
    Quat rotation = LocalRotation;
    Vec3 scale = LocalScale;
    ApplyAncestors(Parent, position, rotation, scale);
    return rotation;
}

Transform Transform::GetWorldTransform()
{
    if (Parent == NULL_ENTITY)
        return *this;
    Vec3 position = LocalPosition;
    Quat rotation = LocalRotation;
    Vec3 scale = LocalScale;
    ApplyAncestors(Parent, position, rotation, scale);
    Transform world(position, rotation, scale);
    world.Plane = Plane;
    return world;
}

void Transform::SetWorldPosition(const Vec3& position)
{
    if (Parent == NULL_ENTITY)
    {
        SetLocalPosition(position);
        return;
    }
    // Undo the parents: translate, rotate, then scale back
    Pose parent = ParentPose();
    Vec3 local = parent.Rotation.Inverse().RotatePoint(position - parent.Position);
    auto unscale = [](float v, float s) { return std::fabs(s) > 1e-6f ? v / s : v; };
    SetLocalPosition(Vec3(unscale(local.X, parent.Scale.X),
                          unscale(local.Y, parent.Scale.Y),
                          unscale(local.Z, parent.Scale.Z)));
}

void Transform::SetLocalPose(const Vec3& position, const Quat& rotation, const Vec3& scale)
{
    Transform pose(position, rotation, scale);
    LocalPosition = pose.LocalPosition;
    LocalRotation = pose.LocalRotation;
    LocalScale = pose.LocalScale;
    Affine = pose.Affine;
    Inverse = pose.Inverse;
    IsDirty = true;
    UpdateChild(Children);
}

void Transform::SetParentEntity(Entity parent, Entity children)
{
    assert(ECS.HasComponent<Transform>(parent));
    Parent = parent;
    Transform& T = ECS.GetComponent<Transform>(parent);
    T.Children.push_back(children);
}

Vec3 Transform::GetRight()
{
    return {Affine[0][0], Affine[1][0], Affine[2][0]};
}

Vec3 Transform::GetUp()
{
    return {Affine[0][1], Affine[1][1], Affine[2][1]};
}

void Transform::SetLocalPosition(const Vec3& pos)
{
    LocalPosition = pos;
    Affine.Rows[0][3] = LocalPosition.X;
    Affine.Rows[1][3] = LocalPosition.Y;
    Affine.Rows[2][3] = LocalPosition.Z;
    Inverse = Affine.AffineInverse();
    IsDirty = true;
    UpdateChild(Children);
}

void Transform::SetGlobalRotation(Quat rot)
{
    if (Parent != NULL_ENTITY)
    {
        // Local rotation = inverse(parents' rotation) * world rotation
        Quat local = ParentPose().Rotation.Inverse() * rot;
        Update(Vec3(), LocalRotation.Inverse() * local);
        return;
    }
    Quat parent = GetWorldRotation();
    Quat invParent = parent.Inverse();
    Update(Vec3(), invParent * rot);
}

void Transform::RebuildAffine()
{
    Mat3 newrot = Mat3::FromQuat(LocalRotation);
    Affine.Rows[0] = {LocalScale.X * newrot[0][0],
                      LocalScale.Y * newrot[0][1],
                      LocalScale.Z * newrot[0][2],
                      LocalPosition.X};
    Affine.Rows[1] = {LocalScale.X * newrot[1][0],
                      LocalScale.Y * newrot[1][1],
                      LocalScale.Z * newrot[1][2],
                      LocalPosition.Y};
    Affine.Rows[2] = {LocalScale.X * newrot[2][0],
                      LocalScale.Y * newrot[2][1],
                      LocalScale.Z * newrot[2][2],
                      LocalPosition.Z};
    Affine.Rows[3] = {0.0, 0.0, 0.0, 1.0};
    Inverse = Affine.AffineInverse();
    IsDirty = true;
    UpdateChild(Children);
}

void Transform::UpdateLocalRow(float row)
{
    Quat q = Quat(Vec3(1, 0, 0), row);
    LocalRotation *= q;
    RebuildAffine();
}

void Transform::UpdateLocalPitch(float pitch)
{
    Quat q = Quat(Vec3(0, 1, 0), pitch);
    LocalRotation *= q;
    RebuildAffine();
}

void Transform::UpdateLocalYaw(float yaw)
{
    Quat q = Quat(Vec3(0.0f, 0.0f, 1.0), yaw);
    LocalRotation *= q;
    RebuildAffine();
}

void Transform::Update(const Vec3& delta, const Quat& rot)
{
    LocalRotation = LocalRotation * rot;
    LocalPosition = LocalPosition + delta;
    Mat3 newrot = Mat3::FromQuat(LocalRotation);
    Affine.Rows[0] = {LocalScale.X * newrot[0][0],
                      LocalScale.Y * newrot[0][1],
                      LocalScale.Z * newrot[0][2],
                      LocalPosition.X};
    Affine.Rows[1] = {LocalScale.X * newrot[1][0],
                      LocalScale.Y * newrot[1][1],
                      LocalScale.Z * newrot[1][2],
                      LocalPosition.Y};
    Affine.Rows[2] = {LocalScale.X * newrot[2][0],
                      LocalScale.Y * newrot[2][1],
                      LocalScale.Z * newrot[2][2],
                      LocalPosition.Z};
    Affine.Rows[3] = {0.0, 0.0, 0.0, 1.0};
    Inverse = Affine.AffineInverse();
    IsDirty = true;
    UpdateChild(Children);
}

Vec3 Transform::TransformVec3(const Vec3& point) const
{
    return Affine * point;
}

Vec3 Transform::TransformNormal(const Vec3& normal) const
{
    return LocalRotation.RotatePoint(normal);
}
