//---------------------------------------------------------------------------------
// ScriptSystem.h
//---------------------------------------------------------------------------------
//
// ECS system running the C++ scripts of the active scene.
//
// Every frame (while the scene simulates):
//   1. Sync     entities with a ScriptComponent get an instance of their script
//               (created by name from the ScriptRegistry, OnStart called).
//               Instances whose entity died or whose component was removed /
//               changed get OnDestroy and are dropped
//   2. Scene    the scene script named in SceneSettings is (re)created the same way
//   3. Contacts physics contact events become OnCollisionEnter / Exit calls on
//               the scripts of both objects
//   4. Update   OnUpdate(seconds) on the scene script, then on every object script
//
// Script instances are runtime only. The scene file stores the script names and
// parameters (ScriptComponent / SceneSettings), so saving a running scene and
// loading it again recreates the scripts with the same configuration.
//
#pragma once

#include "Script.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class ScriptSystem
{
  public:
    void Update(float deltaMilliseconds);

    /**
     * \brief OnRender on every script (HUD drawing)
     */
    void Render();

    /**
     * \brief Destroy every instance (OnDestroy is called). Needed whenever the
     *        world is replaced (scene switch, load, editor play / stop) since
     *        entity ids are reused by the new world
     */
    void Reset();

    size_t InstanceCount() const { return m_Objects.size() + (m_Scene ? 1 : 0); }

    /**
     * \brief Script instance of an entity (nullptr if none)
     */
    ScriptBase* GetScript(Entity entity);

    SceneScript* GetSceneScript() { return static_cast<SceneScript*>(m_Scene.get()); }

    /**
     * \brief Script names that could not be created (not registered)
     */
    const std::vector<std::string>& MissingScripts() const { return m_Missing; }

  private:
    struct Instance
    {
        std::string Name;
        std::unique_ptr<ScriptBase> Script;
    };

    void SyncObjectScripts();
    void SyncSceneScript();
    void DispatchContacts();
    std::unique_ptr<ScriptBase> Create(const std::string& name,
                                       Entity self,
                                       const std::map<std::string, float>& overrides);
    void ReportMissing(const std::string& name);

    std::unordered_map<Entity, Instance> m_Objects;
    std::unique_ptr<ScriptBase> m_Scene;
    std::string m_SceneName;
    std::vector<std::string> m_Missing;
};
