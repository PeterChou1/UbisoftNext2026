#include "RasterizerSystem.h"

#include "AssetServer.h"
#include "Concurrent.h"
#include "ECSManager.h"
#include "FragmentShader.h"
#include "SIMDTriangle.h"
#include "app.h"
#include "stdafx.h"

#include <algorithm>
#include <cmath>

extern ECSManager ECS;

RasterizerSystem::RasterizerSystem()
{
    m_ClippedTriangle = ECS.GetResource<ClippedTriangleBuffer>();
    m_Tiles = ECS.GetResource<Tiles>();
    m_PixelBuffer = ECS.GetResource<PixelBuffer>();
    m_DepthBuffer = ECS.GetResource<DepthBuffer>();
    m_RenderConstants = ECS.GetResource<RenderConstants>();
    m_GameOptions = ECS.GetResource<GameOptions>();
    m_Camera = ECS.GetResource<Camera>();
    m_Lighting = ECS.GetResource<Lighting>();
}

void RasterizerSystem::Rasterize()
{
    if (m_GameOptions->LineRendering)
    {
        RenderLine();
        return;
    }
    AssignTile();
    RasterizeTiles();
    m_PixelBuffer->AccumulatePixel(*m_DepthBuffer);
}

void RasterizerSystem::RenderLine()
{
    // Hardware triangles: no per pixel work, so each triangle's fragment
    // shader is run at its three corners (in one 8 wide call) and the GPU
    // blends the corner colours. Every shader (Shape, Lit, Pulse, Rim ...)
    // shows, with per corner detail; the software rasterizer (Tab) shades
    // every pixel and draws shadows
    AssetServer& loader = AssetServer::GetInstance();
    DirectionalLight& light = m_Lighting->GetDirectionalLight();

    for (auto& binnedTriangles : m_ClippedTriangle->CameraClipBuffer)
    {
        for (auto& tri : binnedTriangles)
        {
            SIMDPixel corners(SIMDVec2(0.0f, 0.0f),
                              SIMD::ONE,
                              SIMD::ZERO,
                              SIMD::ZERO,
                              SIMD::ZERO,
                              tri.BinID,
                              tri.BinIndex);
            for (int lane = 0; lane < SIMDPixel::PIXEL_WIDTH * SIMDPixel::PIXEL_HEIGHT; ++lane)
            {
                const Vertex& v = tri.verts[std::min(lane, 2)];
                corners.WorldSpacePosition.X.V[lane] = v.Position.X;
                corners.WorldSpacePosition.Y.V[lane] = v.Position.Y;
                corners.WorldSpacePosition.Z.V[lane] = v.Position.Z;
                corners.Normal.X.V[lane] = v.Normal.X;
                corners.Normal.Y.V[lane] = v.Normal.Y;
                corners.Normal.Z.V[lane] = v.Normal.Z;
                corners.VertexColor.X.V[lane] = v.Color.X;
                corners.VertexColor.Y.V[lane] = v.Color.Y;
                corners.VertexColor.Z.V[lane] = v.Color.Z;
                corners.TextureCoord.X.V[lane] = v.UV.X;
                corners.TextureCoord.Y.V[lane] = v.UV.Y;
            }
            corners.Mask = SIMD::ONE;
            Material& material = loader.GetMaterial(tri.GetTextureID());
            loader.GetFragShader(tri.GetShaderID())
                    ->Shade(corners, *m_DepthBuffer, material, *m_Camera, light);

            Vec3 colors[3];
            for (int i = 0; i < 3; ++i)
            {
                colors[i] = Vec3(std::clamp(corners.Color.X.V[i], 0.0f, 1.0f),
                                 std::clamp(corners.Color.Y.V[i], 0.0f, 1.0f),
                                 std::clamp(corners.Color.Z.V[i], 0.0f, 1.0f));
            }
            App::DrawTriangle(tri.verts[0].Projection.X,
                              tri.verts[0].Projection.Y,
                              tri.verts[0].Projection.Z,
                              tri.verts[0].Projection.W,
                              tri.verts[1].Projection.X,
                              tri.verts[1].Projection.Y,
                              tri.verts[1].Projection.Z,
                              tri.verts[1].Projection.W,
                              tri.verts[2].Projection.X,
                              tri.verts[2].Projection.Y,
                              tri.verts[2].Projection.Z,
                              tri.verts[2].Projection.W,
                              colors[0].X,
                              colors[0].Y,
                              colors[0].Z,
                              colors[1].X,
                              colors[1].Y,
                              colors[1].Z,
                              colors[2].X,
                              colors[2].Y,
                              colors[2].Z,
                              false);
        }
    }
}

void RasterizerSystem::RasterizeTriangle(
        Triangle& tri, Vec2& tileMin, Vec2& tileMax, bool shadows, bool perspective)
{
    // construct the triangles bounding box to loop through this faster
    auto minPt = Vec2((std::min)((std::max)(tileMin.X, static_cast<float>(tri.minX)),
                                 static_cast<float>(tri.maxX)),
                      (std::min)((std::max)(tileMin.Y, static_cast<float>(tri.minY)),
                                 static_cast<float>(tri.maxY)));

    auto maxPt = Vec2((std::max)((std::min)(tileMax.X, static_cast<float>(tri.maxX)),
                                 static_cast<float>(tri.minX)),
                      (std::max)((std::min)(tileMax.Y, static_cast<float>(tri.maxY)),
                                 static_cast<float>(tri.minY)));

    // iterate over pixels to determine which pixel belong in the triangle
    // Aligning pixel tile boundaries
    minPt.X = std::floor(minPt.X / SIMDPixel::PIXEL_WIDTH) * SIMDPixel::PIXEL_WIDTH;
    minPt.Y = std::floor(minPt.Y / SIMDPixel::PIXEL_HEIGHT) * SIMDPixel::PIXEL_HEIGHT;
    maxPt.X = std::ceil(maxPt.X / SIMDPixel::PIXEL_WIDTH) * SIMDPixel::PIXEL_WIDTH;
    maxPt.Y = std::ceil(maxPt.Y / SIMDPixel::PIXEL_HEIGHT) * SIMDPixel::PIXEL_HEIGHT;

    SIMDTriangle triSIMD(tri);

    SIMDFloat posX = SIMDPixel::PixelOffsetX + minPt.X;
    SIMDFloat posY = SIMDPixel::PixelOffsetY + minPt.Y;

    SIMDFloat deltaX0 = triSIMD.B0 * SIMDPixel::PIXEL_WIDTH;
    SIMDFloat deltaX1 = triSIMD.B1 * SIMDPixel::PIXEL_WIDTH;
    SIMDFloat deltaX2 = triSIMD.B2 * SIMDPixel::PIXEL_WIDTH;
    SIMDFloat deltaY0 = triSIMD.C0 * SIMDPixel::PIXEL_HEIGHT;
    SIMDFloat deltaY1 = triSIMD.C1 * SIMDPixel::PIXEL_HEIGHT;
    SIMDFloat deltaY2 = triSIMD.C2 * SIMDPixel::PIXEL_HEIGHT;

    SIMDVec2 base = SIMDVec2(posX, posY);
    SIMDFloat e1 = triSIMD.EdgeFunc0(base);
    SIMDFloat e2 = triSIMD.EdgeFunc1(base);
    SIMDFloat e3 = triSIMD.EdgeFunc2(base);

    for (int y = static_cast<int>(minPt.Y); y < static_cast<int>(maxPt.Y);
         y += SIMDPixel::PIXEL_HEIGHT)
    {
        SIMDFloat deltaXe1 = e1;
        SIMDFloat deltaXe2 = e2;
        SIMDFloat deltaXe3 = e3;
        posX = SIMDPixel::PixelOffsetX + minPt.X;
        for (int x = static_cast<int>(minPt.X); x < static_cast<int>(maxPt.X);
             x += SIMDPixel::PIXEL_WIDTH)
        {
            SIMDFloat inTriangle = deltaXe1 <= 0.0f & deltaXe2 <= 0.0f & deltaXe3 <= 0.0f;

            if (SIMD::Any(inTriangle))
            {
                int pixelX = x / SIMDPixel::PIXEL_WIDTH;
                int pixelY = y / SIMDPixel::PIXEL_HEIGHT;

                SIMDFloat alpha, beta, gamma;
                triSIMD.ComputeBarycentric(posX, posY, alpha, beta, gamma);

                SIMDFloat depth;
                if (perspective)
                {
                    depth = alpha * triSIMD.InvW1 + beta * triSIMD.InvW2 + gamma * triSIMD.InvW3;
                }
                else
                {
                    // (only the parallel light's shadow map is orthographic)
                    // NDC z is -1 (near) .. 1 (far). The depth buffers keep the
                    // largest value and start at 0 ("nothing"): the shadow
                    // map stores (1 - z) / 2, 1 (near) .. 0 (far), so the far
                    // half of a parallel light's box is not lost
                    depth = alpha * tri.verts[0].Projection.Z + beta * tri.verts[1].Projection.Z +
                            gamma * tri.verts[2].Projection.Z;
                    depth = (SIMD::ONE - depth) * 0.5f;
                }

                SIMDFloat visible =
                        m_DepthBuffer->DepthTest(pixelX, pixelY, depth, inTriangle, shadows);
                if (SIMD::Any(visible))
                {
                    m_DepthBuffer->UpdateBuffer(pixelX, pixelY, visible, depth, shadows);
                    // The shading is deferred to the fragment shading stage
                    if (!shadows)
                    {
                        SIMDPixel pixel(SIMDVec2(posX, posY),
                                        depth,
                                        alpha,
                                        beta,
                                        gamma,
                                        tri.BinID,
                                        tri.BinIndex);
                        m_PixelBuffer->SetBuffer(pixelX, pixelY, pixel, visible);
                    }
                }
            }
            deltaXe1 = deltaXe1 + deltaX0;
            deltaXe2 = deltaXe2 + deltaX1;
            deltaXe3 = deltaXe3 + deltaX2;
            posX = posX + static_cast<float>(SIMDPixel::PIXEL_WIDTH);
        }
        e1 = e1 - deltaY0;
        e2 = e2 - deltaY1;
        e3 = e3 - deltaY2;
        posY = posY + static_cast<float>(SIMDPixel::PIXEL_HEIGHT);
    }
}

void RasterizerSystem::AssignTriangle(Triangle& tri,
                                      std::vector<Tile>& tiles,
                                      unsigned int binID,
                                      bool shadow)
{
    const int startX = tri.minX / TILE_SIZE_X;
    const int endX = (tri.maxX + TILE_SIZE_X - 1) / TILE_SIZE_X;
    const int startY = tri.minY / TILE_SIZE_Y;
    const int endY = (tri.maxY + TILE_SIZE_Y - 1) / TILE_SIZE_Y;
    const int tileCountX = shadow ? m_Tiles->ShadowTileCountX : m_Tiles->TileCountX;
    for (int x = startX; x < endX; x++)
    {
        for (int y = startY; y < endY; y++)
        {
            const int index = y * tileCountX + x;

            if (index >= tiles.size())
                continue;

            Tile& t = tiles[index];

            Vec2 corner0 = t.GetCorner(tri.rejectIndex0);
            Vec2 corner1 = t.GetCorner(tri.rejectIndex1);
            Vec2 corner2 = t.GetCorner(tri.rejectIndex2);

            const float e0 = tri.EdgeFunc0(corner0);
            const float e1 = tri.EdgeFunc1(corner1);
            const float e2 = tri.EdgeFunc2(corner2);

            // trivial reject
            if (e0 > 0 || e1 > 0 || e2 > 0)
                continue;

            t.AddTri(binID, tri);
        }
    }
}

void RasterizerSystem::AssignTile()
{
    const std::vector<std::uint32_t>& coreIds = m_RenderConstants->CoreIds;
    std::vector<Tile>& camTiles = m_Tiles->TilesArray;
    std::vector<Tile>& shadowTiles = m_Tiles->ShadowTilesArray;
    bool shadowMap = m_GameOptions->ShadowsOn();
    Concurrent::ForEach(coreIds.begin(), coreIds.end(), [&](unsigned int binID) {
        for (auto& tri : m_ClippedTriangle->CameraClipBuffer[binID])
            AssignTriangle(tri, camTiles, binID, false);
        if (!shadowMap)
            return;
        for (auto& tri : m_ClippedTriangle->LightClipBuffer[binID])
            AssignTriangle(tri, shadowTiles, binID, true);
    });
}

void RasterizerSystem::RasterizeTiles()
{
    auto rasterize = [&](std::vector<Tile>& tiles, bool shadows, bool perspective) {
        Concurrent::ForEach(tiles.begin(), tiles.end(), [&](Tile& tile) {
            Vec2 tileMin = tile.GetMin();
            Vec2 tileMax = tile.GetMax();
            for (auto& binTriangles : tile.GetBinTriangle())
            {
                for (auto& tri : binTriangles)
                    RasterizeTriangle(tri, tileMin, tileMax, shadows, perspective);
            }
        });
    };
    // Camera: perspective, hidden surfaces removed by the depth buffer
    rasterize(m_Tiles->TilesArray, false, true);
    // Shadow map: from the light
    if (m_GameOptions->ShadowsOn())
        rasterize(m_Tiles->ShadowTilesArray, true, m_Lighting->IsPerspective());
}
