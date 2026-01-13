#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>
#include "Opcodes.h"


#pragma pack(push, 1)
struct MsgHeader {
    Opcode   opcode;
    uint32_t length;
};
#pragma pack(pop)
static const std::wstring PIPE_NAME = L"duskhaven_social_sdk_pipe";
// ---------------------------
// Endianness helpers (little-endian)
// ---------------------------

static inline void append_u32_le(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

static inline uint32_t read_u32_le(const uint8_t* p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

// ---------------------------
// Packet Builder
// ---------------------------

class PacketBuilder {
public:
    static PacketBuilder CreatePacket(Opcode opcode) {
        PacketBuilder pb;
        pb.header_.opcode = opcode;
        pb.header_.length = 0;
        return pb;
    }

    PacketBuilder& writeUInt32(uint32_t v) {
        append_u32_le(buffer_, v);
        return *this;
    }

    PacketBuilder& writeString(std::string_view s) {
        if (s.size() > UINT32_MAX) {
            throw std::runtime_error("String too large to serialize with uint32 length");
        }
        writeUInt32(static_cast<uint32_t>(s.size()));
        buffer_.insert(buffer_.end(),
                       reinterpret_cast<const uint8_t*>(s.data()),
                       reinterpret_cast<const uint8_t*>(s.data()) + s.size());
        return *this;
    }

    // Placeholder send- must push to IPC somewhere later.
    void Send() {
        IPCClient c;
        if (!c.Connect(PIPE_NAME)) {
            LOG_INFO << "Failed to connect to host pipe\n";
            return;
        }
         // payload length excludes header
        const size_t payloadLen = buffer_.size();
        if (payloadLen > UINT32_MAX) {
            throw std::runtime_error("Payload too large for uint32 length");
        }
        header_.length = static_cast<uint32_t>(payloadLen);
        sendHeader(c);
        c.Send(buffer_);
    }

    const std::vector<uint8_t>& bytes() const { return buffer_; }

private:
    PacketBuilder() = default;

void sendHeader(IPCClient& c)
{
    std::vector<uint8_t> data;
    data.resize(sizeof(MsgHeader));

    const uint32_t opv = header_.opcode.raw();
    const uint32_t len = header_.length;

    // opcode (bytes 0–3)
    data[0] = static_cast<uint8_t>(opv & 0xFF);
    data[1] = static_cast<uint8_t>((opv >> 8) & 0xFF);
    data[2] = static_cast<uint8_t>((opv >> 16) & 0xFF);
    data[3] = static_cast<uint8_t>((opv >> 24) & 0xFF);

    // length (bytes 4–7)
    data[4] = static_cast<uint8_t>(len & 0xFF);
    data[5] = static_cast<uint8_t>((len >> 8) & 0xFF);
    data[6] = static_cast<uint8_t>((len >> 16) & 0xFF);
    data[7] = static_cast<uint8_t>((len >> 24) & 0xFF);

    c.Send(data);
}

    MsgHeader header_{};
    std::vector<uint8_t> buffer_;
};

// ---------------------------
// Packet Reader
// ---------------------------

class PacketReader {
public:
    PacketReader(const uint8_t* data, size_t size)
        : data_(data), size_(size) {

        parseHeader();
        cursor_ = 0;

        // Validate payload length vs provided size
        const size_t expectedTotal = header_.length;
        if (size_ < expectedTotal) {
            throw std::runtime_error("Buffer truncated: header length exceeds available bytes");
        }
        total_ = expectedTotal; // reader will not read beyond this packet
    }

    Opcode opcode() const { return header_.opcode; }
    uint32_t payloadLength() const { return header_.length; }
    size_t remaining() const { return total_ - cursor_; }

    uint32_t readUInt32() {
        require(4);
        uint32_t v = read_u32_le(data_ + cursor_);
        cursor_ += 4;
        return v;
    }

    std::string readString() {
        uint32_t n = readUInt32();
        require(n);
        std::string s(reinterpret_cast<const char*>(data_ + cursor_), n);
        cursor_ += n;
        return s;
    }

    // If you want to skip bytes:
    void skip(size_t n) {
        require(n);
        cursor_ += n;
    }

private:
    void parseHeader() {
        // Parse opcode (little-endian) and length (little-endian)
        uint32_t opv = 0;
        opv = static_cast<uint32_t>(data_[0]
            | (data_[1] << 8)
            | (data_[2] << 16)
            | (data_[3] << 24));


        header_.opcode = Opcode(opv);

        const size_t lenOffset = sizeof(Opcode);
        header_.length = read_u32_le(data_ + lenOffset);
    }

    void require(size_t n) const {
        if (cursor_ + n > total_) {
            throw std::runtime_error("Read beyond end of packet payload");
        }
    }

    const uint8_t* data_ = nullptr;
    size_t size_ = 0;

    MsgHeader header_{};
    size_t cursor_ = 0;

    size_t total_ = 0; // header + payload length (one packet)
};
