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
#include "ToonShaderSIMD.h"
#include "UnlitSIMD.h"
#include "Utils.h"
#include "VertShaderTag.h"
#include "VertexShader.h"

#include <set>
#include <unordered_map>

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

    /**
     * \brief Clears the loaded geometry and loads geometry and associated
     *        textures from the AssetList
     * \param AssetList Specify which assets loaded
     */
    void LoadLevelAssets(const std::set<ObjAsset>& AssetList)
    {
        // clear previous loaded assets
        Assets.clear();
        TextureList.clear();
        for (auto objID : AssetList)
        {
            MeshInstance instance;
            const bool success = Utils::LoadInstance(LookUpFilePath(objID), instance, TextureList);
            assert(success && "failed to load assets");
            Assets[objID] = instance;
        }
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
        assert(texID <= TextureList.size() && "texID does not exist");

        if (texID == -1)
            return Material::DefaultMaterial;

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
        case DefaultFragShaderID: {
            FragShaders[shaderID] = std::make_shared<BlinnPhongSIMD>();
        }
        }
    }

    void SetVertShader(Entity e, VertShaderTag& shader)
    {
        shader.VertShaderID = CurrentVertShaderId;

        if (EntityMapToFragShaderID.count(e) > 0)
            RemoveFragShader(e);

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
     * \brief Returns a Mesh Obj given its ID
     *        NOTE: this objID must be loaded with LoadLevelAssets
     *              or else it will throw an error
     * \return
     */
    MeshInstance& GetObj(ObjAsset objID)
    {
        assert(Assets.find(objID) != Assets.end() && "Obj Not Loaded");
        return Assets[objID];
    }

    /**
     * \brief Default shader of the game
     */
    static std::shared_ptr<FragmentShader> defaultFragShader;
    static std::shared_ptr<VertexShader> defaultVertShader;

  private:
    std::string LookUpFilePath(ObjAsset asset)
    {
        switch (asset)
        {
        case ObstacleWall:
            return "data/ObstaclesWall.obj";
        case SupportUnitAsset:
            return "data/SupportUnit.obj";
        case CrystalAsset:
            return "data/Crystal.obj";
        case Laser:
            return "data/Laser.obj";
        case BasicEnemy:
            return "data/Enemy.obj";
        case TargetUISelector:
            return "data/TargetUI.obj";
        case SoldierUnitAsset:
            return "data/Soldier.obj";
        case PlayerBase:
            return "data/PlayerBase2.obj";
        case Ground:
            return "data/FlatGround.obj";
        case WordLogo:
            return "data/MetalInvasionLogo.obj";
        case GoalPost:
            return "data/GoalFlag.obj";
        case TitleScreenBackground:
            return "data/TitleScreenBackground.obj";
        case ExplosionBall:
            return "data/Explosion.obj";
        case BaseTank:
            return "data/Base.obj";
        case CannonTank:
            return "data/Cannon.obj";
        case Bullet:
            return "data/Bullet.obj";
        default:
            assert(false && "not possible");
        }
        return "";
    }

    size_t CurrentFragShaderId = 1;
    size_t CurrentVertShaderId = 1;

    std::vector<Material> TextureList;
    std::unordered_map<ObjAsset, MeshInstance> Assets;
    std::unordered_map<Entity, size_t> EntityMapToFragShaderID;
    std::unordered_map<Entity, size_t> EntityMapToVertShaderID;
    std::unordered_map<size_t, std::shared_ptr<FragmentShader>> FragShaders;
    std::unordered_map<size_t, std::shared_ptr<VertexShader>> VertShaders;
};
