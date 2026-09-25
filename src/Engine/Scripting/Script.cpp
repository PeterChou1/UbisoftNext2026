#include "Script.h"

#include "../Camera.h"
#include "../GameManager.h"
#include "../Input.h"
#include "../RigidBody.h"
#include "../UIState.h"
#include "../World/Prefab.h"
#include "../World/SceneComponents.h"
#include "ScriptRegistry.h"

#include <algorithm>

extern GameManager GameSceneManager;

namespace
{
    const std::string EMPTY;
}

//-----------------------------------------------------------------------------
// ScriptBase
//-----------------------------------------------------------------------------

Entity ScriptBase::SpawnPrefab(const std::string& prefab, const Vec3& position, float yawDegrees)
{
    const Prefab::Data* data = Prefab::Find(prefab);
    return data == nullptr ? NULL_ENTITY : Prefab::Instantiate(*data, position, yawDegrees);
}

float ScriptBase::Param(const std::string& name) const
{
    auto it = m_Params.find(name);
    return it == m_Params.end() ? 0.0f : it->second;
}

void ScriptBase::Bind(const std::string& name,
                      Entity self,
                      const std::map<std::string, float>& params)
{
    m_Name = name;
    m_Self = self;
    m_Params = params;
}

const std::string& ScriptBase::NameOf(Entity entity)
{
    return Has<SceneObject>(entity) ? ECS.GetComponent<SceneObject>(entity).Name : EMPTY;
}

const std::string& ScriptBase::TagOf(Entity entity)
{
    return Has<SceneObject>(entity) ? ECS.GetComponent<SceneObject>(entity).Tag : EMPTY;
}

bool ScriptBase::KeyDown(App::Key key) const
{
    return Input::IsDown(key);
}

bool ScriptBase::KeyPressed(App::Key key) const
{
    return Input::WasPressed(key);
}

Vec2 ScriptBase::MouseScreen() const
{
    auto ui = ECS.GetResource<UIState>();
    return Vec2(ui->mouseX, ui->mouseY);
}

bool ScriptBase::MouseClicked() const
{
    return ECS.GetResource<UIState>()->leftClick;
}

bool ScriptBase::MouseRightClicked() const
{
    return ECS.GetResource<UIState>()->rightClick;
}

bool ScriptBase::MouseGround(Vec3& groundPoint) const
{
    Vec2 mouse = MouseScreen();
    Vec3 planePoint(0, 0, 0);
    Vec3 planeNormal(0, 1, 0);
    groundPoint = ECS.GetResource<Camera>()->ScreenSpaceToWorldPoint(mouse.X, mouse.Y, planePoint, planeNormal);
    return groundPoint.IsValid();
}

ScriptBase* ScriptBase::ScriptInstance(Entity entity) const
{
    return GameSceneManager.Scripts().GetScript(entity);
}

void ScriptBase::LoadScene(const std::string& sceneName)
{
    GameSceneManager.RequestLoad(GameManager::ScenePath(sceneName));
}

void ScriptBase::RestartScene()
{
    GameSceneManager.RequestRestart();
}

ScriptBase* ScriptBase::CurrentSceneScript() const
{
    return GameSceneManager.Scripts().GetSceneScript();
}

void ScriptBase::DrawText(float x, float y, const std::string& text, const Vec3& color)
{
    App::Print(x, y, text.c_str(), color.X, color.Y, color.Z);
}

//-----------------------------------------------------------------------------
// Script
//-----------------------------------------------------------------------------

RigidBody* Script::Body()
{
    return Has<RigidBody>(m_Self) ? &ECS.GetComponent<RigidBody>(m_Self) : nullptr;
}

void Script::SetVelocity(const Vec2& velocity)
{
    if (RigidBody* body = Body())
        body->Velocity = velocity;
}

//-----------------------------------------------------------------------------
// ScriptRegistry
//-----------------------------------------------------------------------------

ScriptRegistry& ScriptRegistry::Get()
{
    static ScriptRegistry registry;
    return registry;
}

const ScriptInfo* ScriptRegistry::Find(const std::string& name) const
{
    auto it = m_Scripts.find(name);
    return it == m_Scripts.end() ? nullptr : &it->second;
}

std::vector<std::string> ScriptRegistry::Names(bool sceneScripts) const
{
    std::vector<std::string> names;
    for (const auto& kv : m_Scripts)
    {
        if (kv.second.IsSceneScript == sceneScripts)
            names.push_back(kv.first);
    }
    return names; // std::map keeps them sorted
}

std::map<std::string, float>
ScriptRegistry::ResolveParams(const std::string& name,
                              const std::map<std::string, float>& overrides) const
{
    std::map<std::string, float> params;
    if (const ScriptInfo* info = Find(name))
    {
        for (const ScriptParam& p : info->Params)
            params[p.Name] = p.Default;
    }
    for (const auto& kv : overrides)
        params[kv.first] = kv.second;
    return params;
}
