#pragma once

#include <ClientData/GxTypes.h>

#include <cstdint>

namespace ClientData
{
    class CGxBatch
    {
      public:
        EGxPrim m_primType;
        uint32_t m_start;
        uint32_t m_count;
        uint16_t m_minIndex;
        uint16_t m_maxIndex;
    };
}
