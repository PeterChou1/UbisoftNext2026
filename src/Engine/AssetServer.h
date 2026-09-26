//---------------------------------------------------------------------------------
// AssetServer.h
//---------------------------------------------------------------------------------
//
// An AssetServer responsible for handling loading of all
// obj/textures/shader files in the game
//
#pragma once

#include "Assets.h"
#include "Entity.h"
#include "FragShaderTag.h"
#include "FragmentShader.h"
#include "Material.h"
#include "MeshInstance.h"
#include "VertShaderTag.h"
#include "VertexShader.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class AssetServer
{
  protected:
    AssetServer() = default;

  public:
    AssetServer(AssetServer& other) = delete;
    void operator=(const AssetServer&) = delete;

    // The global instance
    static AssetServer& GetInstance()
    {
        static AssetServer instance;
        return instance;
    }

    // Directory holding the .obj / .mtl model files
    static constexpr const char* MODEL_DIRECTORY = "data/models/";

    /**
     * \brief Returns a model by name (file name without extension in
     *        data/models). Models are loaded on first use and cached; a model
     *        that can not be loaded is returned empty (nothing is drawn)
     */
    MeshInstance& GetModel(const std::string& name);

    /**
     * \brief Drop a cached model so the next GetModel reads its file again
     *        (after importing or replacing it)
     */
    void ForgetModel(const std::string& name) { m_Models.erase(name); }

    /**
     * \brief Names of every model in data/models (sorted), used by the editor
     */
    static std::vector<std::string> AvailableModels();

    /**
     * \brief The material of a vertex's texture ID (-1: the default material)
     */
    Material& GetMaterial(int texID);

    /**
     * \brief Give the entity a new shader instance of the tag's type; the
     *        tag gets the instance's ID
     */
    void SetFragShader(Entity e, FragShaderTag& shader);
    void SetVertShader(Entity e, VertShaderTag& shader);

    void RemoveFragShader(Entity e);
    void RemoveVertShader(Entity e);

    /**
     * \brief The shader instance of an ID, the default shader when there is none
     */
    const std::shared_ptr<FragmentShader>& GetFragShader(size_t shaderID) const
    {
        return m_FragShaders.Get(shaderID, DefaultFragShader);
    }

    const std::shared_ptr<VertexShader>& GetVertShader(size_t shaderID) const
    {
        return m_VertShaders.Get(shaderID, DefaultVertShader);
    }

    static std::shared_ptr<FragmentShader> DefaultFragShader;
    static std::shared_ptr<VertexShader> DefaultVertShader;

  private:
    // The shader instances of one kind, by ID, and which entity owns which
    template <typename Shader>
    struct ShaderInstances
    {
        size_t NextId = 1;
        std::unordered_map<Entity, size_t> EntityToId;
        std::unordered_map<size_t, std::shared_ptr<Shader>> Shaders;

        size_t Add(Entity e, std::shared_ptr<Shader> shader);
        void Remove(Entity e);

        const std::shared_ptr<Shader>& Get(size_t id, const std::shared_ptr<Shader>& fallback) const
        {
            auto it = Shaders.find(id);
            return it == Shaders.end() ? fallback : it->second;
        }
    };

    std::vector<Material> m_Materials;
    std::unordered_map<std::string, MeshInstance> m_Models;
    ShaderInstances<FragmentShader> m_FragShaders;
    ShaderInstances<VertexShader> m_VertShaders;
};
