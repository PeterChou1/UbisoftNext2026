//---------------------------------------------------------------------------------
// SAT.h
//---------------------------------------------------------------------------------
//
// Implements the Separating Axis Theorem to resolve polygon/polygon or polygon
// circle collision
//
#pragma once

#include "Vec2.h"

#include <vector>

/**
 * \brief Find the minimum translation vector (MTV) between a circle and a
 *        polygon (points and edge normals)
 * \return whether they intersect; if so collisionNormal and overlapAmount
 *         hold the MTV
 */
bool FindMTVCircle(const Vec2& center,
                   float radius,
                   const std::vector<Vec2>& poly,
                   const std::vector<Vec2>& normals,
                   Vec2& collisionNormal,
                   float& overlapAmount);

/**
 * \brief Find the minimum translation vector (MTV) between two polygons
 *        (points and edge normals)
 * \return whether they intersect; if so collisionNormal and overlapAmount
 *         hold the MTV
 */
bool FindMTVPolygon(const std::vector<Vec2>& poly1,
                    const std::vector<Vec2>& poly2,
                    const std::vector<Vec2>& normals1,
                    const std::vector<Vec2>& normals2,
                    Vec2& collisionNormal,
                    float& overlapAmount);
