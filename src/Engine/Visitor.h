//---------------------------------------------------------------------------------
// Visitor.h
//---------------------------------------------------------------------------------
//
// A Visitor represents a System of the Entity Component System: it keeps
// track of every Entity that holds a set of components
//
#pragma once

#include "Entity.h"

#include <set>

struct Visitor
{
    // The components an Entity needs to be visited
    Signature Requirements;
    std::set<Entity> Entities;
    // Visited entities deleted (or that lost a component) since the last
    // ECSManager::FlushECS
    std::set<Entity> DeletedEntities;
};
