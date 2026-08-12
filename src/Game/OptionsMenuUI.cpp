#include "OptionsMenuUI.h"

#include "ColorBuffer.h"
#include "ECSManager.h"
#include "PixelBuffer.h"
#include "Widget.h"
#include "app.h"
#include "stdafx.h"

extern ECSManager ECS;

OptionsMenuUI::OptionsMenuUI()
{
    m_state = ECS.GetResource<UIState>();
    m_gameoptions = ECS.GetResource<GameOptions>();
    m_gamestate = ECS.GetResource<GameState>();
    m_cam = ECS.GetResource<Camera>();
    m_Tiles = ECS.GetResource<Tiles>();
    m_depth = ECS.GetResource<DepthBuffer>();
}

static std::vector<std::string> ShadowQuality = {"Disabled", "Low", "Medium", "High"};

static std::vector<std::string> RenderingType = {"Line", "Raster"};

static std::vector<std::string> RenderingProjection = {"Perspective", "Orthogonal"};

void ChangeResolution(int width, int height)
{
    ECS.GetResource<PixelBuffer>()->ResizePixelBuffer(width, height);
    ECS.GetResource<Tiles>()->SetTiles(width, height);
    ECS.GetResource<DepthBuffer>()->SetDepthSize(width, height);
    ECS.GetResource<ColorBuffer>()->ResizeColorBuffer(width, height);
    ECS.GetResource<Camera>()->ChangeResolution(width, height);
    ECS.GetResource<GameOptions>()->VirtualHeight = height;
    ECS.GetResource<GameOptions>()->VirtualWidth = width;
    ECS.GetResource<GameOptions>()->ScreenRatio = width / height;
}

void OptionsMenuUI::Render()
{
    // Render Container as background
    DrawContainer(50, 50, 500, 700);

    // "Back" button
    if (Button(1, 100, 700, *m_state, 140, 25, "Back"))
    {
        Transform T = Transform({0, 5, -1}, {0.0f, 0.0, 3.0f}, {0, 1, 0});
        m_state->activeItem = 0;
        m_state->hotItem = 0;
        m_state->openDropDownId = 0;
        m_gamestate->LerpToTargetCamera(1.0f, m_cam->CamTransform, T, StartMenu);
    }

    // Common spacing parameters
    float startY = 600.0f;
    float stepY = 100.0f;
    float labelX = 90.0f;
    float dropX = 250.0f;
    float dropW = 150.0f;
    float dropH = 30.0f;

    // 1) Rendering Type
    App::Print(labelX, startY, "Rendering Type", 1, 1, 1);
    DropdownList(
            2, dropX, startY, dropW, dropH, *m_state, RenderingType, m_gameoptions->renderingType);
    m_gameoptions->LineRendering = (m_gameoptions->renderingType == 0);

    // Move down for the next item
    startY -= stepY;

    // 2) Projection
    App::Print(labelX, startY, "Projection", 1, 1, 1);
    int prevProjection = m_gameoptions->renderingProjection;
    DropdownList(3,
                 dropX,
                 startY,
                 dropW,
                 dropH,
                 *m_state,
                 RenderingProjection,
                 m_gameoptions->renderingProjection);
    m_gameoptions->Projection = m_gameoptions->renderingProjection == 0;

    // If the projection type changed, update the camera
    if (m_gameoptions->Projection != prevProjection)
    {
        Vec3 Forward = m_cam->CamTransform.GetForward();
        Vec3 Target = Target - Forward;
        if (m_gameoptions->Projection)
            m_cam->SetProjectionPerspective();
        else
            m_cam->SetProjectionOrthogonal();
    }

    // Move down for the next item
    startY -= stepY;

    // 3) Shadow Quality
    int prevQuality = m_gameoptions->shadowQuality;
    App::Print(labelX, startY, "ShadowQuality", 1, 1, 1);

    // If line rendering is active, disable shadow options
    if (!m_gameoptions->LineRendering)
    {
        DropdownList(4,
                     dropX,
                     startY,
                     dropW,
                     dropH,
                     *m_state,
                     ShadowQuality,
                     m_gameoptions->shadowQuality);

        // Update if changed
        if (prevQuality != m_gameoptions->shadowQuality)
        {
            int currentQuality = m_gameoptions->shadowQuality;
            // Update Shadow Quality based on index
            if (currentQuality == 0)
            {
                m_gameoptions->ShadowMapping = false;
                m_Tiles->SetShadowTiles(0, 0);
                m_depth->SetShadowDepthSize(0, 0);
            }
            else
            {
                m_gameoptions->ShadowMapping = true;
                float VIRTUAL_WIDTH = m_gameoptions->VirtualWidth;
                float VIRTUAL_HEIGHT = m_gameoptions->VirtualHeight;
                // Adjust shadow depth buffer size
                if (currentQuality == 1)
                {
                    m_Tiles->SetShadowTiles(VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
                    m_depth->SetShadowDepthSize(VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
                }
                else if (currentQuality == 2)
                {
                    m_Tiles->SetShadowTiles(5 * VIRTUAL_WIDTH, 5 * VIRTUAL_HEIGHT);
                    m_depth->SetShadowDepthSize(5 * VIRTUAL_WIDTH, 5 * VIRTUAL_HEIGHT);
                }
                else
                {
                    m_Tiles->SetShadowTiles(10 * VIRTUAL_WIDTH, 10 * VIRTUAL_HEIGHT);
                    m_depth->SetShadowDepthSize(10 * VIRTUAL_WIDTH, 10 * VIRTUAL_HEIGHT);
                }
            }
        }
    }
    else
    {
        // Disable shadow if line rendering is used
        m_gameoptions->shadowQuality = 0;
        m_gameoptions->ShadowMapping = false;
        TextLabel(dropX, startY, dropW, dropH, "Disabled", {1.0f, 0.0f, 0.0f}, {0.4f, 0.4f, 0.4f});
    }
}
