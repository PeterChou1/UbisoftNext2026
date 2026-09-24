//---------------------------------------------------------------------------------
// Asset.h
//---------------------------------------------------------------------------------
//
// Shader identifiers used by the AssetServer
//
#pragma once

// 3D models are referenced by name (the .obj file name without extension in
// data/models), see AssetServer::GetModel

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
    NormalShaderID,
    // Lit shader using the vertex colour (procedural 2D shapes)
    ShapeShaderID
};

enum VertShaderTypeID
{
    DefaultVertShaderID
};