#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "../discordpp.h"

namespace DiscordTokenStore
{
struct CachedToken
{
    std::string accessToken;
    std::string refreshToken;
    std::string scopes;
    discordpp::AuthorizationTokenType tokenType = discordpp::AuthorizationTokenType::Bearer;
    uint64_t accessTokenExpiresAtMs = 0;
};

std::optional<CachedToken> Load(uint64_t applicationId);
bool Save(uint64_t applicationId, CachedToken const& token);
void Clear(uint64_t applicationId);
}
