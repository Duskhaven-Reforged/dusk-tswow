#pragma once

#include <cstdint>
#include <string>
#include "SavedConfigs/LuaCppTableBridge.h"

class DHConfig
{
  public:
    static void Initialize();

    static std::string Read(std::string const& key, std::string const& defaultValue = "");
    static std::string ReadString(std::string const& key, std::string const& defaultValue = "");
    static int32_t ReadInt(std::string const& key, int32_t defaultValue = 0);
    static float ReadFloat(std::string const& key, float defaultValue = 0.0f);
    static void Write(std::string const& key, std::string const& value);

    static void AddDefault(std::string const& key, std::string const& value);
    static void WriteDefaults();

    static bool Has(std::string const& key);
};
