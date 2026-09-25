//---------------------------------------------------------------------------------
// Resource.h
//---------------------------------------------------------------------------------
//
// Resources are global objects registered once in the ECS system (at game
// startup). ResetResource is called every time the ECS is Reset
//
#pragma once

class Resource
{
  public:
    virtual ~Resource() = default;
    virtual void ResetResource() = 0;
};
