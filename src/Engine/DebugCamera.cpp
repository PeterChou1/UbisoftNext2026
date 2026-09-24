#include "DebugCamera.h"

#include "Camera.h"
#include "ECSManager.h"
#include "Lighting.h"
#include "RenderConstants.h"
#include "app.h"
#include "stdafx.h"

extern ECSManager ECS;

DebugCamera::DebugCamera()
{
    m_Cam = ECS.GetResource<Camera>();
}

void DebugCamera::Update(float deltaTime)
{
    Vec3 delta = Vec3(0, 0, 0);
    Quat r = Quat({0, 1, 0}, 0);

    float speed = (deltaTime / 100);
    float rotation = 3.141f / 2.0f * (deltaTime / 1000.0f);
    //
    if (App::GetController().GetLeftThumbStickX() > 0.5f)
    {
        r *= Quat(m_Cam->Up, -rotation);
    }
    if (App::GetController().GetLeftThumbStickX() < -0.5f)
    {
        r *= Quat(m_Cam->Up, rotation);
    }

    if (App::GetController().GetLeftThumbStickY() > 0.5f)
    {
        delta += m_Cam->CamTransform.GetForward() * speed;
    }
    if (App::GetController().GetLeftThumbStickY() < -0.5f)
    {
        delta -= m_Cam->CamTransform.GetForward() * speed;
    }

    if (App::IsKeyPressed(App::KEY_Q))
    {
        delta += Vec3(0, 0.1, 0) * speed * 0.1f;
    }

    if (App::IsKeyPressed(App::KEY_E))
    {
        delta -= Vec3(0, 0.1, 0) * speed * 0.1f;
    }

    m_Cam->UpdatePos(delta, r);
}

void DebugCamera::Render()
{
    std::string pos = "Position x:" + std::to_string(m_Cam->Position.X) +
                      " y: " + std::to_string(m_Cam->Position.Y) +
                      " z: " + std::to_string(m_Cam->Position.Z);
    App::Print(100, 50, pos.c_str(), 1, 0, 0);
}
