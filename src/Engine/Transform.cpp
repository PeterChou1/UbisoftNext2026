#include "Transform.h"

#include "ECSManager.h"
#include "stdafx.h"

extern ECSManager ECS;

Vec3 GetScale(const Mat4& M)
{
    Vec3 Scale;
    Vec3 ScaleX = Vec3(M[0][0], M[1][0], M[0][2]);
    Vec3 ScaleY = Vec3(M[0][1], M[1][1], M[1][2]);
    Vec3 ScaleZ = Vec3(M[0][2], M[1][2], M[2][2]);
    Scale.X = ScaleX.GetMagnitude();
    Scale.Y = ScaleY.GetMagnitude();
    Scale.Z = ScaleZ.GetMagnitude();
    return Scale;
}

void UpdateChild(std::vector<Entity> children)
{

    for (Entity child : children)
    {
        assert(ECS.HasComponent<Transform>(child));
        Transform& T = ECS.GetComponent<Transform>(child);
        T.IsDirty = true;
        UpdateChild(T.Children);
    }
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

Transform::Transform(const Mat4& a)
    : Affine(a)
{
    LocalPosition = Vec3(a[0][3], a[1][3], a[2][3]);
    Mat3 r;
    r.Rows[0] = {a[0][0], a[0][1], a[0][2]};
    r.Rows[1] = {a[1][0], a[1][1], a[1][2]};
    r.Rows[2] = {a[2][0], a[2][1], a[2][2]};
    LocalRotation = Quat::FromRotationMatrix(r);
    LocalScale = GetScale(a);
    Inverse = Affine.AffineInverse();
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

void Transform::Scale(Vec3 scale)
{
    Affine.Rows[0] = {scale.X * Affine.Rows[0][0],
                      scale.Y * Affine.Rows[0][1],
                      scale.Z * Affine.Rows[0][2],
                      LocalPosition.X};
    Affine.Rows[1] = {scale.X * Affine.Rows[1][0],
                      scale.Y * Affine.Rows[1][1],
                      scale.Z * Affine.Rows[1][2],
                      LocalPosition.Y};
    Affine.Rows[2] = {scale.X * Affine.Rows[2][0],
                      scale.Y * Affine.Rows[2][1],
                      scale.Z * Affine.Rows[2][2],
                      LocalPosition.Z};
    Affine.Rows[3] = {0.0, 0.0, 0.0, 1.0};
    Inverse = Affine.AffineInverse();
    LocalScale = scale;
    IsDirty = true;
    UpdateChild(Children);
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

Vec3 Transform::GetWorldPosition()
{
    Vec3 worldPos = LocalPosition;
    Entity Iterator = Parent;
    while (Iterator != NULL_ENTITY)
    {
        Transform T = ECS.GetComponent<Transform>(Parent);
        worldPos.X = worldPos.X * T.LocalScale.X;
        worldPos.Y = worldPos.Y * T.LocalScale.Y;
        worldPos.Z = worldPos.Z * T.LocalScale.Z;
        worldPos = T.LocalRotation.RotatePoint(worldPos);
        worldPos += T.LocalPosition;
        Iterator = T.Parent;
    }
    return worldPos;
}

Quat Transform::GetWorldRotation()
{

    Quat Rot = LocalRotation;
    Entity Iterator = Parent;
    while (Iterator != NULL_ENTITY)
    {
        Transform T = ECS.GetComponent<Transform>(Parent);
        Rot = Rot * T.LocalRotation;
        Iterator = T.Parent;
    }
    return Rot;
}

Transform Transform::GetWorldTransform()
{
    const Transform worldTransform = *this;
    const Entity Iterator = Parent;

    if (Iterator != NULL_ENTITY)
    {
        const Transform worldParent = ECS.GetComponent<Transform>(Iterator).GetWorldTransform();
        Vec3 Scale = worldTransform.LocalScale;
        Scale.X *= worldParent.LocalScale.X;
        Scale.Y *= worldParent.LocalScale.Y;
        Scale.Z *= worldParent.LocalScale.Z;
        Quat Rot = worldParent.LocalRotation * worldTransform.LocalRotation;
        Vec3 Position = worldTransform.LocalPosition;
        Position.X *= worldParent.LocalScale.X;
        Position.Y *= worldParent.LocalScale.Y;
        Position.Z *= worldParent.LocalScale.Z;
        Position = worldTransform.LocalRotation.RotatePoint(Position);
        Position = Position + worldParent.LocalPosition;
        Transform T = Transform(Position, Rot, Scale);
        return T;
    }
    return *this;
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

Vec3 Transform::GetForward()
{
    return {Affine[0][2], Affine[1][2], Affine[2][2]};
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

void Transform::SetPosition2D(const Vec2& pos)
{
    LocalPosition.X = pos.X;
    LocalPosition.Y = pos.Y;
    Affine.Rows[0][3] = LocalPosition.X;
    Affine.Rows[1][3] = LocalPosition.Y;
    Inverse = Affine.AffineInverse();
    IsDirty = true;
    UpdateChild(Children);
}

void Transform::SetGlobalRotation(Quat rot)
{
    if (Parent != NULL_ENTITY)
    {
        Update(Vec3(), rot);
    }
    Quat parent = GetWorldRotation();
    Quat invParent = parent.Inverse();
    Update(Vec3(), invParent * rot);
}

void Transform::UpdateLocalRow(float row)
{
    Quat q = Quat(Vec3(1, 0, 0), row);
    LocalRotation *= q;
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

void Transform::UpdateLocalPitch(float pitch)
{
    Quat q = Quat(Vec3(0, 1, 0), pitch);
    LocalRotation *= q;
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

void Transform::UpdateLocalYaw(float yaw)
{
    Quat q = Quat(Vec3(0.0f, 0.0f, 1.0), yaw);
    LocalRotation *= q;
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

Transform Transform::Lerp(const Transform& a, const Transform& b, float t)
{
    // Interpolate position
    Vec3 Position = Vec3::Lerp(a.LocalPosition, b.LocalPosition, t);
    // Interpolate rotation using SLERP
    Quat Rotation = Quat::Slerp(a.LocalRotation, b.LocalRotation, t);
    // Interpolate scale
    Vec3 scale = a.LocalScale * (1 - t) + b.LocalScale * t;
    Transform result = Transform(Position, Rotation);
    result.Scale(scale);
    return result;
}
