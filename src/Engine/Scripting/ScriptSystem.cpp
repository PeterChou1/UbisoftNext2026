#include "ScriptSystem.h"

#include "../ColliderCallbackSystem.h"
#include "../Log.h"
#include "../World/SceneComponents.h"
#include "ScriptRegistry.h"

#include <algorithm>

void ScriptSystem::Update(float deltaMilliseconds)
{
    SyncObjectScripts();
    SyncSceneScript();
    DispatchContacts();

    float seconds = deltaMilliseconds / 1000.0f;
    if (m_Scene)
        m_Scene->OnUpdate(seconds);

    // Scripts may create or destroy objects while updating: iterate over a
    // snapshot and skip objects destroyed by an earlier script this frame
    std::vector<Entity> entities;
    entities.reserve(m_Objects.size());
    for (const auto& kv : m_Objects)
        entities.push_back(kv.first);
    std::sort(entities.begin(), entities.end());
    for (Entity e : entities)
    {
        if (!ECS.IsEntityAlive(e))
            continue;
        auto it = m_Objects.find(e);
        if (it != m_Objects.end())
            it->second.Script->OnUpdate(seconds);
    }
}

void ScriptSystem::Render()
{
    if (m_Scene)
        m_Scene->OnRender();
    for (auto& kv : m_Objects)
    {
        if (ECS.IsEntityAlive(kv.first))
            kv.second.Script->OnRender();
    }
}

void ScriptSystem::Reset()
{
    for (auto& kv : m_Objects)
        kv.second.Script->OnDestroy();
    m_Objects.clear();
    if (m_Scene)
        m_Scene->OnDestroy();
    m_Scene.reset();
    m_SceneName.clear();
    m_Missing.clear();
    // Drop contacts of the previous world
    if (ECS.HasResource<ColliderCallbackSystem>())
        ECS.GetResource<ColliderCallbackSystem>()->TakeContactEvents();
}

ScriptBase* ScriptSystem::GetScript(Entity entity)
{
    auto it = m_Objects.find(entity);
    return it == m_Objects.end() ? nullptr : it->second.Script.get();
}

void ScriptSystem::SyncObjectScripts()
{
    // Drop instances whose object is gone or whose script changed
    for (auto it = m_Objects.begin(); it != m_Objects.end();)
    {
        Entity e = it->first;
        bool valid = ECS.IsEntityAlive(e) && ECS.HasComponent<ScriptComponent>(e) &&
                     ECS.GetComponent<ScriptComponent>(e).Script == it->second.Name;
        if (valid)
        {
            ++it;
            continue;
        }
        std::unique_ptr<ScriptBase> script = std::move(it->second.Script);
        it = m_Objects.erase(it);
        script->OnDestroy();
    }

    // Create instances for new scripted objects (in entity order, deterministic)
    std::vector<Entity> pending;
    for (Entity e : ECS.Visit<ScriptComponent>())
    {
        if (m_Objects.count(e) == 0 && !ECS.GetComponent<ScriptComponent>(e).Script.empty())
            pending.push_back(e);
    }
    for (Entity e : pending)
    {
        const ScriptComponent& component = ECS.GetComponent<ScriptComponent>(e);
        std::string name = component.Script;
        std::unique_ptr<ScriptBase> script = Create(name, e, component.Params);
        if (!script)
            continue;
        ScriptBase* raw = script.get();
        m_Objects[e] = Instance{name, std::move(script)};
        raw->OnStart();
    }
}

void ScriptSystem::SyncSceneScript()
{
    if (!ECS.HasResource<SceneSettings>())
        return;
    auto settings = ECS.GetResource<SceneSettings>();
    if (settings->SceneScript == m_SceneName && (m_Scene || m_SceneName.empty()))
        return;

    if (m_Scene)
        m_Scene->OnDestroy();
    m_Scene.reset();
    m_SceneName = settings->SceneScript;
    if (m_SceneName.empty())
        return;
    m_Scene = Create(m_SceneName, NULL_ENTITY, settings->SceneParams);
    if (m_Scene)
        m_Scene->OnStart();
}

void ScriptSystem::DispatchContacts()
{
    if (!ECS.HasResource<ColliderCallbackSystem>())
        return;
    for (const ContactEvent& contact : ECS.GetResource<ColliderCallbackSystem>()->TakeContactEvents())
    {
        const Entity pair[2][2] = {{contact.A, contact.B}, {contact.B, contact.A}};
        for (const auto& p : pair)
        {
            auto it = m_Objects.find(p[0]);
            if (it == m_Objects.end() || !ECS.IsEntityAlive(p[0]))
                continue;
            auto* script = static_cast<Script*>(it->second.Script.get());
            if (contact.Type == ContactEvent::Enter)
                script->OnCollisionEnter(p[1]);
            else
                script->OnCollisionExit(p[1]);
        }
    }
}

std::unique_ptr<ScriptBase> ScriptSystem::Create(const std::string& name,
                                                 Entity self,
                                                 const std::map<std::string, float>& overrides)
{
    const ScriptRegistry& registry = ScriptRegistry::Get();
    const ScriptInfo* info = registry.Find(name);
    // Object scripts go on objects, scene scripts on the scene
    if (info == nullptr || info->IsSceneScript != (self == NULL_ENTITY))
    {
        ReportMissing(name);
        return nullptr;
    }
    std::unique_ptr<ScriptBase> script = info->Create();
    script->Bind(name, self, registry.ResolveParams(name, overrides));
    return script;
}

void ScriptSystem::ReportMissing(const std::string& name)
{
    if (std::find(m_Missing.begin(), m_Missing.end(), name) == m_Missing.end())
    {
        m_Missing.push_back(name);
        LOG_WARN("Scripts", "Script '%s' is not registered (or is the wrong kind), it will not run", name.c_str());
    }
}
