//---------------------------------------------------------------------------------
// AssetServer.h
//---------------------------------------------------------------------------------
//
// An AssetServer responsible for handling loading of all
// obj/textures/shader files in the game
//
#pragma once

#include "Assets.h"
#include "BlingPhong.h"
#include "DefaultVertexShader.h"
#include "FragShaderTag.h"
#include "Material.h"
#include "MeshInstance.h"
#include "NormalShaderSIMD.h"
#include "OutlineShaderSIMD.h"
#include "ParticleShaderSIMD.h"
#include "RedShaderSIMD.h"
#include "ShapeShaderSIMD.h"
#include "ToonShaderSIMD.h"
#include "UnlitSIMD.h"
#include "Utils.h"
#include "VertShaderTag.h"
#include "VertexShader.h"

#include <algorithm>
#include <filesystem>
#include <set>
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

    /**
     * \brief The Asset Server is A Global Singleton that is instantiated only
     * once GetInstance retrieves this Object \return
     */
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
    MeshInstance& GetModel(const std::string& name)
    {
        auto it = Models.find(name);
        if (it != Models.end())
            return it->second;
        MeshInstance instance;
        if (!Utils::LoadInstance(MODEL_DIRECTORY + name + ".obj", instance, TextureList))
            instance = MeshInstance{};
        NormalizeModel(instance);
        return Models.emplace(name, std::move(instance)).first->second;
    }

    /**
     * \brief Models come in very different sizes (0.2 to 200 units). Scene
     *        objects treat a model like a shape of footprint 1 x 1 standing
     *        on the ground: centre it on x / z, put its lowest point at y = 0
     *        and scale it uniformly so its widest side is 1 unit. The object's
     *        Transform scale then works like a shape's size
     */
    static void NormalizeModel(MeshInstance& instance)
    {
        if (instance.vertices.empty())
            return;
        Vec3 lo = instance.vertices.front().LocalPosition;
        Vec3 hi = lo;
        for (const Vertex& v : instance.vertices)
        {
            lo = Vec3(std::min(lo.X, v.LocalPosition.X), std::min(lo.Y, v.LocalPosition.Y), std::min(lo.Z, v.LocalPosition.Z));
            hi = Vec3(std::max(hi.X, v.LocalPosition.X), std::max(hi.Y, v.LocalPosition.Y), std::max(hi.Z, v.LocalPosition.Z));
        }
        float width = std::max(hi.X - lo.X, hi.Z - lo.Z);
        if (width <= 0.0f)
            return;
        float scale = 1.0f / width;
        Vec3 origin((lo.X + hi.X) * 0.5f, lo.Y, (lo.Z + hi.Z) * 0.5f);
        for (Vertex& v : instance.vertices)
        {
            // Uniform scale: normals keep their direction
            v.LocalPosition = (v.LocalPosition - origin) * scale;
            v.Position = v.LocalPosition;
        }
    }

    bool IsModelLoaded(const std::string& name) const { return Models.count(name) > 0; }

    /**
     * \brief Names of every model in data/models (sorted), used by the editor
     */
    static std::vector<std::string> AvailableModels()
    {
        std::vector<std::string> names;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(MODEL_DIRECTORY, ec))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".obj")
                names.push_back(entry.path().stem().string());
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    /**
     * \brief Given a texture ID returns a material
     *        When we load a vertex every Vertex is given a texID corresponding
     *        to material it belongs to this method is used to retrieve it
     *        when we Fragment Shade
     * \param texID
     * \return
     */
    Material& GetMaterial(int texID)
    {
        // -1 = no .obj material (procedural shapes). Checked before the
        // assert: comparing -1 with the unsigned size always failed
        if (texID == -1)
            return Material::DefaultMaterial;

        assert(texID >= 0 && static_cast<size_t>(texID) < TextureList.size() &&
               "texID does not exist");

        return TextureList[texID];
    }

    void SetFragShader(Entity e, FragShaderTag& shader)
    {
        shader.FragShaderID = CurrentFragShaderId;

        if (EntityMapToFragShaderID.count(e) > 0)
            RemoveFragShader(e);

        size_t shaderID = shader.FragShaderID;
        EntityMapToFragShaderID[e] = shaderID;
        CurrentFragShaderId++;

        switch (shader.FragAssetId)
        {
        case BlinnPhongID: {
            FragShaders[shaderID] = std::make_shared<BlinnPhongSIMD>();
            return;
        }
        case OutlineShaderID: {
            FragShaders[shaderID] = std::make_shared<OutlineScanShaderSIMD>();
            return;
        }
        case ParticleShaderID: {
            FragShaders[shaderID] = std::make_shared<ParticleShaderSIMD>();
            return;
        }
        case ToonShaderID: {
            FragShaders[shaderID] = std::make_shared<ToonShaderSIMD>();
            return;
        }
        case UnlitShaderID: {
            FragShaders[shaderID] = std::make_shared<UnlitSIMD>();
            return;
        }
        case RedShaderID: {
            FragShaders[shaderID] = std::make_shared<RedShaderSIMD>();
            return;
        }
        case NormalShaderID: {
            FragShaders[shaderID] = std::make_shared<NormalShaderSIMD>();
            return;
        }
        case ShapeShaderID: {
            FragShaders[shaderID] = std::make_shared<ShapeShaderSIMD>();
            return;
        }
        case DefaultFragShaderID: {
            FragShaders[shaderID] = std::make_shared<BlinnPhongSIMD>();
        }
        }
    }

    void SetVertShader(Entity e, VertShaderTag& shader)
    {
        shader.VertShaderID = CurrentVertShaderId;

        if (EntityMapToVertShaderID.count(e) > 0)
            RemoveVertShader(e);

        size_t shaderID = shader.VertShaderID;
        EntityMapToVertShaderID[e] = shaderID;
        CurrentVertShaderId++;

        switch (shader.VertAssetId)
        {
        case DefaultVertShaderID: {
            VertShaders[shaderID] = std::make_shared<DefaultVertexShader>();
            break;
        }
        default:
            assert(false && "Not Possible");
        }
    }

    /**
     * \brief Remove a Shader given a shaderID
     * \param shaderID
     */
    void RemoveFragShader(Entity e)
    {
        assert(EntityMapToFragShaderID.count(e) > 0);
        size_t ShaderId = EntityMapToFragShaderID[e];
        EntityMapToFragShaderID.erase(e);
        assert(FragShaders.count(ShaderId) > 0);
        FragShaders.erase(ShaderId);
    }

    void RemoveVertShader(Entity e)
    {
        assert(EntityMapToVertShaderID.count(e) > 0);
        size_t ShaderId = EntityMapToVertShaderID[e];
        EntityMapToVertShaderID.erase(e);
        assert(VertShaders.count(ShaderId) > 0);
        VertShaders.erase(ShaderId);
    }

    bool HaveFragShader(size_t ShaderID) { return FragShaders.count(ShaderID) == 0; }

    /**
     * \brief Returns a Shader given a shaderID
     * \param shaderID
     */
    std::shared_ptr<FragmentShader> GetFragShader(size_t ShaderID)
    {
        if (FragShaders.count(ShaderID) == 0)
            return defaultFragShader;

        return FragShaders[ShaderID];
    }

    std::shared_ptr<VertexShader> GetVertShader(size_t ShaderID)
    {
        if (VertShaders.count(ShaderID) == 0)
            return defaultVertShader;

        return VertShaders[ShaderID];
    }

    size_t GetCurrentShaderCount() { return FragShaders.size(); }

    /**
     * \brief Default shader of the game
     */
    static std::shared_ptr<FragmentShader> defaultFragShader;
    static std::shared_ptr<VertexShader> defaultVertShader;

  private:
    size_t CurrentFragShaderId = 1;
    size_t CurrentVertShaderId = 1;

    std::vector<Material> TextureList;
    std::unordered_map<std::string, MeshInstance> Models;
    std::unordered_map<Entity, size_t> EntityMapToFragShaderID;
    std::unordered_map<Entity, size_t> EntityMapToVertShaderID;
    std::unordered_map<size_t, std::shared_ptr<FragmentShader>> FragShaders;
    std::unordered_map<size_t, std::shared_ptr<VertexShader>> VertShaders;
};
