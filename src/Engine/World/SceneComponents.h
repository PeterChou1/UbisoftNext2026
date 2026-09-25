//---------------------------------------------------------------------------------
// SceneComponents.h
//---------------------------------------------------------------------------------
//
// ECS components / resources that make up an authored scene.
//
// A scene object is a plain ECS entity. What it is depends only on the
// components attached to it:
//
//   Transform       position / rotation on the XZ ground plane (engine component)
//   SceneObject     name and tag, how the editor and scripts identify it
//   Shape2D         a basic 2D shape, extruded and rendered by the 3D renderer
//   Mesh            (alternative to Shape2D) a 3D model from data/models
//   RigidBody       optional physics body (static or dynamic)
//   FragShaderTag   which fragment shader draws it (shapes use ShapeShaderID)
//   VertShaderTag   optional animated vertex shader (Wave, Sway)
//   GameCamera      on the scene's camera object (SceneCamera.h)
//   ScriptComponent optional C++ behaviour script and its parameters
//   PrefabLink      on the root of a prefab instance: which prefab it came from
//
// SceneSettings is an ECS resource holding per scene data (scene script,
// field size, and the camera of scenes saved before the camera was an object).
//
#pragma once

#include "../Resource.h"
#include "../Vec3.h"

#include <map>
#include <string>

/**
 * \brief Identity of an authored object (every editor object has one)
 */
struct SceneObject
{
    // Unique in the scene, scripts look objects up by name
    std::string Name;
    // Free grouping label ("Player", "Pickup" ...), scripts query by tag
    std::string Tag;
};

enum class Shape2DType
{
    Rectangle,
    Circle,
    Triangle,
    Polygon,
    Count
};

/**
 * \brief A basic 2D shape lying on the XZ ground plane. The 3D renderer draws
 *        it as a flat prism (the shape extruded upwards by Thickness)
 */
struct Shape2D
{
    Shape2DType Type = Shape2DType::Rectangle;
    // Footprint size. Circle and Polygon use Width as their diameter
    float Width = 1.0f;
    float Height = 1.0f;
    // Number of sides of a regular Polygon
    int Sides = 6;
    // Extrusion height of the rendered prism
    float Thickness = 0.25f;
    Vec3 Color = {0.8f, 0.8f, 0.8f};
    // Runtime: set once the MeshHandler built the render mesh. Setting it back
    // to false (after editing the shape) rebuilds the mesh. Never saved
    bool Built = false;
};

/**
 * \brief Attaches a C++ behaviour script (by registered name) to an object.
 *        Params override the script's declared parameter defaults
 */
struct ScriptComponent
{
    std::string Script;
    std::map<std::string, float> Params;
};

/**
 * \brief Marks the root object of a prefab instance (World/Prefab.h). The
 *        editor uses it to update, reset or unpack instances
 */
struct PrefabLink
{
    // Prefab name (file data/prefabs/<Prefab>.ubprefab)
    std::string Prefab;
};

/**
 * \brief Per scene settings (ECS resource, saved with the scene)
 */
class SceneSettings : public Resource
{
  public:
    std::string Name;
    // Optional scene level script driving the whole scene
    std::string SceneScript;
    std::map<std::string, float> SceneParams;
    // Size of the playing field (objects are kept inside it by the editor)
    float FieldWidth = 40.0f;
    float FieldHeight = 40.0f;
    // Camera looking at CameraTarget from CameraDistance along a fixed tilt
    Vec3 CameraTarget = {0.0f, 0.0f, 0.0f};
    float CameraDistance = 30.0f;

    void ResetResource() override { *this = SceneSettings{}; }
};
