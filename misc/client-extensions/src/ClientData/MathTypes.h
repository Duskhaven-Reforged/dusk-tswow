#pragma once
#include <cstdint>

namespace ClientData
{
    struct C2Vector
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct C3Vector
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct CRange
    {
        float l;
        float h;
    };

    struct CiRange
    {
        int32_t l;
        int32_t h;
    };

    struct C2iVector
    {
        int32_t x;
        int32_t y;
    };

    struct C4Vector
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 0.0f;
    };

    struct CImVector
    {
        union
        {
            struct
            {
                uint8_t b;
                uint8_t g;
                uint8_t r;
                uint8_t a;
            };

            uint32_t value;
        };
    };

    struct CRect
    {
        float minY = 0.0f;
        float minX = 0.0f;
        float maxY = 0.0f;
        float maxX = 0.0f;
    };

    struct CiRect
    {
        int32_t minY;
        int32_t minX;
        int32_t maxY;
        int32_t maxX;
    };

    struct CBoundingBox
    {
        CRange x;
        CRange y;
        CRange z;
    };

    struct CAaBox
    {
        C3Vector b;
        C3Vector t;
    };

    struct C44Matrix
    {
        float a0 = 1.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float a3 = 0.0f;
        float b0 = 0.0f;
        float b1 = 1.0f;
        float b2 = 0.0f;
        float b3 = 0.0f;
        float c0 = 0.0f;
        float c1 = 0.0f;
        float c2 = 1.0f;
        float c3 = 0.0f;
        float d0 = 0.0f;
        float d1 = 0.0f;
        float d2 = 0.0f;
        float d3 = 1.0f;
    };
}
