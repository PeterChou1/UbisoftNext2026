#include "AssetServer.h"

#include "BlingPhong.h"
#include "DefaultVertexShader.h"
#include "EffectShadersSIMD.h"
#include "EffectVertexShaders.h"
#include "Log.h"
#include "NormalShaderSIMD.h"
#include "OutlineShaderSIMD.h"
#include "ParticleShaderSIMD.h"
#include "RedShaderSIMD.h"
#include "ShapeShaderSIMD.h"
#include "ToonShaderSIMD.h"
#include "UnlitSIMD.h"
#include "Utils.h"
#include "stdafx.h"

#include <algorithm>
#include <cassert>
#include <filesystem>

std::shared_ptr<FragmentShader> AssetServer::DefaultFragShader = std::make_shared<BlinnPhongSIMD>();
std::shared_ptr<VertexShader> AssetServer::DefaultVertShader =
        std::make_shared<DefaultVertexShader>();

namespace
{
    /**
     * \brief Models come in very different sizes (0.2 to 200 units). Scene
     *        objects treat a model like a shape of footprint 1 x 1 standing
     *        on the ground: centre it on x / z, put its lowest point at y = 0
     *        and scale it uniformly so its widest side is 1 unit. The object's
     *        Transform scale then works like a shape's size
     */
    void NormalizeModel(MeshInstance& instance)
    {
        if (instance.vertices.empty())
            return;
        Vec3 lo = instance.vertices.front().LocalPosition;
        Vec3 hi = lo;
        for (const Vertex& v : instance.vertices)
        {
            lo = Vec3(std::min(lo.X, v.LocalPosition.X),
                      std::min(lo.Y, v.LocalPosition.Y),
                      std::min(lo.Z, v.LocalPosition.Z));
            hi = Vec3(std::max(hi.X, v.LocalPosition.X),
                      std::max(hi.Y, v.LocalPosition.Y),
                      std::max(hi.Z, v.LocalPosition.Z));
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

    // A new instance of a fragment shader type (nullptr for an unknown type)
    std::shared_ptr<FragmentShader> MakeFragShader(FragShaderTypeID type)
    {
        switch (type)
        {
        case DefaultFragShaderID:
        case BlinnPhongID:
            return std::make_shared<BlinnPhongSIMD>();
        case OutlineShaderID:
            return std::make_shared<OutlineScanShaderSIMD>();
        case ParticleShaderID:
            return std::make_shared<ParticleShaderSIMD>();
        case ToonShaderID:
            return std::make_shared<ToonShaderSIMD>();
        case UnlitShaderID:
            return std::make_shared<UnlitSIMD>();
        case RedShaderID:
            return std::make_shared<RedShaderSIMD>();
        case NormalShaderID:
            return std::make_shared<NormalShaderSIMD>();
        case ShapeShaderID:
            return std::make_shared<ShapeShaderSIMD>();
        case PulseShaderID:
            return std::make_shared<PulseShaderSIMD>();
        case RimShaderID:
            return std::make_shared<RimShaderSIMD>();
        case StripesShaderID:
            return std::make_shared<StripesShaderSIMD>();
        }
        return nullptr;
    }

    std::shared_ptr<VertexShader> MakeVertShader(VertShaderTypeID type)
    {
        switch (type)
        {
        case DefaultVertShaderID:
            return std::make_shared<DefaultVertexShader>();
        case WaveVertShaderID:
            return std::make_shared<WaveVertexShader>();
        case SwayVertShaderID:
            return std::make_shared<SwayVertexShader>();
        }
        assert(false && "Not Possible");
        return nullptr;
    }
} // namespace

MeshInstance& AssetServer::GetModel(const std::string& name)
{
    auto it = m_Models.find(name);
    if (it != m_Models.end())
        return it->second;
    MeshInstance instance;
    if (!Utils::LoadInstance(MODEL_DIRECTORY + name + ".obj", instance, m_Materials))
    {
        LOG_WARN("Assets", "Model '%s' could not be loaded from %s", name.c_str(), MODEL_DIRECTORY);
        instance = MeshInstance{};
    }
    else
        LOG_TRACE(
                "Assets", "Loaded model %s (%zu vertices)", name.c_str(), instance.vertices.size());
    NormalizeModel(instance);
    return m_Models.emplace(name, std::move(instance)).first->second;
}

std::vector<std::string> AssetServer::AvailableModels()
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

Material& AssetServer::GetMaterial(int texID)
{
    // -1 = no .obj material (procedural shapes). Checked before the
    // assert: comparing -1 with the unsigned size always failed
    if (texID == -1)
        return Material::DefaultMaterial;

    assert(texID >= 0 && static_cast<size_t>(texID) < m_Materials.size() && "texID does not exist");
    return m_Materials[texID];
}

void AssetServer::SetFragShader(Entity e, FragShaderTag& shader)
{
    shader.FragShaderID = m_FragShaders.Add(e, MakeFragShader(shader.FragAssetId));
}

void AssetServer::SetVertShader(Entity e, VertShaderTag& shader)
{
    shader.VertShaderID = m_VertShaders.Add(e, MakeVertShader(shader.VertAssetId));
}

void AssetServer::RemoveFragShader(Entity e)
{
    m_FragShaders.Remove(e);
}

void AssetServer::RemoveVertShader(Entity e)
{
    m_VertShaders.Remove(e);
}

template <typename Shader>
size_t AssetServer::ShaderInstances<Shader>::Add(Entity e, std::shared_ptr<Shader> shader)
{
    if (EntityToId.count(e) > 0)
        Remove(e);
    const size_t id = NextId++;
    EntityToId[e] = id;
    // An unknown type gets an ID without an instance: it draws with the default
    if (shader)
        Shaders[id] = std::move(shader);
    return id;
}

template <typename Shader>
void AssetServer::ShaderInstances<Shader>::Remove(Entity e)
{
    assert(EntityToId.count(e) > 0);
    const size_t id = EntityToId[e];
    EntityToId.erase(e);
    assert(Shaders.count(id) > 0);
    Shaders.erase(id);
}
