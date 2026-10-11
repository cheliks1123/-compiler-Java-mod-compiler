#include "config.h"

#include <windows.h>
#include <wincrypt.h>
#include <dpapi.h>

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

fs::path configPath() {
    char buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("APPDATA", buf, MAX_PATH);
    fs::path dir = n ? fs::path(std::string(buf, n)) / "ModBuilder" : fs::path(".");
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir / "config.json";
}

std::string toHex(const BYTE* data, DWORD len) {
    static const char* h = "0123456789abcdef";
    std::string s;
    for (DWORD i = 0; i < len; ++i) {
        s += h[data[i] >> 4];
        s += h[data[i] & 15];
    }
    return s;
}

std::string fromHex(const std::string& s) {
    std::string out;
    auto v = [](char c) { return c >= 'a' ? c - 'a' + 10 : c - '0'; };
    for (size_t i = 0; i + 1 < s.size(); i += 2) out += static_cast<char>(v(s[i]) * 16 + v(s[i + 1]));
    return out;
}

std::string encrypt(const std::string& plain) {
    if (plain.empty()) return "";
    DATA_BLOB in{(DWORD)plain.size(), (BYTE*)plain.data()}, out{};
    if (!CryptProtectData(&in, L"ModBuilder token", nullptr, nullptr, nullptr, 0, &out)) return "";
    std::string hex = toHex(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return hex;
}

std::string decrypt(const std::string& hex) {
    if (hex.empty()) return "";
    std::string raw = fromHex(hex);
    DATA_BLOB in{(DWORD)raw.size(), (BYTE*)raw.data()}, out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) return "";
    std::string s((char*)out.pbData, out.cbData);
    LocalFree(out.pbData);
    return s;
}

}  // namespace

static GhConfig readOne(const json& j) {
    GhConfig c;
    c.owner = j.value("owner", c.owner);
    c.repo = j.value("repo", c.repo);
    c.workflow = j.value("workflow", c.workflow);
    c.ref = j.value("ref", c.ref);
    c.token = decrypt(j.value("token", std::string()));
    return c;
}

static json writeOne(const GhConfig& c) {
    return {{"owner", c.owner}, {"repo", c.repo}, {"workflow", c.workflow}, {"ref", c.ref},
            {"token", encrypt(c.token)}};
}

AppConfig loadConfig() {
    AppConfig a;
    try {
        std::ifstream f(configPath());
        if (!f) return a;
        json j = json::parse(f);
        if (j.contains("mod") || j.contains("cpp")) {
            if (j.contains("mod")) a.mod = readOne(j["mod"]);
            if (j.contains("cpp")) a.cpp = readOne(j["cpp"]);
        } else {
            a.mod = readOne(j);  // старый формат: один набор настроек = мод
        }
    } catch (...) {}
    return a;
}

bool saveConfig(const AppConfig& a) {
    try {
        json j = {{"mod", writeOne(a.mod)}, {"cpp", writeOne(a.cpp)}};
        std::ofstream f(configPath());
        f << j.dump(2);
        return static_cast<bool>(f);
    } catch (...) {
        return false;
    }
}
