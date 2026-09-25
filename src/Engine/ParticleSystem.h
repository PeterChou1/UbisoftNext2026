//---------------------------------------------------------------------------------
// ParticleSystem.h
//---------------------------------------------------------------------------------
//
// Moves the particles and spawns new ones from the emitters (see Emitter.h),
// destroying both when their time is up
//
#pragma once

class ParticleSystem
{
  public:
    void Update(float deltaTime);
};