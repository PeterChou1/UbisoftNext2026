//---------------------------------------------------------------------------------
// SIMD.h
//---------------------------------------------------------------------------------
//
// Wrapper class for all SIMD (AVX2) operations because intel intrinsics look
// very ugly If your processor does not support AVX2 default back to scalar variant
//
#pragma once

#include "Vec2.h"
#include "Vec3.h"

// -- determine if processor has avx2 --
#if defined(__AVX2__) && defined(__x86_64__)
#include <immintrin.h>
#define SIMD_AVX2_COMPILETIME
#endif

#ifdef SIMD_AVX2_COMPILETIME
/*
 * /brief Float based on avx2 please see the intel intrinsics
 *        most methods here are self-explanatory
 *        for more information see here:
 *        https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#avxnewtechs=AVX2
 */

class SIMDFloat
{
  public:
    SIMDFloat()
        : AVX256(_mm256_setzero_ps())
    {
    }

    SIMDFloat(__m256 avx)
        : AVX256(avx)
    {
    }

    SIMDFloat(const float& a)
        : AVX256(_mm256_broadcast_ss(&a))
    {
    }

    SIMDFloat(float* a)
        : AVX256(_mm256_loadu_ps(a))
    {
    }

    SIMDFloat operator*(const SIMDFloat& rhs) const { return _mm256_mul_ps(AVX256, rhs.AVX256); }

    SIMDFloat operator*(const float rhs) const { return *this * SIMDFloat(rhs); }

    SIMDFloat operator+(const SIMDFloat& rhs) const { return _mm256_add_ps(AVX256, rhs.AVX256); }

    SIMDFloat operator-(const SIMDFloat& rhs) const { return _mm256_sub_ps(AVX256, rhs.AVX256); }

    float& operator[](const size_t i) { return V[i]; }

    bool GetBit(const size_t i) const { return (_mm256_movemask_ps(AVX256) >> i) & 1; }

    SIMDFloat operator>=(const SIMDFloat& rhs) const
    {
        return _mm256_cmp_ps(AVX256, rhs.AVX256, _CMP_NLT_US);
    }

    SIMDFloat operator>=(const float rhs) const { return *this >= SIMDFloat(rhs); }

    SIMDFloat operator>(const SIMDFloat& rhs) const
    {
        return _mm256_cmp_ps(AVX256, rhs.AVX256, _CMP_NLE_US);
    }

    SIMDFloat operator>(const float rhs) const { return *this > SIMDFloat(rhs); }

    SIMDFloat operator<(const SIMDFloat& rhs) const
    {
        return _mm256_cmp_ps(AVX256, rhs.AVX256, _CMP_LT_OS);
    }

    SIMDFloat operator<(const float rhs) const { return *this < SIMDFloat(rhs); }

    SIMDFloat operator<=(const SIMDFloat& rhs) const
    {
        return _mm256_cmp_ps(AVX256, rhs.AVX256, _CMP_LE_OS);
    }

    SIMDFloat operator<=(const float rhs) const { return *this <= SIMDFloat(rhs); }

    SIMDFloat operator&(const SIMDFloat& rhs) const { return _mm256_and_ps(AVX256, rhs.AVX256); }

    SIMDFloat operator|(const SIMDFloat& rhs) const { return _mm256_or_ps(AVX256, rhs.AVX256); }

    SIMDFloat operator==(const SIMDFloat& rhs) const
    {
        return _mm256_cmp_ps(AVX256, rhs.AVX256, _CMP_EQ_US);
    }

    SIMDFloat operator/(const SIMDFloat& rhs) const
    {
        return SIMDFloat(AVX256) * SIMDFloat(_mm256_rcp_ps(rhs.AVX256));
    }

    SIMDFloat operator/(const float rhs) const { return *this * (1.0f / rhs); }

    SIMDFloat floor() { return _mm256_floor_ps(AVX256); }

    SIMDFloat sqrt() const { return _mm256_sqrt_ps(AVX256); }

    SIMDFloat pow(const SIMDFloat& rhs) const { return _mm256_pow_ps(AVX256, rhs.AVX256); }

    union {
        __m256 AVX256;
        float V[8];
    };
};
#else
#include <cmath>
// SIMDFloat polyfill
class SIMDFloat
{
  public:
    SIMDFloat()
    {
        for (int i = 0; i < 8; ++i)
        {
            V[i] = 0.0f;
        }
    }

    SIMDFloat(float val)
    {
        for (int i = 0; i < 8; ++i)
        {
            V[i] = val;
        }
    }

    SIMDFloat(const SIMDFloat& rhs)
    {
        for (int i = 0; i < 8; ++i)
        {
            V[i] = rhs.V[i];
        }
    }

    SIMDFloat(float a[8])
    {
        for (int i = 0; i < 8; ++i)
        {
            V[i] = a[i];
        }
    }

    SIMDFloat& operator=(const SIMDFloat& rhs)
    {
        for (int i = 0; i < 8; ++i)
        {
            V[i] = rhs.V[i];
        }
        return *this;
    }

    SIMDFloat operator*(const SIMDFloat& rhs) const
    {
        SIMDFloat result;
        for (int i = 0; i < 8; ++i)
        {
            result.V[i] = V[i] * rhs.V[i];
        }
        return result;
    }

    SIMDFloat operator+(const SIMDFloat& rhs) const
    {
        SIMDFloat result;
        for (int i = 0; i < 8; ++i)
        {
            result.V[i] = V[i] + rhs.V[i];
        }
        return result;
    }

    SIMDFloat operator-(const SIMDFloat& rhs) const
    {
        SIMDFloat result;
        for (int i = 0; i < 8; ++i)
        {
            result.V[i] = V[i] - rhs.V[i];
        }
        return result;
    }

    SIMDFloat operator/(const SIMDFloat& rhs) const
    {
        SIMDFloat result;
        for (int i = 0; i < 8; ++i)
        {
            result.V[i] = V[i] / rhs.V[i];
        }
        return result;
    }

    SIMDFloat operator>=(const SIMDFloat& rhs) const
    {
        SIMDFloat result;
        for (int i = 0; i < 8; ++i)
            result.V[i] = V[i] >= rhs.V[i] ? 1.0f : 0.0f;
        return result;
    }

    SIMDFloat operator>(const SIMDFloat& rhs) const
    {
        SIMDFloat result;
        for (int i = 0; i < 8; ++i)
            result.V[i] = V[i] > rhs.V[i] ? 1.0f : 0.0f;
        return result;
    }

    SIMDFloat operator<(const SIMDFloat& rhs) const
    {
        SIMDFloat result;
        for (int i = 0; i < 8; ++i)
            result.V[i] = V[i] < rhs.V[i] ? 1.0f : 0.0f;
        return result;
    }

    SIMDFloat operator<=(const SIMDFloat& rhs) const
    {
        SIMDFloat result;
        for (int i = 0; i < 8; ++i)
            result.V[i] = V[i] <= rhs.V[i] ? 1.0f : 0.0f;
        return result;
    }

    SIMDFloat operator==(const SIMDFloat& rhs) const
    {
        SIMDFloat result;
        for (int i = 0; i < 8; ++i)
            result.V[i] = V[i] == rhs.V[i] ? 1.0f : 0.0f;
        return result;
    }

    // Bitwise operators
    SIMDFloat operator&(const SIMDFloat& rhs) const
    {
        SIMDFloat result;
        for (int i = 0; i < 8; ++i)
            result.V[i] = static_cast<float>(static_cast<int>(V[i]) && static_cast<int>(rhs.V[i]));
        return result;
    }

    SIMDFloat operator|(const SIMDFloat& rhs) const
    {
        SIMDFloat result;
        for (int i = 0; i < 8; ++i)
            result.V[i] = static_cast<float>(static_cast<int>(V[i]) || static_cast<int>(rhs.V[i]));
        return result;
    }

    // Mathematical functions
    SIMDFloat floor() const
    {
        SIMDFloat result;
        for (int i = 0; i < 8; ++i)
            result.V[i] = std::floor(V[i]);
        return result;
    }

    SIMDFloat sqrt() const
    {
        SIMDFloat result;
        for (int i = 0; i < 8; ++i)
            result.V[i] = std::sqrt(V[i]);
        return result;
    }

    SIMDFloat pow(const SIMDFloat& rhs) const
    {
        SIMDFloat result;
        for (int i = 0; i < 8; ++i)
            result.V[i] = std::pow(V[i], rhs.V[i]);
        return result;
    }

    bool GetBit(const size_t i) const
    {
        return V[i] == 1.0f; // Check the sign bit (most significant bit)
    }

    // Accessor
    float operator[](const size_t i) const { return V[i]; }

    float& operator[](const size_t i) { return V[i]; }

    // Other methods as needed
    float V[8];
};

#endif

class SIMDVec2
{
  public:
    SIMDVec2() = default;

    SIMDVec2(float x, float y)
        : X(SIMDFloat(x))
        , Y(SIMDFloat(y))
    {
    }

    SIMDVec2(SIMDFloat x, SIMDFloat y)
        : X(x)
        , Y(y)
    {
    }

    SIMDVec2(Vec2 v)
        : X(v.X)
        , Y(v.Y)
    {
    }

    SIMDVec2 operator+(const SIMDVec2& rhs) const { return {X + rhs.X, Y + rhs.Y}; }

    SIMDVec2 operator-(const SIMDVec2& rhs) const { return {X - rhs.X, Y - rhs.Y}; }

    SIMDVec2 operator*(const SIMDFloat& rhs) const { return {X * rhs, Y * rhs}; }

    SIMDVec2 operator/(const SIMDFloat& rhs) const { return {X / rhs, Y / rhs}; }

    SIMDFloat length() const
    {
        SIMDFloat squared = X * X + Y * Y;
        return squared.sqrt();
    }

    SIMDFloat Dot(const SIMDVec2& rhs) { return X * rhs.X + Y * rhs.Y; }

    SIMDVec2 Normalize() { return *this / length(); }

    SIMDFloat X{}, Y{};
};

class SIMDVec3
{
  public:
    SIMDVec3() = default;

    SIMDVec3(float x, float y, float z)
        : X(x)
        , Y(y)
        , Z(z)
    {
    }

    SIMDVec3(SIMDFloat x, SIMDFloat y, SIMDFloat z)
        : X(x)
        , Y(y)
        , Z(z)
    {
    }

    SIMDVec3(Vec3 v)
        : X(v.X)
        , Y(v.Y)
        , Z(v.Z)
    {
    }

    SIMDVec3 operator+(const SIMDVec3& rhs) const { return {X + rhs.X, Y + rhs.Y, Z + rhs.Z}; }

    SIMDVec3 operator*(const SIMDVec3& rhs) const { return {X * rhs.X, Y * rhs.Y, Z * rhs.Z}; }

    SIMDVec3 operator-(const SIMDVec3& rhs) const { return {X - rhs.X, Y - rhs.Y, Z - rhs.Z}; }

    SIMDVec3 operator*(const SIMDFloat& rhs) const { return {X * rhs, Y * rhs, Z * rhs}; }

    SIMDVec3 operator/(const SIMDFloat& rhs) const { return {X / rhs, Y / rhs, Z / rhs}; }

    SIMDFloat length() const
    {
        SIMDFloat squared = X * X + Y * Y + Z * Z;
        return squared.sqrt();
    }

    SIMDFloat Dot(const SIMDVec3& rhs) { return X * rhs.X + Y * rhs.Y + Z * rhs.Z; }

    SIMDVec3 Normalize() { return *this / length(); }

    SIMDFloat X{}, Y{}, Z{};
};

namespace SIMD
{

    static SIMDFloat ONE = 1.0;
    static SIMDFloat ZERO = 0.0;
    static SIMDFloat EPSILON = 0.05f;

#ifdef SIMD_AVX2_COMPILETIME
    /**
     * \brief Return true if all 8 floats are true
     */
    inline bool All(const SIMDFloat& rhs)
    {
        return _mm256_movemask_ps(rhs.AVX256) == 255;
    }

    /**
     * \brief Return true if any 8 floats are true
     */
    inline bool Any(const SIMDFloat& rhs)
    {
        return _mm256_movemask_ps(rhs.AVX256) != 0x0;
    };

    /**
     * \brief Selects A or B based on Mask
     *        if mask[i] == 1 then B is selected
     *        if mask[i] == 0 then A is selected
     */
    inline SIMDFloat Select(const SIMDFloat& mask, SIMDFloat& a, SIMDFloat& b)
    {
        return _mm256_blendv_ps(a.AVX256, b.AVX256, mask.AVX256);
    }

    /**
     * \brief Take Absolute Value of a
     */
    inline SIMDFloat Abs(const SIMDFloat& a)
    {
        const __m256 signMask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7FFFFFFF));
        __m256 result = _mm256_and_ps(a.AVX256, signMask);
        return result;
    }

    /**
     * \brief Get max from both A and B
     * \return Combined SIMDFloat with larger members from one of two floats
     */
    inline SIMDFloat Max(SIMDFloat a, SIMDFloat b)
    {
        return Select(a > b, b, a);
    }

    /**
     * \brief Get max from both A and B
     * \return Combined SIMDFloat with larger members from one of two floats
     */
    inline SIMDFloat Min(SIMDFloat a, SIMDFloat b)
    {
        return Select(a > b, a, b);
    }

#else
    inline bool All(const SIMDFloat& rhs)
    {
        for (int i = 0; i < 8; ++i)
        {
            if (rhs[i] == 0.0f)
            {
                return false;
            }
        }
        return true;
    }

    inline bool Any(const SIMDFloat& rhs)
    {
        for (int i = 0; i < 8; ++i)
        {
            if (rhs[i] != 0.0f)
            {
                return true;
            }
        }
        return false;
    }
    inline SIMDFloat Abs(const SIMDFloat& a)
    {
        SIMDFloat result;
        for (int i = 0; i < 8; ++i)
        {
            result[i] = std::abs(a[i]);
        }
        return result;
    }
    inline SIMDFloat Select(const SIMDFloat& mask, const SIMDFloat& a, const SIMDFloat& b)
    {
        SIMDFloat result;
        for (int i = 0; i < 8; ++i)
        {
            result[i] = (mask[i] != 0.0f) ? b[i] : a[i];
        }
        return result;
    }

    inline SIMDFloat Max(const SIMDFloat& a, const SIMDFloat& b)
    {
        SIMDFloat result;
        for (int i = 0; i < 8; ++i)
        {
            result[i] = (a[i] > b[i]) ? a[i] : b[i];
        }
        return result;
    }

    inline SIMDFloat Min(const SIMDFloat& a, const SIMDFloat& b)
    {
        SIMDFloat result;
        for (int i = 0; i < 8; ++i)
        {
            result[i] = (a[i] < b[i]) ? a[i] : b[i];
        }
        return result;
    }

#endif
    /**
     * \brief Clamp c between a and b
     *        a < b
     */
    inline SIMDFloat Clamp(SIMDFloat a, SIMDFloat b, SIMDFloat c)
    {
        return Min(b, Max(a, c));
    }
}; // namespace SIMD
 