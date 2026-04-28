#pragma once

#include <ClientData/SharedDefines.h>

namespace WMOLogging
{
    void Apply();
    void RecordMapAssetOpen(const char* path, HANDLE handle);
    void RecordMapAssetMissing(const char* path);
    void RecordWmoRootParseBegin(void* objectPtr, const char* candidatePath, uintptr_t bufferPtr, uint32_t bufferSize);
    void RecordMissingBspTraversal(void* traversalContext, int nodeIndex, int* queryBounds, int* clipBounds);
}
