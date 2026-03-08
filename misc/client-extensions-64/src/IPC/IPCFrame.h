#pragma once
#include "PipeServer.h"
#include "OpcodeDispatcher.h"
#include "../handlers/HandlerRegistry.h"
#include "../handlers/VirtualHandler.h"
#include <string>
#include <memory>
#include <vector>

class IPCFrame {
public:
    IPCFrame();
    ~IPCFrame();

    bool Start(const std::wstring& pipeName);
    void Stop();

private:
    OpcodeDispatcher dispatcher_;
    PipeServer pipeServer_;

    // IPC handlers (self-registered via HandlerRegistry)
    std::vector<std::unique_ptr<VirtualHandler>> handlers_;
};
