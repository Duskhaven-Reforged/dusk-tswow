#pragma once
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>
#include "../handlers/Opcodes.h"

#pragma pack(push, 1)
struct MsgHeader
{
    Opcode opcode;
    uint32_t length;
};
#pragma pack(pop)
// ---------------------------
// Endianness helpers (little-endian)
// ---------------------------

static inline void append_u32_le(std::vector<uint8_t> &out, uint32_t v)
{
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

static inline void append_u64_le(std::vector<uint8_t>& out, uint64_t v)
{
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 32) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 40) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 48) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 56) & 0xFF));
}

static inline void append_bool(std::vector<uint8_t>& out, bool v)
{
    // IPC rule: 0 = false, 1 = true
    out.push_back(v ? 1 : 0);
}

static inline void append_f32_le(std::vector<uint8_t>& out, float v)
{
    uint32_t raw;
    static_assert(sizeof(float) == sizeof(uint32_t), "float must be 32-bit");
    std::memcpy(&raw, &v, sizeof(uint32_t));
    append_u32_le(out, raw);
}


static inline uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint64_t read_u64_le(const uint8_t *p)
{
    return (uint64_t)p[0] | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24) | ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40) | ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
}

static inline bool read_bool(const uint8_t *p)
{
    // IPC rule: 0 = false, anything else = true
    return p[0] != 0;
}

static inline float read_f32_le(const uint8_t *p)
{
    uint32_t raw = read_u32_le(p);
    float value;
    std::memcpy(&value, &raw, sizeof(float));
    return value;
}

// ---------------------------
// Packet Builder
// ---------------------------

class PacketBuilder
{
public:
    static PacketBuilder CreatePacket(Opcode opcode)
    {
        PacketBuilder pb;
        pb.header_.opcode = opcode;
        pb.header_.length = 0;
        return pb;
    }

    PacketBuilder &writeUInt32(uint32_t v)
    {
        append_u32_le(buffer_, v);
        return *this;
    }
    
    PacketBuilder& writeUInt64(uint64_t v)
    {
        append_u64_le(buffer_, v);
        return *this;
    }

    PacketBuilder& writeBool(bool v)
    {
        append_bool(buffer_, v);
        return *this;
    }

    PacketBuilder& writeFloat(float v)
    {
        append_f32_le(buffer_, v);
        return *this;
    }

    PacketBuilder &writeString(std::string_view s)
    {
        if (s.size() > UINT32_MAX)
        {
            throw std::runtime_error("String too large to serialize with uint32 length");
        }
        writeUInt32(static_cast<uint32_t>(s.size()));
        buffer_.insert(buffer_.end(),
                       reinterpret_cast<const uint8_t *>(s.data()),
                       reinterpret_cast<const uint8_t *>(s.data()) + s.size());
        return *this;
    }

    const std::vector<uint8_t> &bytes() const { return buffer_; }

private:
    PacketBuilder() = default;

    MsgHeader header_{};
    std::vector<uint8_t> buffer_;
};

// ---------------------------
// Packet Reader
// ---------------------------

class PacketReader
{
public:
    PacketReader(MsgHeader hdr, const uint8_t *data, size_t size)
        : data_(data), size_(size)
    {

        header_ = hdr;
        cursor_ = 0;

        // Validate payload length vs provided size
        const size_t expectedTotal = header_.length;
        if (size_ < expectedTotal)
        {
            throw std::runtime_error("Buffer truncated: header length exceeds available bytes");
        }
        total_ = expectedTotal; // reader will not read beyond this packet
    }

    Opcode opcode() const { return header_.opcode; }
    uint32_t payloadLength() const { return header_.length; }
    size_t remaining() const { return total_ - cursor_; }

    uint32_t readUInt32()
    {
        require(4);
        uint32_t v = read_u32_le(data_ + cursor_);
        cursor_ += 4;
        return v;
    }

    uint64_t readUInt64()
    {
        require(8);
        uint64_t v = read_u64_le(data_ + cursor_);
        cursor_ += 8;
        return v;
    }

    bool readBool()
    {
        require(1);
        bool v = read_bool(data_ + cursor_);
        cursor_ += 1;
        return v;
    }

    float readFloat()
    {
        require(4);
        float v = read_f32_le(data_ + cursor_);
        cursor_ += 4;
        return v;
    }

    std::string readString()
    {
        uint32_t n = readUInt32();
        require(n);
        std::string s(reinterpret_cast<const char *>(data_ + cursor_), n);
        cursor_ += n;
        return s;
    }

    // If you want to skip bytes:
    void skip(size_t n)
    {
        require(n);
        cursor_ += n;
    }

private:
    void require(size_t n) const
    {
        if (cursor_ + n > total_)
        {
            throw std::runtime_error("Read beyond end of packet payload");
        }
    }

    const uint8_t *data_ = nullptr;
    size_t size_ = 0;

    MsgHeader header_{};
    size_t cursor_ = 0;

    size_t total_ = 0; // header + payload length (one packet)
};
