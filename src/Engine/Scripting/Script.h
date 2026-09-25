//---------------------------------------------------------------------------------
// Script.h
//---------------------------------------------------------------------------------
//
// Base classes for C++ gameplay scripts.
//
//   Script       behaviour attached to one scene object (ScriptComponent)
//   SceneScript  drives a whole scene (SceneSettings::SceneScript)
//
// Scripts are plain classes registered by name in the ScriptRegistry. The
// ScriptSystem creates one instance per object / scene when the scene starts
// playing and calls the lifecycle methods every frame:
//
//   OnStart()                  once, before the first update
//   OnUpdate(deltaSeconds)     every frame
//   OnCollisionEnter(other)    (Script) a body started touching this object
//   OnCollisionExit(other)     (Script) it stopped touching
//   OnRender()                 after the 3D scene is drawn (HUD text, lines)
//   OnDestroy()                the object / scene went away
//
// Scripts do not own data that must be saved: gameplay state that should
// survive a save lives in ECS components, scripts are the behaviour.
//
#pragma once

#include "../ECSManager.h"
#include "../Transform.h"
#include "../Vec2.h"
#include "../World/SceneObjects.h"
#include "app.h"

#include <map>
#include <string>
#include <vector>

extern ECSManager ECS;

class RigidBody;

class ScriptBase
{
  public:
    virtual ~ScriptBase() = default;

    virtual void OnStart() {}
    virtual void OnUpdate(float deltaSeconds) {}
    virtual void OnRender() {}
    virtual void OnDestroy() {}

    const std::string& ScriptName() const { return m_Name; }

    // -- Parameters (declared at registration, edited in the scene editor) ------

    /**
     * \brief Value of a parameter (0 if the script did not declare it)
     */
    float Param(const std::string& name) const;

    /**
     * \brief Called by the ScriptSystem when the instance is created
     */
    void Bind(const std::string& name, Entity self, const std::map<std::string, float>& params);

  protected:
    // -- ECS access -------------------------------------------------------------

    template <typename T>
    T& Get(Entity entity)
    {
        return ECS.GetComponent<T>(entity);
    }

    template <typename T>
    bool Has(Entity entity) const
    {
        return ECS.IsEntityAlive(entity) && ECS.HasComponent<T>(entity);
    }

    template <typename T>
    std::shared_ptr<T> Resource()
    {
        return ECS.GetResource<T>();
    }

    bool IsAlive(Entity entity) const { return entity != NULL_ENTITY && ECS.IsEntityAlive(entity); }

    // -- Scene queries / changes ------------------------------------------------

    Entity Find(const std::string& name) const { return SceneObjects::FindByName(name); }

    std::vector<Entity> FindByTag(const std::string& tag) const
    {
        return SceneObjects::FindByTag(tag);
    }

    const std::string& NameOf(Entity entity);
    const std::string& TagOf(Entity entity);

    Vec3 PositionOf(Entity entity) const { return SceneObjects::GetPosition(entity); }

    void SetPositionOf(Entity entity, const Vec3& position)
    {
        SceneObjects::SetPosition(entity, position);
    }

    /**
     * \brief Create a new shape object while the scene runs
     */
    Entity Spawn(const SceneObjects::ShapeDesc& desc) { return SceneObjects::CreateShape(desc); }

    void Destroy(Entity entity) { SceneObjects::Destroy(entity); }

    // -- Input ------------------------------------------------------------------

    bool KeyDown(App::Key key) const;
    bool KeyPressed(App::Key key) const;

    // Mouse (virtual screen coordinates, y up). Clicks are reported for the
    // frame the button went down
    Vec2 MouseScreen() const;
    bool MouseClicked() const;
    bool MouseRightClicked() const;
    bool MouseDown() const;

    /**
     * \brief Point of the ground (y = 0) under the mouse, false if the mouse
     *        ray misses the ground
     */
    bool MouseGround(Vec3& groundPoint) const;

    // -- Scene flow / drawing ---------------------------------------------------

    /**
     * \brief Load data/scenes/<sceneName>.ubsave at the start of the next frame
     */
    void LoadScene(const std::string& sceneName);

    /**
     * \brief Restart the scene that is currently playing
     */
    void RestartScene();

    /**
     * \brief The script driving the current scene (nullptr if none)
     */
    ScriptBase* CurrentSceneScript() const;

    /**
     * \brief Script instance running on another object if it is a T (nullptr
     *        otherwise), so scripts can talk to each other, e.g.
     *        if (auto* unit = ScriptOf<MIUnit>(other)) unit->Damage(10);
     */
    template <typename T>
    T* ScriptOf(Entity entity) const
    {
        return dynamic_cast<T*>(ScriptInstance(entity));
    }
    ScriptBase* ScriptInstance(Entity entity) const;

    /**
     * \brief The current scene script if it is a T, e.g.
     *        if (auto* game = SceneScriptAs<CollectGame>()) game->AddScore(1);
     */
    template <typename T>
    T* SceneScriptAs() const
    {
        return dynamic_cast<T*>(CurrentSceneScript());
    }

    /**
     * \brief Draw text on screen (virtual screen coordinates, y up). Call it
     *        from OnRender
     */
    void DrawText(float x, float y, const std::string& text, const Vec3& color = {1, 1, 1});

    Entity m_Self = NULL_ENTITY;

  private:
    std::string m_Name;
    std::map<std::string, float> m_Params;
};

/**
 * \brief Behaviour attached to a scene object
 */
class Script : public ScriptBase
{
  public:
    virtual void OnCollisionEnter(Entity other) {}
    virtual void OnCollisionExit(Entity other) {}

    Entity Self() const { return m_Self; }

  protected:
    Transform& GetTransform() { return ECS.GetComponent<Transform>(m_Self); }

    Vec3 Position() const { return SceneObjects::GetPosition(m_Self); }

    void SetPosition(const Vec3& position) { SceneObjects::SetPosition(m_Self, position); }

    float Yaw() const { return SceneObjects::GetYaw(m_Self); }

    void SetYaw(float degrees) { SceneObjects::SetYaw(m_Self, degrees); }

    /**
     * \brief Physics body of this object (nullptr if it has none)
     */
    RigidBody* Body();

    /**
     * \brief Velocity on the ground plane (x, z) of a dynamic body
     */
    void SetVelocity(const Vec2& velocity);

    void DestroySelf() { Destroy(m_Self); }
};

/**
 * \brief Script driving a whole scene (spawning, rules, win / lose, HUD ...)
 */
class SceneScript : public ScriptBase
{
};
