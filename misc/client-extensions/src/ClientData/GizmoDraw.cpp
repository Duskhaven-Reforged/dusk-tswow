#include <ClientData/Draw.h>
#include <ClientData/GizmoDraw.h>
#include <ClientData/GxBatch.h>
#include <ClientData/GxDevice.h>
#include <ClientData/VectorMath.h>

#include <cmath>

namespace ClientData::GizmoDraw
{
    namespace
    {
        constexpr float kPi = 3.14159265358979323846f;

        CImVector Color(uint8_t b, uint8_t g, uint8_t r, uint8_t a)
        {
            CImVector color{};
            color.b = b;
            color.g = g;
            color.r = r;
            color.a = a;
            return color;
        }

        CImVector Red()
        {
            return Color(0x40, 0x40, 0xFF, 0xFF);
        }

        CImVector Green()
        {
            return Color(0x40, 0xFF, 0x40, 0xFF);
        }

        CImVector Blue()
        {
            return Color(0xFF, 0x40, 0x40, 0xFF);
        }

        CImVector Yellow()
        {
            return Color(0x40, 0xFF, 0xFF, 0xFF);
        }

        CImVector ColorFor(Axis axis, Axis selected, CImVector base)
        {
            return selected == axis ? Yellow() : base;
        }
    }

    void DrawArrowSolid(C3Vector const& origin, C3Vector const& dir, C3Vector const& side, float length,
                        float headLength, float headRadius, float shaftRadius, CImVector color)
    {
        CGxDevice* device = CGxDevice::Get();
        if (!device)
            return;

        C3Vector up = VectorMath::Cross(dir, side);
        float baseDist = length - headLength;
        C3Vector shaftBase = origin;
        C3Vector shaftTop = VectorMath::Add(origin, VectorMath::Scale(dir, baseDist));
        C3Vector tip = VectorMath::Add(origin, VectorMath::Scale(dir, length));

        constexpr int kSegments = 12;
        constexpr int kVertCount = 3 * kSegments + 3;
        constexpr int kTriCount = kSegments * 5;
        constexpr int kIndexCount = kTriCount * 3;

        C3Vector verts[kVertCount];
        for (int i = 0; i < kSegments; ++i)
        {
            float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(kSegments);
            float c = cosf(angle);
            float s = sinf(angle);
            C3Vector shaftRadial = VectorMath::Scale(VectorMath::Add(VectorMath::Scale(side, c), VectorMath::Scale(up, s)),
                                                     shaftRadius);
            C3Vector headRadial = VectorMath::Scale(VectorMath::Add(VectorMath::Scale(side, c), VectorMath::Scale(up, s)),
                                                    headRadius);

            verts[i] = VectorMath::Add(shaftBase, shaftRadial);
            verts[kSegments + i] = VectorMath::Add(shaftTop, shaftRadial);
            verts[2 * kSegments + i] = VectorMath::Add(shaftTop, headRadial);
        }

        verts[3 * kSegments] = tip;
        verts[3 * kSegments + 1] = shaftBase;
        verts[3 * kSegments + 2] = shaftTop;

        uint16_t indices[kIndexCount];
        int k = 0;
        for (int i = 0; i < kSegments; ++i)
        {
            int i0 = i;
            int i1 = (i + 1) % kSegments;
            indices[k++] = static_cast<uint16_t>(i0);
            indices[k++] = static_cast<uint16_t>(kSegments + i0);
            indices[k++] = static_cast<uint16_t>(kSegments + i1);
            indices[k++] = static_cast<uint16_t>(i0);
            indices[k++] = static_cast<uint16_t>(kSegments + i1);
            indices[k++] = static_cast<uint16_t>(i1);

            indices[k++] = static_cast<uint16_t>(3 * kSegments + 1);
            indices[k++] = static_cast<uint16_t>(i1);
            indices[k++] = static_cast<uint16_t>(i0);

            int head0 = 2 * kSegments + i;
            int head1 = 2 * kSegments + ((i + 1) % kSegments);
            indices[k++] = static_cast<uint16_t>(3 * kSegments + 2);
            indices[k++] = static_cast<uint16_t>(head0);
            indices[k++] = static_cast<uint16_t>(head1);

            indices[k++] = static_cast<uint16_t>(3 * kSegments);
            indices[k++] = static_cast<uint16_t>(head1);
            indices[k++] = static_cast<uint16_t>(head0);
        }

        GxPrimVertexPtr(kVertCount, verts, sizeof(C3Vector), nullptr, 0, &color, 0, nullptr, 0, nullptr, 0);
        GxPrimIndexPtr(kIndexCount, indices);

        CGxBatch batch{};
        batch.m_primType = GxPrim_Triangles;
        batch.m_minIndex = 0;
        batch.m_maxIndex = static_cast<uint16_t>(-1);
        batch.m_start = 0;
        batch.m_count = kIndexCount;
        device->Draw(&batch, 1);
    }

    void DrawTranslationGizmo(C3Vector const& origin, float scale, Axis selected)
    {
        float length = 1.0f * scale;
        float headLength = 0.25f * scale;
        float headRadius = 0.08f * scale;
        float shaftRadius = 0.02f * scale;

        DrawArrowSolid(origin, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, length, headLength, headRadius, shaftRadius,
                       ColorFor(Axis::X, selected, Red()));
        DrawArrowSolid(origin, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, length, headLength, headRadius, shaftRadius,
                       ColorFor(Axis::Y, selected, Green()));
        DrawArrowSolid(origin, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, length, headLength, headRadius, shaftRadius,
                       ColorFor(Axis::Z, selected, Blue()));
    }

    void DrawLine(C3Vector const& a, C3Vector const& b, CImVector color)
    {
        CGxDevice* device = CGxDevice::Get();
        if (!device)
            return;

        C3Vector verts[2] = {a, b};
        uint16_t indices[2] = {0, 1};
        GxPrimVertexPtr(2, verts, sizeof(C3Vector), nullptr, 0, &color, 0, nullptr, 0, nullptr, 0);
        GxPrimIndexPtr(2, indices);

        CGxBatch batch{};
        batch.m_primType = GxPrim_Lines;
        batch.m_minIndex = 0;
        batch.m_maxIndex = static_cast<uint16_t>(-1);
        batch.m_start = 0;
        batch.m_count = 2;
        device->Draw(&batch, 1);
    }

    void DrawSphere(C3Vector const& center, float radius, CImVector color)
    {
        CGxDevice* device = CGxDevice::Get();
        if (!device)
            return;

        constexpr int kStacks = 12;
        constexpr int kSlices = 16;
        constexpr int kRingVerts = (kStacks - 1) * kSlices;
        constexpr int kVertCount = kRingVerts + 2;
        constexpr int kSouthPole = kVertCount - 1;
        constexpr int kTriCount = kSlices * 2 + (kStacks - 2) * kSlices * 2;
        constexpr int kIndexCount = kTriCount * 3;

        C3Vector verts[kVertCount];
        verts[0] = {center.x, center.y, center.z + radius};
        verts[kSouthPole] = {center.x, center.y, center.z - radius};

        for (int stack = 1; stack < kStacks; ++stack)
        {
            float phi = kPi * static_cast<float>(stack) / static_cast<float>(kStacks);
            float z = cosf(phi);
            float r = sinf(phi);
            for (int i = 0; i < kSlices; ++i)
            {
                float theta = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(kSlices);
                int index = 1 + (stack - 1) * kSlices + i;
                verts[index] = {center.x + radius * r * cosf(theta), center.y + radius * r * sinf(theta),
                                center.z + radius * z};
            }
        }

        uint16_t indices[kIndexCount];
        int k = 0;
        for (int i = 0; i < kSlices; ++i)
        {
            indices[k++] = 0;
            indices[k++] = static_cast<uint16_t>(1 + i);
            indices[k++] = static_cast<uint16_t>(1 + ((i + 1) % kSlices));
        }

        for (int stack = 0; stack < kStacks - 2; ++stack)
        {
            int ringA = 1 + stack * kSlices;
            int ringB = ringA + kSlices;
            for (int i = 0; i < kSlices; ++i)
            {
                int a0 = ringA + i;
                int a1 = ringA + (i + 1) % kSlices;
                int b0 = ringB + i;
                int b1 = ringB + (i + 1) % kSlices;
                indices[k++] = static_cast<uint16_t>(a0);
                indices[k++] = static_cast<uint16_t>(b0);
                indices[k++] = static_cast<uint16_t>(b1);
                indices[k++] = static_cast<uint16_t>(a0);
                indices[k++] = static_cast<uint16_t>(b1);
                indices[k++] = static_cast<uint16_t>(a1);
            }
        }

        int lastRing = 1 + (kStacks - 2) * kSlices;
        for (int i = 0; i < kSlices; ++i)
        {
            indices[k++] = static_cast<uint16_t>(kSouthPole);
            indices[k++] = static_cast<uint16_t>(lastRing + ((i + 1) % kSlices));
            indices[k++] = static_cast<uint16_t>(lastRing + i);
        }

        GxPrimVertexPtr(kVertCount, verts, sizeof(C3Vector), nullptr, 0, &color, 0, nullptr, 0, nullptr, 0);
        GxPrimIndexPtr(kIndexCount, indices);

        CGxBatch batch{};
        batch.m_primType = GxPrim_Triangles;
        batch.m_minIndex = 0;
        batch.m_maxIndex = static_cast<uint16_t>(-1);
        batch.m_start = 0;
        batch.m_count = kIndexCount;
        device->Draw(&batch, 1);
    }

    void DrawWireSphere(C3Vector const& center, float radius, CImVector color)
    {
        constexpr int kSegments = 32;
        C3Vector verts[kSegments * 3];
        uint16_t indices[kSegments * 2 * 3];
        int v = 0;
        int k = 0;

        for (int axis = 0; axis < 3; ++axis)
        {
            int base = v;
            for (int i = 0; i < kSegments; ++i)
            {
                float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(kSegments);
                float c = cosf(angle) * radius;
                float s = sinf(angle) * radius;
                C3Vector p = center;
                if (axis == 0)
                {
                    p.y += c;
                    p.z += s;
                }
                else if (axis == 1)
                {
                    p.x += c;
                    p.z += s;
                }
                else
                {
                    p.x += c;
                    p.y += s;
                }
                verts[v++] = p;
            }
            for (int i = 0; i < kSegments; ++i)
            {
                indices[k++] = static_cast<uint16_t>(base + i);
                indices[k++] = static_cast<uint16_t>(base + (i + 1) % kSegments);
            }
        }

        CGxDevice* device = CGxDevice::Get();
        if (!device)
            return;

        GxPrimVertexPtr(v, verts, sizeof(C3Vector), nullptr, 0, &color, 0, nullptr, 0, nullptr, 0);
        GxPrimIndexPtr(k, indices);
        CGxBatch batch{};
        batch.m_primType = GxPrim_Lines;
        batch.m_minIndex = 0;
        batch.m_maxIndex = static_cast<uint16_t>(-1);
        batch.m_start = 0;
        batch.m_count = k;
        device->Draw(&batch, 1);
    }

    void DrawTorus(C3Vector const& origin, C3Vector const& axis, C3Vector const& side, float ringRadius,
                   float tubeRadius, CImVector color)
    {
        CGxDevice* device = CGxDevice::Get();
        if (!device)
            return;

        C3Vector up = VectorMath::Cross(axis, side);
        constexpr int kMajor = 32;
        constexpr int kMinor = 8;
        constexpr int kVertCount = kMajor * kMinor;
        constexpr int kIndexCount = kMajor * kMinor * 6;

        C3Vector verts[kVertCount];
        for (int i = 0; i < kMajor; ++i)
        {
            float u = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(kMajor);
            C3Vector radial = VectorMath::Add(VectorMath::Scale(side, cosf(u)), VectorMath::Scale(up, sinf(u)));
            C3Vector ringCenter = VectorMath::Add(origin, VectorMath::Scale(radial, ringRadius));

            for (int j = 0; j < kMinor; ++j)
            {
                float v = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(kMinor);
                verts[i * kMinor + j] = VectorMath::Add(
                    ringCenter,
                    VectorMath::Scale(VectorMath::Add(VectorMath::Scale(radial, cosf(v)), VectorMath::Scale(axis, sinf(v))),
                                      tubeRadius));
            }
        }

        uint16_t indices[kIndexCount];
        int k = 0;
        for (int i = 0; i < kMajor; ++i)
        {
            int nextI = (i + 1) % kMajor;
            for (int j = 0; j < kMinor; ++j)
            {
                int nextJ = (j + 1) % kMinor;
                int a = i * kMinor + j;
                int b = nextI * kMinor + j;
                int c = nextI * kMinor + nextJ;
                int d = i * kMinor + nextJ;
                indices[k++] = static_cast<uint16_t>(a);
                indices[k++] = static_cast<uint16_t>(b);
                indices[k++] = static_cast<uint16_t>(c);
                indices[k++] = static_cast<uint16_t>(a);
                indices[k++] = static_cast<uint16_t>(c);
                indices[k++] = static_cast<uint16_t>(d);
            }
        }

        GxPrimVertexPtr(kVertCount, verts, sizeof(C3Vector), nullptr, 0, &color, 0, nullptr, 0, nullptr, 0);
        GxPrimIndexPtr(kIndexCount, indices);
        CGxBatch batch{};
        batch.m_primType = GxPrim_Triangles;
        batch.m_minIndex = 0;
        batch.m_maxIndex = static_cast<uint16_t>(-1);
        batch.m_start = 0;
        batch.m_count = kIndexCount;
        device->Draw(&batch, 1);
    }

    void DrawRotationGizmo(C3Vector const& origin, float scale, Axis selected)
    {
        float ringRadius = 1.0f * scale;
        float tubeRadius = 0.02f * scale;
        DrawTorus(origin, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, ringRadius, tubeRadius,
                  ColorFor(Axis::X, selected, Red()));
        DrawTorus(origin, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, ringRadius, tubeRadius,
                  ColorFor(Axis::Y, selected, Green()));
        DrawTorus(origin, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, ringRadius, tubeRadius,
                  ColorFor(Axis::Z, selected, Blue()));
    }
}
