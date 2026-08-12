#include "RasterizerSystem.h"

#include "AssetServer.h"
#include "Concurrent.h"
#include "ECSManager.h"
#include "SIMDTriangle.h"
#include "stdafx.h"

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

    std::vector<unsigned int> coreID = m_RenderConstants->CoreIds;
    AssetServer& loader = AssetServer::GetInstance();

    std::for_each(coreID.begin(), coreID.end(), [&](unsigned int binID) {
        std::vector<Triangle>& binnedTriangles = m_ClippedTriangle->CameraClipBuffer[binID];
        for (auto& tri : binnedTriangles)
        {
            Material& texture = loader.GetMaterial(tri.GetTextureID());
            Vec3 Color = texture.diffuse;
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
                              Color.X,
                              Color.Y,
                              Color.Z,
                              Color.X,
                              Color.Y,
                              Color.Z,
                              Color.X,
                              Color.Y,
                              Color.Z,
                              false);
            // App::DrawLine(tri.verts[0].Projection.X, tri.verts[0].Projection.Y,
            //               tri.verts[1].Projection.X, tri.verts[1].Projection.Y);
            // App::DrawLine(tri.verts[1].Projection.X, tri.verts[1].Projection.Y,
            //               tri.verts[2].Projection.X, tri.verts[2].Projection.Y);
            // App::DrawLine(tri.verts[2].Projection.X, tri.verts[2].Projection.Y,
            //               tri.verts[1].Projection.X, tri.verts[1].Projection.Y);
        }
    });
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
                    depth = alpha * tri.verts[0].Projection.Z + beta * tri.verts[1].Projection.Z +
                            gamma * tri.verts[2].Projection.Z;
                    depth = depth * -1;
                }

                SIMDPixel pixel = SIMDPixel(
                        SIMDVec2(posX, posY), depth, alpha, beta, gamma, tri.BinID, tri.BinIndex);
                SIMDFloat visible =
                        m_DepthBuffer->DepthTest(pixelX, pixelY, depth, inTriangle, shadows);

                if (SIMD::Any(visible))
                {
                    m_DepthBuffer->UpdateBuffer(pixelX, pixelY, visible, depth, shadows);
                    // We deferred the shading to the fragment shading stage
                    if (!shadows)
                        m_PixelBuffer->SetBuffer(pixelX, pixelY, pixel, visible);
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
    int tileCountX = shadow ? m_Tiles->TILE_S_COUNT_X : m_Tiles->TILE_COUNT_X;
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
    std::vector<unsigned int> coreID = m_RenderConstants->CoreIds;
    std::vector<Tile>& camTiles = m_Tiles->TilesArray;
    std::vector<Tile>& shadowTiles = m_Tiles->ShadowTilesArray;
    bool shadowMap = m_GameOptions->ShadowMapping;
    Concurrent::ForEach(coreID.begin(), coreID.end(), [&](unsigned int binID) {
        std::vector<Triangle>& binCamTriangles = m_ClippedTriangle->CameraClipBuffer[binID];
        std::vector<Triangle>& binLightTriangles = m_ClippedTriangle->LightClipBuffer[binID];
        for (auto& tri : binCamTriangles)
        {
            // iterate through triangle bounding box
            AssignTriangle(tri, camTiles, binID, false);
        }
        if (!shadowMap)
            return;
        for (auto& tri : binLightTriangles)
        {
            AssignTriangle(tri, shadowTiles, binID, true);
        }
    });
}

void RasterizerSystem::RasterizeTiles()
{
    std::vector<Tile>& camTiles = m_Tiles->TilesArray;
    std::vector<Tile>& shadowTiles = m_Tiles->ShadowTilesArray;
    bool perspective = m_Lighting->IsPerspective();

    Concurrent::ForEach(camTiles.begin(), camTiles.end(), [&](Tile& tile) {
        std::vector<std::vector<Triangle>>& binTriangles = tile.GetBinTriangle();
        Vec2 tileMin = tile.GetMin();
        Vec2 tileMax = tile.GetMax();
        for (auto& binTriangle : binTriangles)
        {
            /// Perspective Projection
            /// Hidden Surface is determined by: Depth Buffer Algorithmn
            for (auto& tri : binTriangle)
            {
                RasterizeTriangle(tri, tileMin, tileMax, false, m_GameOptions->Projection);
            }
        }
    });
    if (!m_GameOptions->ShadowMapping)
        return;

    Concurrent::ForEach(shadowTiles.begin(), shadowTiles.end(), [&](Tile& tile) {
        std::vector<std::vector<Triangle>>& binTriangles = tile.GetBinTriangle();
        Vec2 tileMin = tile.GetMin();
        Vec2 tileMax = tile.GetMax();
        // Rasterizer Light Camera Perspective
        for (auto& binTriangle : binTriangles)
        {
            for (auto& tri : binTriangle)
            {
                RasterizeTriangle(tri, tileMin, tileMax, true, perspective);
            }
        }
    });
}
