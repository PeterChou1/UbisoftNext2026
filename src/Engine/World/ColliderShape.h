//---------------------------------------------------------------------------------
// ColliderShape.h
//---------------------------------------------------------------------------------
//
// Which shape an object's physics body (RigidBody) has. Without this
// component (or with Auto) the body follows the object: a rectangle gets a
// box, a circle a circle, other polygons their outline, models a circle.
// Otherwise:
//
//   Box       the object's footprint as a rectangle
//   Circle    a circle around the footprint
//   Polygon   the object's outline (a circle becomes a many sided polygon)
//
// Scale sizes the collider relative to the object (1 = the same size).
//
// The editor edits it in the RigidBody section ("Collider" in the component
// catalog, so duplicates, prefabs and scene files keep it). Change it from
// code with SceneObjects::SetColliderShape (which rebuilds the body).
//
#pragma once

#include "../Reflection/Reflection.h"

enum class ColliderShapeType
{
    Auto,
    Box,
    Circle,
    Polygon
};

struct ColliderShape
{
    ColliderShapeType Type = ColliderShapeType::Auto;
    float Scale = 1.0f;
};

REFLECT(ColliderShape)
{
    Field("Shape", &ColliderShape::Type)
            .Options({"Auto", "Box", "Circle", "Polygon"})
            .Tooltip("Shape of the physics body (Auto: like the object)");
    Field("Scale", &ColliderShape::Scale).Range(0.1f, 5.0f).Step(0.1f).Tooltip("Collider size relative to the object");
}
