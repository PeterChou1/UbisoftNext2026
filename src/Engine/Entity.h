//---------------------------------------------------------------------------------
// Entity.h
//---------------------------------------------------------------------------------
//
// Constants and types of the ECS system
//

#pragma once

#include <bitset>
#include <cstdint>

// Id of an Entity: an index into the arrays of the EntityManager and the
// ComponentBuffers
using Entity = std::uint32_t;
// No Entity can have the null entity ID
constexpr Entity NULL_ENTITY = 0;
// Number of Entity ids (including NULL_ENTITY): the size of the arrays of
// the EntityManager and the ComponentBuffers
constexpr Entity MAX_ENTITIES = 5000;
// Used to identify components
using ComponentTypeID = std::size_t;
// Max number of component types
constexpr ComponentTypeID MAX_COMPONENTS = 32;
// The component types an Entity owns (one bit per ComponentTypeID)
using Signature = std::bitset<MAX_COMPONENTS>;