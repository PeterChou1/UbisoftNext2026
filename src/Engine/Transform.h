//---------------------------------------------------------------------------------
// Transform.h
//---------------------------------------------------------------------------------
//
// Position, rotation and scale of an entity, relative to its parent entity
// (or the world for a root). Affine / Inverse cache the local matrix and its
// inverse; functions that change the local pose rebuild them, set IsDirty and
// mark every descendant dirty.
//
#pragma once

#include "Entity.h"
#include "Mat4.h"
#include "Quat.h"
#include "Vec3.h"

#include <vector>

/// The plane of the 3d transform that the 2d physics system works in
enum SlicePlane
{
    XY,
    XZ,
    YZ
};

struct Transform
{
    Entity Parent = NULL_ENTITY;
    std::vector<Entity> Children;
    Vec3 LocalPosition;
    Vec3 LocalScale;
    Quat LocalRotation;
    Mat4 Affine;
    Mat4 Inverse;
    SlicePlane Plane = XY;
    bool IsDirty{};

    /// Position / rotation / scale of all parents combined
    struct Pose
    {
        Vec3 Position;
        Quat Rotation;
        Vec3 Scale;
    };

    /// At the origin, not rotated, scale 1
    Transform();
    Transform(const Vec3& pos);
    Transform(const Vec3& pos, const Quat& rot);
    Transform(const Vec3& pos, const Quat& rot, const Vec3& scale);

    /// At pos with its -Z axis pointing at target (camera convention)
    Transform(const Vec3& pos, const Vec3& target, const Vec3& up);

    /// Scale uniformly (multiplies the current scale)
    void Scale(float scale);

    /// The parents' pose (identity for a root transform)
    Pose ParentPose() const;

    /// Position with every parent applied
    Vec3 GetWorldPosition();

    /// Move to a world position (converted into the parent's space)
    void SetWorldPosition(const Vec3& position);

    /// Replace the local position, rotation and scale at once
    void SetLocalPose(const Vec3& position, const Quat& rotation, const Vec3& scale);

    /// Rotation with every parent applied
    Quat GetWorldRotation();

    /// The transform in world space (its Affine maps local points to the
    /// world). A root transform is returned unchanged
    Transform GetWorldTransform();

    /// Make `parent` the parent of this transform, which belongs to `child`
    void SetParentEntity(Entity parent, Entity child);

    /// Local X / Y axes (scaled) in the parent's space
    Vec3 GetRight();
    Vec3 GetUp();

    void SetLocalPosition(const Vec3& pos);

    /// Set the local rotation so the world rotation becomes rot
    void SetGlobalRotation(Quat rot);

    /// Rebuild Affine / Inverse after changing LocalPosition, LocalRotation or
    /// LocalScale directly, and mark it dirty
    void RebuildAffine();

    /// Rotate around the local X / Y / Z axis
    void UpdateLocalRow(float roll);
    void UpdateLocalPitch(float pitch);
    void UpdateLocalYaw(float yaw);

    /// Move by delta and rotate by rot (in local space)
    void Update(const Vec3& delta, const Quat& rot);

    /// Local point to the parent's space
    Vec3 TransformVec3(const Vec3& point) const;

    /// Rotates a normal by the local rotation. Scale is ignored, so the
    /// result is wrong for non-uniform scaling
    Vec3 TransformNormal(const Vec3& normal) const;

  private:
    /// Affine / Inverse from the local position, rotation and scale
    void BuildAffine();
};
