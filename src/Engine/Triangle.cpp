#include "Triangle.h"

#include <algorithm>

namespace
{
    /// Which tile corner (0..3) to test to trivially reject / accept a tile
    /// against the edge with normal (-b, c)
    void TileCorners(int b, int c, int& reject, int& accept)
    {
        const bool normalX = -b > 0;
        const bool normalY = c > 0;
        if (normalX)
        {
            reject = normalY ? 3 : 1;
            accept = normalY ? 0 : 2;
        }
        else
        {
            reject = normalY ? 2 : 0;
            accept = normalY ? 1 : 3;
        }
    }
} // namespace

void Triangle::PerspectiveDivision()
{
    verts[0].PerspectiveDivision();
    verts[1].PerspectiveDivision();
    verts[2].PerspectiveDivision();
}

bool Triangle::Setup(int id, int index)
{
    // Edge function coefficients, bounding box and the tile corners used to
    // trivially reject / accept tiles, see
    // https://www.cs.cmu.edu/afs/cs/academic/class/15869-f11/www/readings/abrash09_lrbrast.pdf
    BinIndex = index;
    BinID = id;

    const float X0 = verts[0].Projection.X, Y0 = verts[0].Projection.Y;
    const float X1 = verts[1].Projection.X, Y1 = verts[1].Projection.Y;
    const float X2 = verts[2].Projection.X, Y2 = verts[2].Projection.Y;

    B0 = static_cast<int>(Y1 - Y0);
    C0 = static_cast<int>(X1 - X0);
    B1 = static_cast<int>(Y2 - Y1);
    C1 = static_cast<int>(X2 - X1);
    B2 = static_cast<int>(Y0 - Y2);
    C2 = static_cast<int>(X0 - X2);

    const int det = C2 * -B1 + C1 * B2;

    if (det < 0)
    {
        B0 *= -1;
        C0 *= -1;
        B1 *= -1;
        C1 *= -1;
        B2 *= -1;
        C2 *= -1;
    }

    // Degenerate (no area)
    if (det == 0)
        return false;

    invDet = 1.0f / static_cast<float>(det);

    // set up bounding box
    maxX = static_cast<int>(std::max<float>(std::max<float>(X0, X1), X2));
    maxY = static_cast<int>(std::max<float>(std::max<float>(Y0, Y1), Y2));
    minX = static_cast<int>(std::min<float>(std::min<float>(X0, X1), X2));
    minY = static_cast<int>(std::min<float>(std::min<float>(Y0, Y1), Y2));

    TileCorners(B0, C0, rejectIndex0, acceptIndex0);
    TileCorners(B1, C1, rejectIndex1, acceptIndex1);
    TileCorners(B2, C2, rejectIndex2, acceptIndex2);

    return det > 0;
}
