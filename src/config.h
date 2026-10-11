#pragma once
#include "github.h"

// У каждого компилятора (вкладки «Мод (Java)» и «C++») свой репозиторий и свои настройки.
struct AppConfig {
    GhConfig mod;
    GhConfig cpp;
};

// Настройки лежат в %APPDATA%\ModBuilder\config.json, токены шифруются через Windows DPAPI
// (расшифровать может только этот пользователь Windows на этом компьютере).
AppConfig loadConfig();
bool saveConfig(const AppConfig& cfg);
