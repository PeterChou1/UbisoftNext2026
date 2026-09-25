#include "Vec4.h"

const Vec4& Vec4::operator*=(const Vec4& rhs)
{
    X *= rhs.X;
    Y *= rhs.Y;
    Z *= rhs.Z;
    W *= rhs.W;
    return *this;
}

Vec3 Vec4::ToVec3() const
{
    if (W != 0)
        return Vec3(X / W, Y / W, Z / W);
    return Vec3(X, Y, Z);
}
