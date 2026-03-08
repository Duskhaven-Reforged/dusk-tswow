#pragma once

#include <functional>
#include <memory>
#include <vector>

class VirtualHandler;

// Global registry for IPC handlers.
class HandlerRegistry {
public:
    using Creator = std::function<std::unique_ptr<VirtualHandler>()>;

    static void RegisterCreator(Creator creator);
    static std::vector<std::unique_ptr<VirtualHandler>> CreateAll();
};

// Self-registration helper.
// Usage (in a .cpp):
//   #include "IPC/HandlerRegistry.h"
//   REGISTER_IPC_HANDLER(MyHandler)
#define REGISTER_IPC_HANDLER(HandlerType)                                      \
    namespace {                                                                \
    struct HandlerType##Registrar {                                            \
        HandlerType##Registrar() {                                             \
            HandlerRegistry::RegisterCreator([]() {                            \
                return std::make_unique<HandlerType>();                       \
            });                                                                \
        }                                                                      \
    };                                                                         \
    static HandlerType##Registrar s_##HandlerType##_registrar;                 \
    }
