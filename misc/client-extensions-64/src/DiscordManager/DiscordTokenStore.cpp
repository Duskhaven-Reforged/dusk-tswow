#include "DiscordTokenStore.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

namespace
{
constexpr uint32_t kTokenCacheVersion = 1;

void AppendUInt32(std::vector<uint8_t>& out, uint32_t value)
{
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void AppendUInt64(std::vector<uint8_t>& out, uint64_t value)
{
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 48) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 56) & 0xFF));
}

void AppendString(std::vector<uint8_t>& out, std::string_view value)
{
    AppendUInt32(out, static_cast<uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

bool ReadUInt32(std::vector<uint8_t> const& data, size_t& offset, uint32_t& out)
{
    if (offset + 4 > data.size())
    {
        return false;
    }

    out = static_cast<uint32_t>(data[offset]) |
          (static_cast<uint32_t>(data[offset + 1]) << 8) |
          (static_cast<uint32_t>(data[offset + 2]) << 16) |
          (static_cast<uint32_t>(data[offset + 3]) << 24);
    offset += 4;
    return true;
}

bool ReadUInt64(std::vector<uint8_t> const& data, size_t& offset, uint64_t& out)
{
    if (offset + 8 > data.size())
    {
        return false;
    }

    out = static_cast<uint64_t>(data[offset]) |
          (static_cast<uint64_t>(data[offset + 1]) << 8) |
          (static_cast<uint64_t>(data[offset + 2]) << 16) |
          (static_cast<uint64_t>(data[offset + 3]) << 24) |
          (static_cast<uint64_t>(data[offset + 4]) << 32) |
          (static_cast<uint64_t>(data[offset + 5]) << 40) |
          (static_cast<uint64_t>(data[offset + 6]) << 48) |
          (static_cast<uint64_t>(data[offset + 7]) << 56);
    offset += 8;
    return true;
}

bool ReadString(std::vector<uint8_t> const& data, size_t& offset, std::string& out)
{
    uint32_t size = 0;
    if (!ReadUInt32(data, offset, size))
    {
        return false;
    }

    if (offset + size > data.size())
    {
        return false;
    }

    out.assign(reinterpret_cast<char const*>(data.data() + offset), size);
    offset += size;
    return true;
}

#ifdef _WIN32
std::filesystem::path GetCachePath(uint64_t applicationId)
{
    DWORD required = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (required == 0)
    {
        return {};
    }

    std::wstring localAppData(required, L'\0');
    DWORD written = GetEnvironmentVariableW(
        L"LOCALAPPDATA",
        localAppData.data(),
        static_cast<DWORD>(localAppData.size()));
    if (written == 0)
    {
        return {};
    }

    if (!localAppData.empty() && localAppData.back() == L'\0')
    {
        localAppData.pop_back();
    }

    return std::filesystem::path(localAppData) / L"Duskhaven" / L"ClientExtensions64" /
           (L"discord_token_" + std::to_wstring(applicationId) + L".bin");
}

bool ReadFileBytes(std::filesystem::path const& path, std::vector<uint8_t>& out)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
    {
        return false;
    }

    input.seekg(0, std::ios::end);
    std::streamsize size = input.tellg();
    if (size < 0)
    {
        return false;
    }

    input.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(size));
    if (size == 0)
    {
        return true;
    }

    input.read(reinterpret_cast<char*>(out.data()), size);
    return input.good();
}

bool WriteFileBytes(std::filesystem::path const& path, std::vector<uint8_t> const& bytes)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
    {
        std::cerr << "[Auth] Failed to create Discord token directory: " << ec.message() << std::endl;
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        return false;
    }

    if (!bytes.empty())
    {
        output.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    return output.good();
}

bool ProtectBytes(std::vector<uint8_t> const& plaintext, std::vector<uint8_t>& ciphertext)
{
    DATA_BLOB inputBlob{};
    inputBlob.pbData = const_cast<BYTE*>(plaintext.data());
    inputBlob.cbData = static_cast<DWORD>(plaintext.size());

    DATA_BLOB outputBlob{};
    if (!CryptProtectData(
            &inputBlob,
            L"Duskhaven Discord token cache",
            nullptr,
            nullptr,
            nullptr,
            CRYPTPROTECT_UI_FORBIDDEN,
            &outputBlob))
    {
        std::cerr << "[Auth] CryptProtectData failed: " << GetLastError() << std::endl;
        return false;
    }

    ciphertext.assign(outputBlob.pbData, outputBlob.pbData + outputBlob.cbData);
    LocalFree(outputBlob.pbData);
    return true;
}

bool UnprotectBytes(std::vector<uint8_t> const& ciphertext, std::vector<uint8_t>& plaintext)
{
    DATA_BLOB inputBlob{};
    inputBlob.pbData = const_cast<BYTE*>(ciphertext.data());
    inputBlob.cbData = static_cast<DWORD>(ciphertext.size());

    DATA_BLOB outputBlob{};
    if (!CryptUnprotectData(
            &inputBlob,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            CRYPTPROTECT_UI_FORBIDDEN,
            &outputBlob))
    {
        std::cerr << "[Auth] CryptUnprotectData failed: " << GetLastError() << std::endl;
        return false;
    }

    plaintext.assign(outputBlob.pbData, outputBlob.pbData + outputBlob.cbData);
    LocalFree(outputBlob.pbData);
    return true;
}
#endif
}

namespace DiscordTokenStore
{
std::optional<CachedToken> Load(uint64_t applicationId)
{
#ifdef _WIN32
    auto cachePath = GetCachePath(applicationId);
    if (cachePath.empty())
    {
        return std::nullopt;
    }

    std::vector<uint8_t> encryptedBytes;
    if (!ReadFileBytes(cachePath, encryptedBytes) || encryptedBytes.empty())
    {
        return std::nullopt;
    }

    std::vector<uint8_t> plaintext;
    if (!UnprotectBytes(encryptedBytes, plaintext))
    {
        return std::nullopt;
    }

    CachedToken token;
    size_t offset = 0;
    uint32_t version = 0;
    uint32_t tokenTypeRaw = 0;

    if (!ReadUInt32(plaintext, offset, version) ||
        version != kTokenCacheVersion ||
        !ReadUInt32(plaintext, offset, tokenTypeRaw) ||
        !ReadUInt64(plaintext, offset, token.accessTokenExpiresAtMs) ||
        !ReadString(plaintext, offset, token.accessToken) ||
        !ReadString(plaintext, offset, token.refreshToken) ||
        !ReadString(plaintext, offset, token.scopes))
    {
        std::cerr << "[Auth] Discord token cache is unreadable, ignoring it\n";
        return std::nullopt;
    }

    token.tokenType = static_cast<discordpp::AuthorizationTokenType>(tokenTypeRaw);
    return token;
#else
    (void)applicationId;
    return std::nullopt;
#endif
}

bool Save(uint64_t applicationId, CachedToken const& token)
{
#ifdef _WIN32
    auto cachePath = GetCachePath(applicationId);
    if (cachePath.empty())
    {
        return false;
    }

    std::vector<uint8_t> plaintext;
    AppendUInt32(plaintext, kTokenCacheVersion);
    AppendUInt32(plaintext, static_cast<uint32_t>(token.tokenType));
    AppendUInt64(plaintext, token.accessTokenExpiresAtMs);
    AppendString(plaintext, token.accessToken);
    AppendString(plaintext, token.refreshToken);
    AppendString(plaintext, token.scopes);

    std::vector<uint8_t> encrypted;
    if (!ProtectBytes(plaintext, encrypted))
    {
        return false;
    }

    return WriteFileBytes(cachePath, encrypted);
#else
    (void)applicationId;
    (void)token;
    return false;
#endif
}

void Clear(uint64_t applicationId)
{
#ifdef _WIN32
    auto cachePath = GetCachePath(applicationId);
    if (cachePath.empty())
    {
        return;
    }

    std::error_code ec;
    std::filesystem::remove(cachePath, ec);
    if (ec)
    {
        std::cerr << "[Auth] Failed to remove Discord token cache: " << ec.message() << std::endl;
    }
#else
    (void)applicationId;
#endif
}
}
