#include <CDBCMgr/CDBCMgr.h>
#include <ClientLua.h>
#include <Logger.h>
#include <SharedDefines.h>
#include <SpellAttrDefines.h>

#include <string>
#include <unordered_map>

namespace {
std::unordered_map<uint32_t, std::string> s_cachedSpellDescriptions;

bool SafeParseSpellDescription(SpellRow* row, char* dest, size_t destSize) {
    if (!row || !dest || destSize == 0) {
        return false;
    }

    dest[0] = '\0';
#ifdef _MSC_VER
    __try {
        SpellParser::ParseText(row, dest, static_cast<uint32_t>(destSize), 0, 0, 0, 0, 1, 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LOG_ERROR << "GetSpellDescription parse failed for spellId=" << row->m_ID;
        dest[0] = '\0';
        return false;
    }
#else
    SpellParser::ParseText(row, dest, static_cast<uint32_t>(destSize), 0, 0, 0, 0, 1, 0);
#endif
    return dest[0] != '\0';
}

bool SafeCopySpellTextField(const char* src, char* dest, size_t destSize) {
    if (!src || !dest || destSize == 0) {
        return false;
    }

    dest[0] = '\0';
#ifdef _MSC_VER
    __try {
        strncpy_s(dest, destSize, src, _TRUNCATE);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dest[0] = '\0';
        return false;
    }
#else
    strncpy_s(dest, destSize, src, _TRUNCATE);
#endif
    return dest[0] != '\0';
}

bool ContainsUnresolvedSpellTokens(const char* text) {
    return text && strchr(text, '$') != nullptr;
}

bool TryGetFormattedSpellDescription(uint32_t spellId, char* dest, size_t destSize) {
    if (!dest || destSize == 0) {
        return false;
    }

    dest[0] = '\0';
#ifdef _MSC_VER
    __try {
        SpellRow row = {};
        auto itr = s_cachedSpellDescriptions.find(spellId);
        if (itr != s_cachedSpellDescriptions.end()) {
            SafeCopySpellTextField(itr->second.c_str(), dest, destSize);
            return dest[0] != '\0';
        }

        if (!ClientDB::GetLocalizedRow((void*)0xAD49D0, spellId, &row) || !row.m_description_lang) {
            s_cachedSpellDescriptions.emplace(spellId, "");
            return false;
        }

        SafeParseSpellDescription(&row, dest, destSize);
        if (ContainsUnresolvedSpellTokens(dest)) {
            dest[0] = '\0';
        }

        s_cachedSpellDescriptions.emplace(spellId, dest);
        return dest[0] != '\0';
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LOG_ERROR << "GetSpellDescription failed, returning empty string";
        return false;
    }
#else
    SpellRow row = {};
    auto itr = s_cachedSpellDescriptions.find(spellId);
    if (itr != s_cachedSpellDescriptions.end()) {
        SafeCopySpellTextField(itr->second.c_str(), dest, destSize);
        return dest[0] != '\0';
    }

    if (!ClientDB::GetLocalizedRow((void*)0xAD49D0, spellId, &row) || !row.m_description_lang) {
        s_cachedSpellDescriptions.emplace(spellId, "");
        return false;
    }

    SafeParseSpellDescription(&row, dest, destSize);
    if (ContainsUnresolvedSpellTokens(dest)) {
        dest[0] = '\0';
    }

    s_cachedSpellDescriptions.emplace(spellId, dest);
    return dest[0] != '\0';
#endif
}
}

LUA_FUNCTION(GetSpellDescription, (lua_State* L)) {
    if (ClientLua::IsNumber(L, 1)) {
        uint32_t spellId = ClientLua::GetNumber(L, 1);
        char dest[1024] = {};
        if (TryGetFormattedSpellDescription(spellId, dest, sizeof(dest))) {
            ClientLua::PushString(L, dest);
            return 1;
        }
    }

    ClientLua::PushString(L, "");
    return 1;
}
LUA_FUNCTION(GetSpellNameById, (lua_State* L)) {
    if (ClientLua::IsNumber(L, 1)) {
        uint32_t spellId = ClientLua::GetNumber(L, 1);
        SpellRow row;

        if (ClientDB::GetLocalizedRow((void*)0xAD49D0, spellId, &row)) {
            ClientLua::PushString(L, row.m_name_lang);
            ClientLua::PushString(L, row.m_nameSubtext_lang);
            return 2;
        }
    }

    ClientLua::PushNil(L);
    ClientLua::PushNil(L);
    return 2;
}
#pragma optimize("", off)
LUA_FUNCTION(GetSpellIconById, (lua_State* L)) {
    if (ClientLua::IsNumber(L, 1)) {
        uint32_t spellId = ClientLua::GetNumber(L, 1);
        SpellRow row;
        if (ClientDB::GetLocalizedRow((void*)0xAD49D0, spellId, &row)) {
            SpellIconRow* iconRow = reinterpret_cast<SpellIconRow*>(ClientDB::GetRow(reinterpret_cast<void*>(0xAD48A4), row.m_spellIconID));
            if (iconRow) {
                ClientLua::PushString(L,iconRow->m_textureFilename);
            }else{
                ClientLua::PushNil(L);
            }
            return 1;
        }
    }

    ClientLua::PushNil(L);
    return 1;
}

LUA_FUNCTION(SpellRequiresComboPoints, (lua_State* L)) {
    if (ClientLua::IsNumber(L, 1)) {
        uint32_t spellId = ClientLua::GetNumber(L, 1);
        SpellRow row;

        if (ClientDB::GetLocalizedRow((void*)0xAD49D0, spellId, &row)) {
            ClientLua::PushBoolean(L, HasAttribute(&row, SPELL_ATTR1_CU_COMBODAMAGE) || HasAttribute(&row, SPELL_ATTR1_CU_COMBODURATION));
            return 1;
        }
    }

    ClientLua::PushBoolean(L, false);
    return 1;
}

LUA_FUNCTION(UnitCustomCastingData, (lua_State* L)) {
    if (!ClientLua::IsString(L, 1))
        ClientLua::DisplayError(L, "Usage: UnitCustomCastingData(\"unit\")", "");

    CGUnit* unitFromName = ClntObjMgr::GetUnitFromName(ClientLua::ToLString(L, 1, 0));

    SpellRow buffer;
    float spellId = 0.f;
    bool hideCastbar = false;
    bool invertCastbar = false;
    uint32_t currentCast = 0;

    if (!unitFromName)
        return 0;

    if (unitFromName->currentCastId)
        currentCast = unitFromName->currentCastId;

    if (!currentCast && unitFromName->currentChannelId)
        currentCast = unitFromName->currentChannelId;

    if (!currentCast || !ClientDB::GetLocalizedRow((void*)0xAD49D0, currentCast, &buffer))
        return 0;

    spellId = static_cast<float>(buffer.m_ID);
    double castTime = SpellRec_C::GetCastTime(&buffer, 0, 0, 1);
    SpellAdditionalAttributesRow* customAttributesRow = GlobalCDBCMap.getRow<SpellAdditionalAttributesRow>("SpellAdditionalAttributes", buffer.m_ID);

    if (customAttributesRow && (customAttributesRow->customAttr2 & SPELL_ATTR2_CU_FORCE_HIDE_CASTBAR))
        hideCastbar = true;

    if (castTime <= 250 && (customAttributesRow && (customAttributesRow->customAttr2 & SPELL_ATTR2_CU_LOW_TIME_FORCE_HIDE_CASTBAR)))
        hideCastbar = true;

    if (customAttributesRow && (customAttributesRow->customAttr2 & SPELL_ATTR2_CU_INVERT_CASTBAR))
        invertCastbar = true;

    ClientLua::PushNumber(L, spellId);
    ClientLua::PushBoolean(L, hideCastbar);
    ClientLua::PushBoolean(L, invertCastbar);
    return 3;
}
#pragma optimize("", on)
