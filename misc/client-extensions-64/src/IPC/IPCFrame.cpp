#include "IPCFrame.h"
#include <iostream>

IPCFrame::IPCFrame() : pipeServer_(dispatcher_) {
    handlers_ = HandlerRegistry::CreateAll();
}

IPCFrame::~IPCFrame() {
    Stop();
}

bool IPCFrame::Start(const std::wstring& pipeName) {
    std::wcout << L"🚀 Starting IPC Server on pipe: " << pipeName << std::endl;

    for (auto& h : handlers_) {
        h->Register(dispatcher_);
    }

    return pipeServer_.Start(pipeName);
}

void IPCFrame::Stop() {
    for (auto& h : handlers_) {
        h->Unregister(dispatcher_);
    }

    pipeServer_.Stop();
}
