#pragma once
#include "Assets.h"
#include "ColliderCategory.h"
#include "Entity.h"
#include "Quat.h"
#include "Transform.h"
#include "Vec3.h"

float WrapAngle(float angle);

float SmoothInterpolate(Vec3& Delta, Transform& T, float TurnSpeed, float DeltaTime);

Entity CreateRigidBodyRect(float x,
                           float y,
                           float width,
                           float height,
                           float rotation,
                           ColliderCategory category,
                           bool collidable = true,
                           bool AI = false);

void CreateCircleWall(float centerX,
                      float centerY,
                      float circleRadius,
                      float wallThickness,
                      int segments,
                      float openingAngleDeg,
                      ColliderCategory category);

Entity
CreateMeshEntity(Vec3 Position, ObjAsset meshID, Quat Rotate = Quat(), Vec3 Scale = {1, 1, 1});

void CreateObstacleLines(Vec3 Location, int linecount, bool horizontal, float spacing = 1.0f);

void CreateMaze(Vec3 origin, int rows, int cols);