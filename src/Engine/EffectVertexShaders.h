//---------------------------------------------------------------------------------
// EffectVertexShaders.h
//---------------------------------------------------------------------------------
//
// Animated vertex shaders. They move each vertex before projecting it, like
// DefaultVertexShader, without changing the vertex buffer: v.Position stays
// the undisplaced world position (the MeshHandler only rewrites it when the
// object moves), so the offset never accumulates.
//
//   WaveVertexShader  the surface bobs up and down in a travelling wave
//   SwayVertexShader  the object sways from side to side, more at the top
//                     (grass, flags, trees): the offset grows with the model
//                     height (LocalPosition.Y)
//
// DeltaTime is the time the shader has run (seconds), advanced by the
// ShaderHandler.
//
#pragma once
#include "VertexShader.h"

class WaveVertexShader : public VertexShader
{
  public:
    static constexpr float AMPLITUDE = 0.35f;
    static constexpr float FREQUENCY = 0.8f; // waves per world unit (radians)
    static constexpr float SPEED = 3.0f;

    /**
     * \brief World position of a vertex after the wave at time t
     */
    static Vec3 Displace(const Vertex& v, float t);

    void Shade(Vertex& v, Camera& cam, DirectionalLight& light) override;
};

class SwayVertexShader : public VertexShader
{
  public:
    static constexpr float STRENGTH = 0.25f; // offset per unit of height
    static constexpr float SPEED = 2.0f;

    static Vec3 Displace(const Vertex& v, float t);

    void Shade(Vertex& v, Camera& cam, DirectionalLight& light) override;
};
