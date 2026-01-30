#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "../handlers/Opcodes.h"
#include "PacketBuilder.h"

class OpcodeDispatcher {
public:
    using Handler = std::function<void(Opcode opcode, PacketReader reader)>;

    void Register(Opcode opcode, Handler h) {
        std::scoped_lock lk(mu_);
        handlers_[opcode] = std::move(h);
    }

    void Unregister(Opcode opcode) {
        std::scoped_lock lk(mu_);
        handlers_.erase(opcode);
    }

    bool Dispatch(Opcode opcode, PacketReader data) {
        Handler h;
        {
            std::scoped_lock lk(mu_);
            auto it = handlers_.find(opcode);
            if (it == handlers_.end()) return false;
            h = it->second;
        }
        h(opcode, data);
        return true;
    }

private:
    std::mutex mu_;
    std::unordered_map<Opcode, Handler, OpcodeHash> handlers_;
};
