#pragma once

#include <ClientData/MathTypes.h>
#include <ClientData/SharedDefines.h>

namespace ClientData
{
    class CGCamera
    {
      public:
        void SetupWorldProjection(CRect* rect)
        {
            reinterpret_cast<void(__thiscall*)(CGCamera*, CRect*)>(0x5FE880)(this, rect);
        }

        virtual float Fov();
        virtual C3Vector& Forward(C3Vector& vec);
        virtual C3Vector& Right(C3Vector& vec);
        virtual C3Vector& Up(C3Vector& vec);

        char _pad0[0x4];
        C3Vector m_position;
        float m_facing[3 * 3];
        float m_nearZ;
        float m_farZ;
        float m_fov;
        float m_aspect;
    };
}
