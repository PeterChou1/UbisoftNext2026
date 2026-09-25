//---------------------------------------------------------------------------------
// TypeID.h
//---------------------------------------------------------------------------------
//
// A unique ComponentTypeID for every component type used in the ECS System
//
#pragma once

#include "Entity.h"

inline ComponentTypeID GenerateComponentID()
{
    static ComponentTypeID counter = 0;
    return counter++;
}

template <typename T>
struct TypeID
{
    static inline const ComponentTypeID VALUE = GenerateComponentID();
};
