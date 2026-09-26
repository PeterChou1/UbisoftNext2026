//---------------------------------------------------------------------------------
// ScriptRegistry.h
//---------------------------------------------------------------------------------
//
// Maps script names to factories and declared parameters. A project registers
// its scripts once at startup:
//
//     ScriptRegistry::Get().Register<Rotator>(
//             "Rotator", "Spins the object", {{"Speed", 90.0f, 15.0f}});
//
// The scene editor lists the registered names (object scripts on objects,
// scene scripts in the scene settings) and shows the parameters with their
// step size. The ScriptSystem creates instances by name when a scene plays.
//
#pragma once

#include "Script.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

struct ScriptParam
{
    std::string Name;
    float Default = 0.0f;
    // Increment used by the editor's - / + buttons
    float Step = 1.0f;
};

struct ScriptInfo
{
    std::string Description;
    bool IsSceneScript = false;
    std::vector<ScriptParam> Params;
    std::function<std::unique_ptr<ScriptBase>()> Create;
};

class ScriptRegistry
{
  public:
    static ScriptRegistry& Get();

    /**
     * \brief Register (or replace) a script type under a name
     */
    template <typename T>
    void Register(const std::string& name,
                  const std::string& description = "",
                  std::vector<ScriptParam> params = {})
    {
        static_assert(std::is_base_of<Script, T>::value || std::is_base_of<SceneScript, T>::value,
                      "Scripts derive from Script or SceneScript");
        ScriptInfo info;
        info.Description = description;
        info.IsSceneScript = std::is_base_of<SceneScript, T>::value;
        info.Params = std::move(params);
        info.Create = [] { return std::make_unique<T>(); };
        m_Scripts[name] = std::move(info);
    }

    const ScriptInfo* Find(const std::string& name) const;

    /**
     * \brief Registered names, sorted (object or scene scripts)
     */
    std::vector<std::string> Names(bool sceneScripts) const;

    /**
     * \brief Declared defaults overridden by the given values
     */
    std::map<std::string, float> ResolveParams(const std::string& name,
                                               const std::map<std::string, float>& overrides) const;

  private:
    std::map<std::string, ScriptInfo> m_Scripts;
};
