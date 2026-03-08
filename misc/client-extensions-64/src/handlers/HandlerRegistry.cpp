#include "HandlerRegistry.h"

#include "VirtualHandler.h"

namespace {
    std::vector<HandlerRegistry::Creator>& Creators() {
        static std::vector<HandlerRegistry::Creator> creators;
        return creators;
    }
}

void HandlerRegistry::RegisterCreator(Creator creator) {
    Creators().push_back(std::move(creator));
}

std::vector<std::unique_ptr<VirtualHandler>> HandlerRegistry::CreateAll() {
    std::vector<std::unique_ptr<VirtualHandler>> handlers;
    handlers.reserve(Creators().size());

    for (auto& c : Creators()) {
        handlers.push_back(c());
    }

    return handlers;
}
