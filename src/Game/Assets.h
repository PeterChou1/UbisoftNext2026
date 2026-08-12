//---------------------------------------------------------------------------------
// Asset.h
//---------------------------------------------------------------------------------
//
// Defines Enum Used by the AssetServer to locate where an asset is located
//
#pragma once

// Graphical Assets .obj files
enum ObjAsset
{
    GoalPost,
    TitleScreenBackground,
    ExplosionBall,
    BaseTank,
    CannonTank,
    Bullet,
    // MetalInvasion
    CrystalAsset,
    Laser,
    Ground,
    PlayerBase,
    SoldierUnitAsset,
    SupportUnitAsset,
    BasicEnemy,
    TargetUISelector,
    ObstacleWall,
    WordLogo
};

// Shaders -> See SIMDShader.h for more details
enum FragShaderTypeID
{
    DefaultFragShaderID,
    BlinnPhongID,
    OutlineShaderID,
    ParticleShaderID,
    ToonShaderID,
    UnlitShaderID,
    RedShaderID,
    NormalShaderID
};

enum VertShaderTypeID
{
    DefaultVertShaderID
};