//---------------------------------------------------------------------------------
// Emitter.h
//---------------------------------------------------------------------------------
//
// Particle emitters and the particles they spawn (see ParticleSystem)
//
#pragma once

#include "Vec3.h"

struct Particle
{
    Vec3 direction;
    Vec3 Color;
    // Its quad was created by the MeshHandler
    bool loaded;
    // Seconds left
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
    // particles emitted per frame
    int density = 1;
    // seconds the emitter emits for
    float duration = 1.0f;
    // speed of each particle (saved, not used by the ParticleSystem)
    float speed = 1.0f;
    // size of each particle
    float size = 1.0f;
    // seconds each particle lasts
    float particleTime = 1.0f;
    // half angle of the cone (degrees)
    float coneAngle;
    Vec3 direction;
    Vec3 color;
};
