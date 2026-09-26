//---------------------------------------------------------------------------------
// Vertex.h
//---------------------------------------------------------------------------------
//
// A Vertex loaded from an obj file contains normals, position, UV coordinates
//
#pragma once

#include "Vec2.h"
#include "Vec3.h"
#include "Vec4.h"

#include <string>

struct Vertex
{
    // Fragment Shader ID of this vertex
    size_t FragShaderID = 0;
    // Vertex shader ID of this vertex
    size_t VertexShaderID = 0;

    // texture ID of the texture that shade this vertex
    int TextureID{};
    Vec3 Color{};
    // texture uv
    Vec2 UV{};
    // vertex in local space
    Vec3 LocalPosition;
    Vec3 LocalNormal;
    // vertex in camera space
    Vec3 PositionCamera;
    // vertex in world space
    Vec3 Position{};
    Vec3 Normal{};
    // 4d homogenous coordinates output by vertex shader
    Vec4 Projection{};
    // inverse w used for perspective corrected interpolation
    float InverseW{};
    // inverse w used for shadow mapping
    float InverseShadowW{};
    // Position of the vertex projected from the light perspective
    Vec4 ShadowProjection;

    Vertex() = default;

    Vertex(const Vec3& pos, const Vec3& normal, const Vec2& tex)
        : UV(tex)
        , LocalPosition(pos)
        , LocalNormal(normal)
        , Position(pos)
        , Normal(normal)
    {
    }

    // used as a hashing function for when we're loading obj files
    std::string ToString()
    {
        return "{" + std::to_string(TextureID) + LocalPosition.ToString() + LocalNormal.ToString() +
               UV.ToString() + "}";
    }

    void PerspectiveDivision()
    {
        InverseW = 1 / Projection.W;
        InverseShadowW = 1 / ShadowProjection.W;
        Projection *= InverseW;
        ShadowProjection *= InverseShadowW;
        UV *= InverseW;
    }

    // Interpolation (clipping): Position, UV and Normal only
    Vertex operator*(const float t) const
    {
        auto copy = *this;
        copy.Position *= t;
        copy.UV *= t;
        copy.Normal *= t;
        return copy;
    }

    Vertex operator+(const Vertex& v) const
    {
        auto copy = *this;
        copy.Position += v.Position;
        copy.UV += v.UV;
        copy.Normal += v.Normal;
        return copy;
    }
};
