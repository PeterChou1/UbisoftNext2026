//---------------------------------------------------------------------------------
// Utils.h
//---------------------------------------------------------------------------------
//
// Implements a couple of useful utility functions
//
#pragma once
#include "Mat2.h"
#include "Material.h"
#include "MeshInstance.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace Utils
{
    /// Closest point to `point` on the line segment ab
    Vec2 PointToLineSegment(Vec2 point, Vec2 a, Vec2 b);

    /// Rotation by angle (radians) used for 2d physics shapes
    Mat2 RotationMatrix(float angle);

    /// Rotates every point by matrix, then moves it by position, into out
    /// (no allocation once out is big enough)
    void TranslatePointsInto(const std::vector<Vec2>& points,
                             Mat2 matrix,
                             const Vec2& position,
                             std::vector<Vec2>& out);

    inline void TranslatePointsInto(const std::vector<Vec2>& points,
                                    float angle,
                                    const Vec2& position,
                                    std::vector<Vec2>& out)
    {
        TranslatePointsInto(points, RotationMatrix(angle), position, out);
    }

    inline float Clamp(float n, float lower, float upper)
    {
        return std::max(lower, std::min(n, upper));
    }

    /// Erase the index ranges [first, second) from vec in one pass. Adapted from
    /// https://stackoverflow.com/questions/33571609/having-trouble-with-vector-erase-and-remove-if
    template <typename Type>
    void EraseRanges(const std::vector<std::pair<int, int>>& rangesToErase, std::vector<Type>& vec)
    {
        std::vector<bool> erase(vec.size(), false);
        for (const std::pair<int, int>& range : rangesToErase)
            std::fill(erase.begin() + range.first, erase.begin() + range.second, true);

        // remove_if visits the elements in order
        auto next = erase.cbegin();
        vec.erase(std::remove_if(vec.begin(), vec.end(), [&next](const Type&) { return *next++; }),
                  vec.end());
    }

    /// Loads an .obj file into mesh (see Utils.cpp for what is supported).
    /// The materials of its .mtl file are appended to textureList and each
    /// vertex's TextureID indexes them (-1: default material)
    bool LoadInstance(std::string filename, MeshInstance& mesh, std::vector<Material>& textureList);
} // namespace Utils
