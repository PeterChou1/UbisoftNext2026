//---------------------------------------------------------------------------------
// CameraPickingTests.cpp
//---------------------------------------------------------------------------------
//
// Camera::ScreenSpaceToWorldPoint must be the exact inverse of
// WorldPointToScreenSpace (the projection the renderer uses). The scene
// editor, unit move orders and wall placement all turn the mouse position
// into a ground point with it.
//
#include "Camera.h"
#include "TestFramework.h"

#include <cmath>

namespace
{
    struct Pose
    {
        const char* Name;
        Vec3 Position;
        Vec3 Target;
    };

    // Main level, editor (default and zoomed / panned) and a steep top down view
    const Pose POSES[] = {
            {"main level", {0, 10, -5}, {0, 0, 0}},
            {"editor", Vec3(0, 1, -0.55f).Normalize() * 34.0f, {0, 0, 0}},
            {"editor panned", Vec3(8, 0, -6) + Vec3(0, 1, -0.55f).Normalize() * 15.0f, {8, 0, -6}},
            {"top down", {0.5f, 30, -0.5f}, {0, 0, 0}},
    };
} // namespace

TEST_CASE("Camera: screen to ground is the inverse of world to screen")
{
    for (const Pose& pose : POSES)
    {
        Camera camera;
        camera.SetProjectionPerspective();
        camera.SetPositionAndOrientation(pose.Position, pose.Target, {0, 1, 0});

        for (float x = -12.0f; x <= 12.0f; x += 4.0f)
        {
            for (float z = -8.0f; z <= 12.0f; z += 4.0f)
            {
                Vec3 world(pose.Target.X + x, 0.0f, pose.Target.Z + z);
                Vec2 screen = camera.WorldPointToScreenSpace(world);
                if (screen.X < 0 || screen.Y < 0 || screen.X > APP_VIRTUAL_WIDTH ||
                    screen.Y > APP_VIRTUAL_HEIGHT)
                    continue; // not visible from this pose

                Vec3 planePoint(0, 0, 0);
                Vec3 planeNormal(0, 1, 0);
                Vec3 ground =
                        camera.ScreenSpaceToWorldPoint(screen.X, screen.Y, planePoint, planeNormal);
                // Screen coordinates are rounded to whole pixels by the renderer,
                // allow the matching world distance
                float error = std::sqrt((ground.X - world.X) * (ground.X - world.X) +
                                        (ground.Z - world.Z) * (ground.Z - world.Z));
                ++TestFramework::TotalChecks();
                if (!(error < 0.2f) || std::fabs(ground.Y) > 1e-3f)
                {
                    TestFramework::ReportFailure(__FILE__,
                                                 __LINE__,
                                                 std::string(pose.Name) + ": (" +
                                                         std::to_string(world.X) + ", " +
                                                         std::to_string(world.Z) + ") picked as (" +
                                                         std::to_string(ground.X) + ", " +
                                                         std::to_string(ground.Z) + ")");
                }
            }
        }
    }
}
