#pragma once

#include <ClientData/GizmoPick.h>
#include <ClientData/MathTypes.h>

namespace ClientData::GizmoDraw
{
    void DrawArrowSolid(C3Vector const& origin, C3Vector const& dir, C3Vector const& side, float length,
                        float headLength, float headRadius, float shaftRadius, CImVector color);
    void DrawTranslationGizmo(C3Vector const& origin, float scale, Axis selected);
    void DrawLine(C3Vector const& a, C3Vector const& b, CImVector color);
    void DrawSphere(C3Vector const& center, float radius, CImVector color);
    void DrawWireSphere(C3Vector const& center, float radius, CImVector color);
    void DrawTorus(C3Vector const& origin, C3Vector const& axis, C3Vector const& side, float ringRadius,
                   float tubeRadius, CImVector color);
    void DrawRotationGizmo(C3Vector const& origin, float scale, Axis selected);
}
