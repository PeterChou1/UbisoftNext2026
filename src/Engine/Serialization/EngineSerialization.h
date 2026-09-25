//---------------------------------------------------------------------------------
// EngineSerialization.h
//---------------------------------------------------------------------------------
//
// Serialize functions for the engine level components (transform, physics,
// rendering handles, particles, AI obstacles).
//
// Runtime handles
// ---------------
// Some components carry handles into render caches that are rebuilt every
// session (Mesh::Loaded, FragShaderTag::FragShaderID, Particle::loaded ...).
// Those values are NOT written to the save file. When loading, they are reset
// to their "not yet initialized" state so the MeshHandler / ShaderHandler
// systems rebuild the render data on the next frame, exactly like they do for
// a freshly created Entity.
//
#pragma once

#include "../AABB.h"
#include "../Emitter.h"
#include "../FragShaderTag.h"
#include "../Map.h"
#include "../Mesh.h"
#include "../RigidBody.h"
#include "../Shape.h"
#include "../Transform.h"
#include "../VertShaderTag.h"
#include "Archive.h"
#include "MathSerialization.h"

// Valid values of the engine enums stored in save files
SERIALIZATION_ENUM_RANGE(ShapeType, CircleShape, PolygonShape)
SERIALIZATION_ENUM_RANGE(ColliderCategory, Default, Category8)
SERIALIZATION_ENUM_RANGE(SlicePlane, XY, YZ)
SERIALIZATION_ENUM_RANGE(FragShaderTypeID, DefaultFragShaderID, StripesShaderID)
SERIALIZATION_ENUM_RANGE(VertShaderTypeID, DefaultVertShaderID, SwayVertShaderID)
SERIALIZATION_ENUM_RANGE(EmitterType, Sphere, Cone)

/**
 * \brief Befriended by classes that keep part of their state private
 */
struct SerializationAccess
{
    template <typename Archive>
    static void SerializeShape(Archive& ar, Shape& shape)
    {
        ar(shape.m_ShapeEnum, shape.Width, shape.Height, shape.Radius);
        ar(shape.PolygonPoints, shape.EdgeNormals, shape.LocalSpacePoints);
        ar(shape.Max, shape.Min);
        // DebugPoints and ContactPoints are scratch buffers rebuilt each physics step
        if constexpr (Archive::IsLoading)
        {
            shape.DebugPoints.clear();
            shape.ContactPoints.clear();
        }
    }

    template <typename Archive>
    static void SerializeRigidBody(Archive& ar, RigidBody& rb)
    {
        ar(rb.IsIntersecting, rb.Initialized, rb.Collidable, rb.Category, rb.Color);
        ar(rb.RigidBodyAABB, rb.Shape);
        ar(rb.Position, rb.Velocity, rb.Force);
        ar(rb.Angular, rb.AngularVelocity, rb.AngularDelta);
        ar(rb.StaticFriction, rb.DynamicFriction);
        ar(rb.m_InvInertia, rb.m_InvMass, rb.m_Restitution);
    }
};

template <typename Archive>
void Serialize(Archive& ar, Shape& shape)
{
    SerializationAccess::SerializeShape(ar, shape);
}

template <typename Archive>
void Serialize(Archive& ar, AABB& aabb)
{
    ar(aabb.OriginalMax, aabb.OriginalMin, aabb.Max, aabb.Min);
}

template <typename Archive>
void Serialize(Archive& ar, RigidBody& rb)
{
    SerializationAccess::SerializeRigidBody(ar, rb);
}

template <typename Archive>
void Serialize(Archive& ar, Transform& t)
{
    ar(t.Parent, t.Children);
    ar(t.LocalPosition, t.LocalScale, t.LocalRotation);
    ar(t.Affine, t.Inverse);
    ar(t.Plane, t.IsDirty);
}

template <typename Archive>
void Serialize(Archive& ar, Mesh& mesh)
{
    ar(mesh.Model);
    // Render data is rebuilt by the MeshHandler
    if constexpr (Archive::IsLoading)
        mesh.Loaded = false;
}

template <typename Archive>
void Serialize(Archive& ar, FragShaderTag& tag)
{
    ar(tag.FragAssetId);
    // Shader instances are recreated by the ShaderHandler
    if constexpr (Archive::IsLoading)
    {
        tag.FragShaderID = 0;
        tag.Initialized = false;
    }
}

template <typename Archive>
void Serialize(Archive& ar, VertShaderTag& tag)
{
    ar(tag.VertAssetId);
    if constexpr (Archive::IsLoading)
    {
        tag.VertShaderID = 0;
        tag.Initialized = false;
    }
}

template <typename Archive>
void Serialize(Archive& ar, Particle& particle)
{
    ar(particle.direction, particle.Color, particle.duration);
    // The particle quad is rebuilt by the MeshHandler
    if constexpr (Archive::IsLoading)
        particle.loaded = false;
}

template <typename Archive>
void Serialize(Archive& ar, Emitter& emitter)
{
    ar(emitter.emitterType, emitter.density, emitter.duration, emitter.speed, emitter.size);
    ar(emitter.particleTime, emitter.coneAngle, emitter.direction, emitter.color);
}

template <typename Archive>
void Serialize(Archive& ar, AIObstacle& obstacle)
{
    ar(obstacle.Width, obstacle.Height);
}
