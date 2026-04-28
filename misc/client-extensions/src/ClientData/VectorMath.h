#pragma once

#include <ClientData/MathTypes.h>
#include <SharedDefines.h>

#include <cmath>

namespace ClientData::VectorMath
{
    static constexpr float Pi = 3.1415927f;
    static constexpr float TwoPi = 6.2831855f;
    static constexpr float OneOverTwoPi = 1.0f / TwoPi;

    inline bool Fequalz(float a, float b, float tolerance)
    {
        return tolerance > fabsf(a - b);
    }

    inline bool Fequal(float a, float b)
    {
        return Fequalz(a, b, 0.00000023841858f);
    }

    inline int32_t Fint(float value)
    {
        return static_cast<int32_t>(value);
    }

    inline int32_t FintNearest(float value)
    {
        return value <= 0.0f ? static_cast<int32_t>(value - 0.5f) : static_cast<int32_t>(value + 0.5f);
    }

    inline uint32_t Fuint(float value)
    {
        return static_cast<uint32_t>(value);
    }

    inline uint32_t FuintNearest(float value)
    {
        return static_cast<uint32_t>(value + 0.5f);
    }

    inline uint32_t FuintPositiveInfinity(float value)
    {
        return static_cast<uint32_t>(value + 0.99994999f);
    }

    inline C2Vector C2FromAxisAngle(float axisAngle, float magnitude)
    {
        return {magnitude * cosf(axisAngle), magnitude * sinf(axisAngle)};
    }

    inline C2Vector Min(C2Vector const& left, C2Vector const& right)
    {
        return {fminf(left.x, right.x), fminf(left.y, right.y)};
    }

    inline C2Vector Max(C2Vector const& left, C2Vector const& right)
    {
        return {fmaxf(left.x, right.x), fmaxf(left.y, right.y)};
    }

    inline C2Vector Lerp(C2Vector const& amount, C2Vector const& low, C2Vector const& high)
    {
        return {low.x + (high.x - low.x) * amount.x, low.y + (high.y - low.y) * amount.y};
    }

    inline float Dot(C2Vector const& left, C2Vector const& right)
    {
        return left.x * right.x + left.y * right.y;
    }

    inline float Cross(C2Vector const& left, C2Vector const& right)
    {
        return left.x * right.y - right.x * left.y;
    }

    inline float LengthSquared(C2Vector const& value)
    {
        return Dot(value, value);
    }

    inline float Length(C2Vector const& value)
    {
        return sqrtf(LengthSquared(value));
    }

    inline C2Vector Normalize(C2Vector value)
    {
        float length = Length(value);
        if (length <= 0.0f)
            return {};

        return {value.x / length, value.y / length};
    }

    inline float AxisAngle(C2Vector const& value, float magnitude)
    {
        if (Fequal(magnitude, 0.0f))
            return 0.0f;

        float angle = acosf(value.x / magnitude);
        return value.y >= 0.0f ? angle : TwoPi - angle;
    }

    inline float AxisAngle(C2Vector const& value)
    {
        return AxisAngle(value, Length(value));
    }

    inline C3Vector Add(C3Vector const& left, C3Vector const& right)
    {
        return {left.x + right.x, left.y + right.y, left.z + right.z};
    }

    inline C3Vector Subtract(C3Vector const& left, C3Vector const& right)
    {
        return {left.x - right.x, left.y - right.y, left.z - right.z};
    }

    inline C3Vector Scale(C3Vector const& value, float scalar)
    {
        return {value.x * scalar, value.y * scalar, value.z * scalar};
    }

    inline float Dot(C3Vector const& left, C3Vector const& right)
    {
        return left.x * right.x + left.y * right.y + left.z * right.z;
    }

    inline C3Vector Cross(C3Vector const& left, C3Vector const& right)
    {
        return {
            left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x,
        };
    }

    inline float Length(C3Vector const& value)
    {
        return sqrtf(Dot(value, value));
    }

    inline C3Vector Min(C3Vector const& left, C3Vector const& right)
    {
        return {fminf(left.x, right.x), fminf(left.y, right.y), fminf(left.z, right.z)};
    }

    inline C3Vector Max(C3Vector const& left, C3Vector const& right)
    {
        return {fmaxf(left.x, right.x), fmaxf(left.y, right.y), fmaxf(left.z, right.z)};
    }

    inline C3Vector Lerp(C3Vector const& amount, C3Vector const& low, C3Vector const& high)
    {
        return {
            low.x + (high.x - low.x) * amount.x,
            low.y + (high.y - low.y) * amount.y,
            low.z + (high.z - low.z) * amount.z,
        };
    }

    inline C3Vector Normalize(C3Vector value)
    {
        float length = Length(value);
        if (length <= 0.0f)
            return {};

        return Scale(value, 1.0f / length);
    }

    inline C3Vector ProjectionOnPlane(C3Vector const& value, C3Vector const& normal)
    {
        return Subtract(value, Scale(normal, Dot(value, normal)));
    }

    inline C3Vector NearestOnPlane(C3Vector const& point, C3Vector const& onPlane, C3Vector const& normal)
    {
        return Add(onPlane, ProjectionOnPlane(Subtract(point, onPlane), normal));
    }

    inline uint32_t MajorAxis(C3Vector const& value)
    {
        float x = fabsf(value.x);
        float y = fabsf(value.y);
        float z = fabsf(value.z);

        if (x <= y)
        {
            if (y > z)
                return 1;
        }
        else if (x > z)
            return 0;

        return 2;
    }

    inline uint32_t MinorAxis(C3Vector const& value)
    {
        float x = fabsf(value.x);
        float y = fabsf(value.y);
        float z = fabsf(value.z);

        if (x >= y)
        {
            if (z > y)
                return 1;
        }
        else if (x >= z)
            return 2;
        else
            return 0;

        return 2;
    }

    inline uint32_t MakeARGB(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
    {
        return uint32_t(a) << 24 | uint32_t(r) << 16 | uint32_t(g) << 8 | uint32_t(b);
    }

    inline CImVector MakeColor(float a, float r, float g, float b)
    {
        CImVector color{};
        color.value = MakeARGB(FuintNearest(a * 255.0f), FuintNearest(r * 255.0f), FuintNearest(g * 255.0f),
                               FuintNearest(b * 255.0f));
        return color;
    }

    inline CRect Intersection(CRect const& left, CRect const& right)
    {
        CRect result{};
        result.maxX = right.maxX <= left.maxX ? right.maxX : left.maxX;
        result.maxY = right.maxY <= left.maxY ? right.maxY : left.maxY;
        result.minX = right.minX >= left.minX ? right.minX : left.minX;
        result.minY = right.minY >= left.minY ? right.minY : left.minY;
        return result;
    }

    inline bool IsPointInside(CRect const& rect, C2Vector const& point)
    {
        return rect.minX <= point.x && rect.maxX >= point.x && rect.minY <= point.y && rect.maxY >= point.y;
    }

    inline bool IsOutsideUnitRect(CRect const& rect)
    {
        return rect.maxY < 0.0f || rect.minY > 1.0f || rect.maxX < 0.0f || rect.minX > 1.0f;
    }

    inline CAaBox BoundingBox(C3Vector const* vectors, uint32_t vectorsCount)
    {
        CAaBox box{};
        if (!vectors || vectorsCount == 0)
            return box;

        box.b = vectors[0];
        box.t = vectors[0];
        for (uint32_t i = 1; i < vectorsCount; ++i)
        {
            box.b = Min(box.b, vectors[i]);
            box.t = Max(box.t, vectors[i]);
        }

        return box;
    }
}
