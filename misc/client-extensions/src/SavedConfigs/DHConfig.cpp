#include <ClientLua.h>
#include <Logger.h>
#include <SavedConfigs/DHConfig.h>
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    static const std::pair<char const*, char const*> kBuiltInDefaults[] = {
        {"allowDiscord", "1"},
    };

    struct DHConfigState
    {
        bool initialized = false;
        std::filesystem::path path;
        std::unordered_map<std::string, std::string> values;
        std::unordered_map<std::string, std::string> defaults;
        std::mutex mutex;
    };

    DHConfigState& State()
    {
        static DHConfigState state;
        return state;
    }

    std::filesystem::path ResolveDHConfigPath()
    {
        char buffer[MAX_PATH] = {};
        DWORD length          = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
            return std::filesystem::path(buffer).parent_path() / "WTF/DHConfig";

        return std::filesystem::current_path() / "WTF/DHConfig";
    }

    std::string Escape(std::string const& value)
    {
        std::string escaped;
        escaped.reserve(value.size());

        for (char ch : value)
        {
            switch (ch)
            {
                case '\\':
                    escaped += "\\\\";
                    break;
                case '\n':
                    escaped += "\\n";
                    break;
                case '\r':
                    escaped += "\\r";
                    break;
                case '=':
                    escaped += "\\=";
                    break;
                default:
                    escaped.push_back(ch);
                    break;
            }
        }

        return escaped;
    }

    bool ParseLine(std::string const& line, std::string& key, std::string& value)
    {
        key.clear();
        value.clear();

        bool escape          = false;
        bool sawSeparator    = false;
        std::string* current = &key;

        for (char ch : line)
        {
            if (escape)
            {
                switch (ch)
                {
                    case 'n':
                        current->push_back('\n');
                        break;
                    case 'r':
                        current->push_back('\r');
                        break;
                    default:
                        current->push_back(ch);
                        break;
                }
                escape = false;
                continue;
            }

            if (ch == '\\')
            {
                escape = true;
                continue;
            }

            if (!sawSeparator && ch == '=')
            {
                sawSeparator = true;
                current      = &value;
                continue;
            }

            current->push_back(ch);
        }

        if (escape)
            current->push_back('\\');

        return sawSeparator;
    }

    void LoadUnlocked()
    {
        DHConfigState& state = State();
        state.values.clear();
        state.path = ResolveDHConfigPath();

        std::ifstream input(state.path);
        if (!input.is_open())
        {
            LOG_INFO << "Save file missing, starting empty at " << state.path.string();
            return;
        }

        std::string line;
        size_t lineNumber = 0;
        while (std::getline(input, line))
        {
            ++lineNumber;

            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (line.empty())
                continue;

            std::string key;
            std::string value;
            if (!ParseLine(line, key, value))
            {
                LOG_WARN << "Skipping malformed save line " << lineNumber;
                continue;
            }

            state.values[key] = value;
        }

        LOG_INFO << "Loaded " << state.values.size() << " save keys from " << state.path.string();
    }

    void EnsureInitializedUnlocked()
    {
        DHConfigState& state = State();
        if (state.initialized)
            return;

        LoadUnlocked();
        state.initialized = true;
    }

    void PersistUnlocked()
    {
        DHConfigState& state = State();

        std::error_code createError;
        std::filesystem::create_directories(state.path.parent_path(), createError);
        if (createError)
        {
            LOG_ERROR << "Failed to prepare save file directory " << state.path.parent_path().string();
            return;
        }

        std::ofstream output(state.path, std::ios::out | std::ios::trunc);
        if (!output.is_open())
        {
            LOG_ERROR << "Failed to open save file for write: " << state.path.string();
            return;
        }

        std::vector<std::pair<std::string, std::string>> entries(state.values.begin(), state.values.end());
        std::sort(entries.begin(), entries.end(),
                  [](auto const& left, auto const& right) { return left.first < right.first; });

        bool first = true;
        for (auto const& [key, value] : entries)
        {
            if (!first)
                output << "\n";

            first = false;
            output << Escape(key) << "=" << Escape(value);
        }
    }

    void InitializeBuiltInDefaultsUnlocked()
    {
        DHConfigState& state = State();
        for (auto const& [key, value] : kBuiltInDefaults)
        {
            state.defaults[key] = value;
        }

        bool changed = false;
        for (auto const& [key, value] : state.defaults)
        {
            if (state.values.find(key) != state.values.end())
                continue;

            state.values[key] = value;
            changed           = true;
        }

        if (changed)
            PersistUnlocked();
    }

    void PushLuaString(lua_State* L, std::string const& value)
    {
        ClientLua::PushString(L, value.c_str());
    }

    std::string GetValueUnlocked(std::string const& key, std::string const& defaultValue)
    {
        DHConfigState& state = State();
        auto itr             = state.values.find(key);
        if (itr != state.values.end())
            return itr->second;

        auto defaultItr = state.defaults.find(key);
        if (defaultItr != state.defaults.end())
            return defaultItr->second;

        return defaultValue;
    }

    std::string TrimCopy(std::string value)
    {
        auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };

        value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                                                [&](char ch) { return !isSpace(static_cast<unsigned char>(ch)); }));
        value.erase(std::find_if(value.rbegin(), value.rend(),
                                 [&](char ch) { return !isSpace(static_cast<unsigned char>(ch)); })
                        .base(),
                    value.end());
        return value;
    }

    std::string ToLowerCopy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return value;
    }

    bool TryParseInt(std::string const& value, int32_t& parsed)
    {
        std::string trimmed = TrimCopy(value);
        if (trimmed.empty())
            return false;

        errno       = 0;
        char* end   = nullptr;
        long result = std::strtol(trimmed.c_str(), &end, 10);
        if (errno == ERANGE || end != trimmed.c_str() + trimmed.size())
            return false;

        if (result < (std::numeric_limits<int32_t>::min)() || result > (std::numeric_limits<int32_t>::max)())
            return false;

        parsed = static_cast<int32_t>(result);
        return true;
    }

    bool TryParseFloat(std::string const& value, float& parsed)
    {
        std::string trimmed = TrimCopy(value);
        if (trimmed.empty())
            return false;

        errno        = 0;
        char* end    = nullptr;
        float result = std::strtof(trimmed.c_str(), &end);
        if (errno == ERANGE || end != trimmed.c_str() + trimmed.size())
            return false;

        parsed = result;
        return true;
    }
} // namespace

void DHConfig::Initialize()
{
    std::lock_guard<std::mutex> lock(State().mutex);
    EnsureInitializedUnlocked();
    InitializeBuiltInDefaultsUnlocked();
}

std::string DHConfig::Read(std::string const& key, std::string const& defaultValue)
{
    return ReadString(key, defaultValue);
}

std::string DHConfig::ReadString(std::string const& key, std::string const& defaultValue)
{
    std::lock_guard<std::mutex> lock(State().mutex);
    EnsureInitializedUnlocked();
    return GetValueUnlocked(key, defaultValue);
}

int32_t DHConfig::ReadInt(std::string const& key, int32_t defaultValue)
{
    int32_t parsed = 0;
    return TryParseInt(ReadString(key, std::to_string(defaultValue)), parsed) ? parsed : defaultValue;
}

float DHConfig::ReadFloat(std::string const& key, float defaultValue)
{
    float parsed = 0.0f;
    return TryParseFloat(ReadString(key, std::to_string(defaultValue)), parsed) ? parsed : defaultValue;
}

void DHConfig::Write(std::string const& key, std::string const& value)
{
    std::lock_guard<std::mutex> lock(State().mutex);
    EnsureInitializedUnlocked();

    DHConfigState& state = State();
    auto itr             = state.values.find(key);
    if (itr != state.values.end() && itr->second == value)
        return;

    state.values[key] = value;
    PersistUnlocked();
}

void DHConfig::AddDefault(std::string const& key, std::string const& value)
{
    std::lock_guard<std::mutex> lock(State().mutex);
    EnsureInitializedUnlocked();

    DHConfigState& state = State();
    state.defaults[key]  = value;
}

void DHConfig::WriteDefaults()
{
    std::lock_guard<std::mutex> lock(State().mutex);
    EnsureInitializedUnlocked();

    DHConfigState& state = State();
    bool changed         = false;
    for (auto const& [key, value] : state.defaults)
    {
        if (state.values.find(key) != state.values.end())
            continue;

        state.values[key] = value;
        changed           = true;
    }

    if (changed)
        PersistUnlocked();
}

bool DHConfig::Has(std::string const& key)
{
    std::lock_guard<std::mutex> lock(State().mutex);
    EnsureInitializedUnlocked();

    DHConfigState& state = State();
    return state.values.find(key) != state.values.end() || state.defaults.find(key) != state.defaults.end();
}

LUA_FUNCTION(DHConfigRead, (lua_State * L))
{
    PushLuaString(L, DHConfig::ReadString(ClientLua::GetString(L, 1), ClientLua::GetString(L, 2)));
    return 1;
}

LUA_FUNCTION(DHConfigReadString, (lua_State * L))
{
    PushLuaString(L, DHConfig::ReadString(ClientLua::GetString(L, 1), ClientLua::GetString(L, 2)));
    return 1;
}

LUA_FUNCTION(DHConfigReadInt, (lua_State * L))
{
    ClientLua::PushNumber(
        L, DHConfig::ReadInt(ClientLua::GetString(L, 1), static_cast<int32_t>(ClientLua::GetNumber(L, 2, 0))));
    return 1;
}

LUA_FUNCTION(DHConfigReadFloat, (lua_State * L))
{
    ClientLua::PushNumber(
        L, DHConfig::ReadFloat(ClientLua::GetString(L, 1), static_cast<float>(ClientLua::GetNumber(L, 2, 0))));
    return 1;
}

LUA_FUNCTION(DHConfigWrite, (lua_State * L))
{
    DHConfig::Write(ClientLua::GetString(L, 1), ClientLua::GetString(L, 2));
    return 0;
}

LUA_FUNCTION(DHConfigWriteTable, (lua_State * L))
{
    if (ClientLua::GetTop(L) < 2)
    {
        ClientLua::PushString(L, "DHConfigWriteTable(key, table) requires 2 arguments");
        return 1;
    }

    if (ClientLua::Type(L, 1) != LUA_TSTRING)
    {
        ClientLua::PushString(L, "DHConfigWriteTable: arg1 must be a string key");
        return 1;
    }

    if (ClientLua::Type(L, 2) != LUA_TTABLE)
    {
        ClientLua::PushString(L, "DHConfigWriteTable: arg2 must be a table");
        return 1;
    }

    LuaValue root          = LuaCppBridge::ReadTable(L, 2);
    std::string serialized = LuaCppBridge::ToLuaSource(root);
    DHConfig::Write(ClientLua::GetString(L, 1), serialized);
    return 1;
}

LUA_FUNCTION(DHConfigReadTable, (lua_State * L))
{
    if (ClientLua::Type(L, 1) != LUA_TSTRING)
    {
        ClientLua::PushString(L, "DHConfigReadTable: arg1 must be a string key");
        return 1;
    }

    std::string key        = ClientLua::GetString(L, 1);
    std::string serialized = DHConfig::ReadString(key, "");

    if (serialized.empty())
    {
        if (ClientLua::GetTop(L) >= 2)
            ClientLua::PushValue(L, 2);
        else
            ClientLua::PushNil(L);
        return 1;
    }

    std::string chunk = "return " + serialized;
    if (ClientLua::LoadBuffer(L, chunk.c_str(), chunk.size(), key.c_str()) != 0)
        return 1;

    ClientLua::PCall(L, 0, 1, 0);
    return 1;
}

LUA_FUNCTION(DHConfigAddDefault, (lua_State * L))
{
    DHConfig::AddDefault(ClientLua::GetString(L, 1), ClientLua::GetString(L, 2));
    return 0;
}

LUA_FUNCTION(DHConfigWriteDefaults, (lua_State * L))
{
    DHConfig::WriteDefaults();
    return 0;
}

LUA_FUNCTION(DHConfigHas, (lua_State * L))
{
    ClientLua::PushBoolean(L, DHConfig::Has(ClientLua::GetString(L, 1)));
    return 1;
}
