#pragma once

#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "ClientLua.h"
#include "Logger.h"
#include <set>

struct LuaKey
{
    using Variant = std::variant<std::string, double, bool>;
    Variant data{std::string{}};

    LuaKey() = default;
    LuaKey(const std::string& v) : data(v) {}
    LuaKey(std::string&& v) : data(std::move(v)) {}
    LuaKey(double v) : data(v) {}
    LuaKey(bool v) : data(v) {}
};

struct LuaKeyLess
{
    bool operator()(const LuaKey& a, const LuaKey& b) const
    {
        if (a.data.index() != b.data.index())
            return a.data.index() < b.data.index();

        return std::visit(
            [](const auto& lhs, const auto& rhs) -> bool
            {
                using L = std::decay_t<decltype(lhs)>;
                using R = std::decay_t<decltype(rhs)>;
                if constexpr (std::is_same_v<L, R>)
                    return lhs < rhs;
                else
                    return false;
            },
            a.data, b.data);
    }
};

struct LuaValue
{
    using Object  = std::map<LuaKey, LuaValue, LuaKeyLess>;
    using Array   = std::vector<LuaValue>;
    using Variant = std::variant<std::nullptr_t, bool, double, std::string, Array, Object, std::uintptr_t>;

    Variant data{nullptr};

    LuaValue() = default;
    LuaValue(std::nullptr_t) : data(nullptr) {}
    LuaValue(bool v) : data(v) {}
    LuaValue(double v) : data(v) {}
    LuaValue(const std::string& v) : data(v) {}
    LuaValue(std::string&& v) : data(std::move(v)) {}
    LuaValue(const Array& v) : data(v) {}
    LuaValue(Array&& v) : data(std::move(v)) {}
    LuaValue(const Object& v) : data(v) {}
    LuaValue(Object&& v) : data(std::move(v)) {}
    LuaValue(std::uintptr_t v) : data(v) {}
};

namespace LuaCppBridge
{
    inline int AbsIndex(lua_State* L, int idx)
    {
        return (idx > 0) ? idx : (ClientLua::GetTop(L) + idx + 1);
    }

    struct StackGuard
    {
        lua_State* L;
        int top;

        explicit StackGuard(lua_State* state) : L(state), top(ClientLua::GetTop(state)) {}
        ~StackGuard()
        {
            ClientLua::SetTop(L, top);
        }

        StackGuard(const StackGuard&)            = delete;
        StackGuard& operator=(const StackGuard&) = delete;
    };

    inline std::string ToStringCopy(lua_State* L, int idx)
    {
        size_t len    = 0;
        const char* s = ClientLua::ToLString(L, idx, &len);
        return s ? std::string(s, len) : std::string{};
    }

    inline bool IsIntegerNumber(lua_State* L, int idx)
    {
        if (!ClientLua::IsNumber(L, idx))
            return false;

        const double d = ClientLua::ToNumber(L, idx);
        const int i    = ClientLua::ToInteger(L, idx);
        return d == static_cast<double>(i);
    }

    inline LuaKey ReadKey(lua_State* L, int idx)
    {
        switch (ClientLua::Type(L, idx))
        {
            case LUA_TSTRING:
                return LuaKey(ToStringCopy(L, idx));
            case LUA_TNUMBER:
                return LuaKey(ClientLua::ToNumber(L, idx));
            case LUA_TBOOLEAN:
                return LuaKey(ClientLua::ToBoolean(L, idx) != 0);
        }
    }

    inline bool IsArrayLike(lua_State* L, int tableIdx)
    {
        StackGuard guard(L);
        const int tbl = AbsIndex(L, tableIdx);

        std::set<int> keys;
        int nonNumeric = 0;

        ClientLua::PushNil(L); // first key

        while (ClientLua::LuaNext(L, tbl) != 0)
        {
            // key at -2, value at -1
            const int keyType = ClientLua::Type(L, -2);

            if (keyType != LUA_TNUMBER || !IsIntegerNumber(L, -2))
            {
                ++nonNumeric;
                ClientLua::Remove(L, -1); // pop value, keep key
                continue;
            }

            const int k = ClientLua::ToInteger(L, -2);
            if (k <= 0)
            {
                ++nonNumeric;
                ClientLua::Remove(L, -1);
                continue;
            }

            keys.insert(k);
            ClientLua::Remove(L, -1); // pop value, keep key
        }

        if (nonNumeric > 0 || keys.empty())
            return false;

        int expected = 1;
        for (int k : keys)
        {
            if (k != expected)
                return false;
            ++expected;
        }
        return true;
    }

    inline LuaValue ReadValue(lua_State* L, int idx, int depth = 0, int maxDepth = 32);

    inline LuaValue::Array ReadArray(lua_State* L, int tableIdx, int depth, int maxDepth)
    {
        StackGuard guard(L);
        const int tbl = AbsIndex(L, tableIdx);

        LuaValue::Array out;
        out.reserve(8);

        for (int i = 1;; ++i)
        {
            ClientLua::RawGetI(L, tbl, i);
            if (ClientLua::Type(L, -1) == LUA_TNIL)
                break;

            out.emplace_back(ReadValue(L, -1, depth + 1, maxDepth));
            ClientLua::SetTop(L, -2); // pop fetched value
        }

        return out;
    }

    inline LuaValue::Object ReadObject(lua_State* L, int tableIdx, int depth, int maxDepth)
    {
        StackGuard guard(L);
        const int tbl = AbsIndex(L, tableIdx);

        LuaValue::Object out;
        ClientLua::PushNil(L);

        while (ClientLua::LuaNext(L, tbl) != 0)
        {
            out.emplace(ReadKey(L, -2), ReadValue(L, -1, depth + 1, maxDepth));
             ClientLua::Remove(L, -1); // pop value, keep key
        }

        return out;
    }

    inline LuaValue ReadValue(lua_State* L, int idx, int depth, int maxDepth)
    {
        if (depth > maxDepth)
            return LuaValue(std::string{"<max depth reached>"});

        switch (ClientLua::Type(L, idx))
        {
            case LUA_TNIL:
                return LuaValue(nullptr);
            case LUA_TBOOLEAN:
                return LuaValue(ClientLua::ToBoolean(L, idx) != 0);
            case LUA_TNUMBER:
                return LuaValue(ClientLua::ToNumber(L, idx));
            case LUA_TSTRING:
                return LuaValue(ToStringCopy(L, idx));
            case LUA_TTABLE:
                return IsArrayLike(L, idx) ? LuaValue(ReadArray(L, idx, depth, maxDepth)) :
                                             LuaValue(ReadObject(L, idx, depth, maxDepth));
            case LUA_TLIGHTUSERDATA:
            case LUA_TUSERDATA:
                return LuaValue(reinterpret_cast<std::uintptr_t>(ClientLua::ToUserdata(L, idx)));
            case LUA_TFUNCTION:
                return LuaValue(std::string{"<function>"});
            case LUA_TTHREAD:
                return LuaValue(std::string{"<thread>"});
            default:
                return LuaValue(std::string{"<unknown>"});
        }
    }

    inline LuaValue ReadTable(lua_State* L, int tableIdx, int maxDepth = 32)
    {
        return (ClientLua::Type(L, tableIdx) == LUA_TTABLE) ? ReadValue(L, tableIdx, 0, maxDepth) :
                                                              LuaValue(std::string{"<not a table>"});
    }

    inline bool IsValidLuaIdentifier(std::string_view s)
    {
        if (s.empty())
            return false;

        const unsigned char c0 = static_cast<unsigned char>(s.front());
        if (!(std::isalpha(c0) || s.front() == '_'))
            return false;

        for (char ch : s)
        {
            const unsigned char c = static_cast<unsigned char>(ch);
            if (!(std::isalnum(c) || ch == '_'))
                return false;
        }

        return true;
    }

    inline std::string EscapeLuaString(std::string_view s)
    {
        std::string out;
        out.reserve(s.size() + 8);

        static constexpr char hex[] = "0123456789ABCDEF";

        for (unsigned char ch : s)
        {
            switch (ch)
            {
                case '\\':
                    out += "\\\\";
                    break;
                case '"':
                    out += "\\\"";
                    break;
                case '\a':
                    out += "\\a";
                    break;
                case '\b':
                    out += "\\b";
                    break;
                case '\f':
                    out += "\\f";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                case '\v':
                    out += "\\v";
                    break;
                case '\0':
                    out += "\\0";
                    break;
                default:
                    if (ch < 32 || ch == 127)
                    {
                        out += "\\x";
                        out += hex[(ch >> 4) & 0xF];
                        out += hex[ch & 0xF];
                    }
                    else
                    {
                        out.push_back(static_cast<char>(ch));
                    }
                    break;
            }
        }

        return out;
    }

    inline void SerializeLuaValueCompact(const LuaValue& value, std::string& out);

    inline void SerializeLuaNumberCompact(double value, std::string& out)
    {
        if (!std::isfinite(value))
        {
            out += "nil";
            return;
        }

        std::ostringstream ss;
        ss << std::setprecision(17) << value;
        out += ss.str();
    }

    inline void SerializeLuaArrayCompact(const LuaValue::Array& arr, std::string& out)
    {
        out += "{";
        for (size_t i = 0; i < arr.size(); ++i)
        {
            if (i != 0)
                out += ",";
            SerializeLuaValueCompact(arr[i], out);
        }
        out += "}";
    }

    inline void SerializeLuaKeyCompact(const LuaKey& key, std::string& out)
    {
        std::visit(
            [&](const auto& k)
            {
                using T = std::decay_t<decltype(k)>;

                if constexpr (std::is_same_v<T, std::string>)
                {
                    if (IsValidLuaIdentifier(k))
                    {
                        out += k;
                        out += "=";
                    }
                    else
                    {
                        out += "[\"";
                        out += EscapeLuaString(k);
                        out += "\"]=";
                    }
                }
                else if constexpr (std::is_same_v<T, double>)
                {
                    out += "[";
                    SerializeLuaNumberCompact(k, out);
                    out += "]=";
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    out += "[";
                    out += (k ? "true" : "false");
                    out += "]=";
                }
            },
            key.data);
    }

    inline void SerializeLuaObjectCompact(const LuaValue::Object& obj, std::string& out)
    {
        out += "{";
        size_t i = 0;
        for (const auto& [key, val] : obj)
        {
            if (i++ != 0)
                out += ",";
            SerializeLuaKeyCompact(key, out);
            SerializeLuaValueCompact(val, out);
        }
        out += "}";
    }

    inline void SerializeLuaValueCompact(const LuaValue& value, std::string& out)
    {
        std::visit(
            [&](const auto& v)
            {
                using T = std::decay_t<decltype(v)>;

                if constexpr (std::is_same_v<T, std::nullptr_t>)
                {
                    out += "nil";
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    out += (v ? "true" : "false");
                }
                else if constexpr (std::is_same_v<T, double>)
                {
                    SerializeLuaNumberCompact(v, out);
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    out += "\"";
                    out += EscapeLuaString(v);
                    out += "\"";
                }
                else if constexpr (std::is_same_v<T, LuaValue::Array>)
                {
                    SerializeLuaArrayCompact(v, out);
                }
                else if constexpr (std::is_same_v<T, LuaValue::Object>)
                {
                    SerializeLuaObjectCompact(v, out);
                }
                else if constexpr (std::is_same_v<T, std::uintptr_t>)
                {
                    out += "nil";
                }
            },
            value.data);
    }

    inline std::string ToLuaSource(const LuaValue& value)
    {
        std::string out;
        out.reserve(256);
        SerializeLuaValueCompact(value, out);
        return out;
    }

    inline std::string LuaKeyToDebugString(const LuaKey& key)
    {
        return std::visit(
            [](const auto& k) -> std::string
            {
                using T = std::decay_t<decltype(k)>;

                if constexpr (std::is_same_v<T, std::string>)
                {
                    return "\"" + k + "\"";
                }
                else if constexpr (std::is_same_v<T, double>)
                {
                    std::ostringstream ss;
                    ss << std::setprecision(17) << k;
                    return ss.str();
                }
                else
                {
                    return k ? "true" : "false";
                }
            },
            key.data);
    }

    inline void PrintIndent(int indent)
    {
        for (int i = 0; i < indent; ++i)
            LOG_DEBUG << ' ';
    }

    inline void PrintLuaValue(const LuaValue& v, int indent = 0)
    {
        std::visit(
            [&](const auto& value)
            {
                using T = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<T, std::nullptr_t>)
                {
                    LOG_DEBUG << "nil";
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    LOG_DEBUG << (value ? "true" : "false");
                }
                else if constexpr (std::is_same_v<T, double>)
                {
                    LOG_DEBUG << value;
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    LOG_DEBUG << '"' << value << '"';
                }
                else if constexpr (std::is_same_v<T, LuaValue::Array>)
                {
                    LOG_DEBUG << "[\n";
                    for (size_t i = 0; i < value.size(); ++i)
                    {
                        PrintIndent(indent + 2);
                        PrintLuaValue(value[i], indent + 2);
                        if (i + 1 != value.size())
                            LOG_DEBUG << ",";
                        LOG_DEBUG << "\n";
                    }
                    PrintIndent(indent);
                    LOG_DEBUG << "]";
                }
                else if constexpr (std::is_same_v<T, LuaValue::Object>)
                {
                    LOG_DEBUG << "{\n";
                    size_t i = 0;
                    for (const auto& [k, val] : value)
                    {
                        PrintIndent(indent + 2);
                        LOG_DEBUG << LuaKeyToDebugString(k) << ": ";
                        PrintLuaValue(val, indent + 2);
                        if (++i != value.size())
                            LOG_DEBUG << ",";
                        LOG_DEBUG << "\n";
                    }
                    PrintIndent(indent);
                    LOG_DEBUG << "}";
                }
                else if constexpr (std::is_same_v<T, std::uintptr_t>)
                {
                    LOG_DEBUG << "<ptr 0x" << std::hex << value << std::dec << ">";
                }
            },
            v.data);
    }
} // namespace LuaCppBridge