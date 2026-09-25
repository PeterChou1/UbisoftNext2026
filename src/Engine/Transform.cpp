#include "Transform.h"

#include "ECSManager.h"

#include <cmath>

extern ECSManager ECS;

namespace
{
    // Deepest hierarchy followed (guards against cycles in damaged data)
    constexpr int MAX_DEPTH = 256;

    Vec3 MulScale(const Vec3& a, const Vec3& b)
    {
        return Vec3(a.X * b.X, a.Y * b.Y, a.Z * b.Z);
    }

    void MarkChildrenDirty(const std::vector<Entity>& children)
    {
        for (Entity child : children)
        {
            // A child destroyed without detaching it first is skipped
            if (!ECS.IsEntityAlive(child) || !ECS.HasComponent<Transform>(child))
                continue;
            Transform& t = ECS.GetComponent<Transform>(child);
            t.IsDirty = true;
            MarkChildrenDirty(t.Children);
        }
    }

    /// Apply the ancestors starting at `parent` to a pose: each ancestor
    /// scales, rotates, then translates (its Affine)
    Transform::Pose ApplyAncestors(Entity parent, Transform::Pose pose)
    {
        Entity it = parent;
        for (int depth = 0; it != NULL_ENTITY && depth < MAX_DEPTH; ++depth)
        {
            if (!ECS.IsEntityAlive(it) || !ECS.HasComponent<Transform>(it))
                break;
            const Transform& p = ECS.GetComponent<Transform>(it);
            pose.Position = p.LocalRotation.RotatePoint(MulScale(p.LocalScale, pose.Position)) +
                            p.LocalPosition;
            pose.Rotation = p.LocalRotation * pose.Rotation;
            pose.Scale = MulScale(p.LocalScale, pose.Scale);
            it = p.Parent;
        }
        return pose;
    }
} // namespace

Transform::Transform()
    : Transform(Vec3(0, 0, 0))
{
}

Transform::Transform(const Vec3& pos)
    : Transform(pos, Quat(0, 0, 0, 1))
{
}

Transform::Transform(const Vec3& pos, const Quat& rot)
    : Transform(pos, rot, Vec3(1, 1, 1))
{
}

Transform::Transform(const Vec3& pos, const Quat& rot, const Vec3& scale)
    : LocalPosition(pos)
    , LocalScale(scale)
    , LocalRotation(rot)
{
    BuildAffine();
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

void Transform::BuildAffine()
{
    Mat3 r = Mat3::FromQuat(LocalRotation);
    const Vec3& pos = LocalPosition;
    const Vec3& scale = LocalScale;
    Affine.Rows[0] = {scale.X * r[0][0], scale.Y * r[0][1], scale.Z * r[0][2], pos.X};
    Affine.Rows[1] = {scale.X * r[1][0], scale.Y * r[1][1], scale.Z * r[1][2], pos.Y};
    Affine.Rows[2] = {scale.X * r[2][0], scale.Y * r[2][1], scale.Z * r[2][2], pos.Z};
    Affine.Rows[3] = {0.0, 0.0, 0.0, 1.0};
    Inverse = Affine.AffineInverse();
}

void Transform::RebuildAffine()
{
    BuildAffine();
    IsDirty = true;
    MarkChildrenDirty(Children);
}

void Transform::Scale(float scale)
{
    for (int row = 0; row < 3; row++)
    {
        Vec4& r = Affine.Rows[row];
        r = {scale * r[0], scale * r[1], scale * r[2], LocalPosition[row]};
    }
    Affine.Rows[3] = {0.0, 0.0, 0.0, 1.0};
    Inverse = Affine.AffineInverse();
    IsDirty = true;
    LocalScale = LocalScale * scale;
    MarkChildrenDirty(Children);
}

Transform::Pose Transform::ParentPose() const
{
    return ApplyAncestors(Parent, {Vec3(0, 0, 0), Quat(0, 0, 0, 1), Vec3(1, 1, 1)});
}

// The world pose of a child is its local pose with every ancestor applied
// (scale, rotate, translate)

Vec3 Transform::GetWorldPosition()
{
    return ApplyAncestors(Parent, {LocalPosition, LocalRotation, LocalScale}).Position;
}

Quat Transform::GetWorldRotation()
{
    return ApplyAncestors(Parent, {LocalPosition, LocalRotation, LocalScale}).Rotation;
}

Transform Transform::GetWorldTransform()
{
    if (Parent == NULL_ENTITY)
        return *this;
    Pose pose = ApplyAncestors(Parent, {LocalPosition, LocalRotation, LocalScale});
    Transform world(pose.Position, pose.Rotation, pose.Scale);
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
    LocalPosition = position;
    LocalRotation = rotation;
    LocalScale = scale;
    RebuildAffine();
}

void Transform::SetParentEntity(Entity parent, Entity child)
{
    assert(ECS.HasComponent<Transform>(parent));
    Parent = parent;
    ECS.GetComponent<Transform>(parent).Children.push_back(child);
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
    Affine.Rows[0][3] = pos.X;
    Affine.Rows[1][3] = pos.Y;
    Affine.Rows[2][3] = pos.Z;
    Inverse = Affine.AffineInverse();
    IsDirty = true;
    MarkChildrenDirty(Children);
}

void Transform::SetGlobalRotation(Quat rot)
{
    if (Parent == NULL_ENTITY)
    {
        Update(Vec3(), LocalRotation.Inverse() * rot);
        return;
    }
    // Local rotation = inverse(parents' rotation) * world rotation
    Quat local = ParentPose().Rotation.Inverse() * rot;
    Update(Vec3(), LocalRotation.Inverse() * local);
}

void Transform::UpdateLocalRow(float roll)
{
    LocalRotation *= Quat(Vec3(1, 0, 0), roll);
    RebuildAffine();
}

void Transform::UpdateLocalPitch(float pitch)
{
    LocalRotation *= Quat(Vec3(0, 1, 0), pitch);
    RebuildAffine();
}

void Transform::UpdateLocalYaw(float yaw)
{
    LocalRotation *= Quat(Vec3(0, 0, 1), yaw);
    RebuildAffine();
}

void Transform::Update(const Vec3& delta, const Quat& rot)
{
    LocalRotation = LocalRotation * rot;
    LocalPosition = LocalPosition + delta;
    RebuildAffine();
}

Vec3 Transform::TransformVec3(const Vec3& point) const
{
    return Affine * point;
}

Vec3 Transform::TransformNormal(const Vec3& normal) const
{
    return LocalRotation.RotatePoint(normal);
}
