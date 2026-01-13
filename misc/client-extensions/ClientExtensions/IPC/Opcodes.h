#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>
class Opcode {
public:
    enum Value : uint32_t {
        DEFAULT_VALUE = 0x0,
        CMSG_LOBBY_JOIN = 0x1001,
        CMSG_LOBBY_LEAVE = 0x1002,
        CMSG_LOBBY_UPDATE = 0x1003
    };
    constexpr Opcode() : value_(DEFAULT_VALUE) {}
    constexpr Opcode(Value v) : value_(v) {}
    constexpr explicit Opcode(uint32_t v) : value_(static_cast<Value>(v)) {}
    constexpr uint32_t raw() const { return value_; }

    // Comparisons
    constexpr bool operator==(Opcode other) const { return value_ == other.value_; }
    constexpr bool operator!=(Opcode other) const { return value_ != other.value_; }
    constexpr bool operator==(uint32_t other) const { return value_ == other; }
    constexpr bool operator!=(uint32_t other) const { return value_ != other; }

private:
    Value value_;
};

struct OpcodeHash {
    size_t operator()(const Opcode& op) const noexcept {
        return std::hash<uint32_t>{}(op.raw());
    }
};