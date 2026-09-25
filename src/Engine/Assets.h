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
    ShapeShaderID,
    // Effect shaders for shapes and models (EffectShadersSIMD.h); new values
    // go at the end: scene files store the number
    PulseShaderID,
    RimShaderID,
    StripesShaderID
};

enum VertShaderTypeID
{
    DefaultVertShaderID,
    // Animated vertex shaders (EffectVertexShaders.h)
    WaveVertShaderID,
    SwayVertShaderID
};