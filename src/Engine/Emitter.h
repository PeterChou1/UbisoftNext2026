#pragma once
#include "Vec3.h"

struct Particle
{
    Vec3 direction;
    Vec3 Color;
    bool loaded;
    float duration;
};

enum EmitterType
{
    Sphere,
    Cone
};

struct Emitter
{
    EmitterType emitterType = Sphere;
    // specifies how many particles the emitter will emit
    int density = 1.0f;
    // specifies how long the emitter will emit for
    float duration = 1.0f;
    // specify the speed of each individual particle
    float speed = 1.0f;
    // specify the size of each particle
    float size = 1.0f;
    // specifies how long each particle will last
    float particleTime = 1.0f;
    // angle of cone
    float coneAngle;
    Vec3 direction;
    Vec3 color;
    Emitter() = default;
};
