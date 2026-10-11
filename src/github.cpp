#include "github.h"

#include <windows.h>
#include <winhttp.h>

#include <nlohmann/json.hpp>

#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif

using json = nlohmann::json;

namespace {

std::wstring W(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

std::string U(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

struct HttpResp {
    int status = 0;
    std::string body;
    std::string error;
};

std::string apiMessage(const HttpResp& r) {
    try {
        auto j = json::parse(r.body);
        if (j.contains("message") && j["message"].is_string()) return j["message"].get<std::string>();
    } catch (...) {}
    return "";
}

std::string explain(const HttpResp& r, const char* what) {
    if (!r.error.empty()) return std::string(what) + ": " + r.error;
    std::string m = apiMessage(r);
    std::string hint;
    switch (r.status) {
        case 401: hint = "токен недействителен или просрочен"; break;
        case 403: hint = "нет прав у токена (нужны repo и workflow) или лимит запросов"; break;
        case 404: hint = "репозиторий или workflow не найден (проверь владельца, репозиторий, имя файла и права токена)"; break;
        case 422: hint = "у workflow нет `on: workflow_dispatch` или такой ветки нет"; break;
        default: break;
    }
    std::string out = std::string(what) + ": HTTP " + std::to_string(r.status);
    if (!hint.empty()) out += " — " + hint;
    if (!m.empty()) out += " (" + m + ")";
    return out;
}

}  // namespace

struct GitHub::Impl {
    HINTERNET session = nullptr;

    Impl() {
        session = WinHttpOpen(L"ModBuilder/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
    }
    ~Impl() {
        if (session) WinHttpCloseHandle(session);
    }

    // Редиректы идут вручную: токен отправляется только на api.github.com,
    // а логи/артефакты лежат на другом хосте (подписанная ссылка без авторизации).
    HttpResp request(const std::string& method, const std::string& url, const std::string& body,
                     const std::string& token) {
        HttpResp r;
        if (!session) { r.error = "WinHTTP не открылся"; return r; }
        std::string cur = url;

        for (int hop = 0; hop < 5; ++hop) {
            std::wstring wurl = W(cur);
            URL_COMPONENTSW uc{};
            uc.dwStructSize = sizeof(uc);
            wchar_t host[256], path[4096], extra[4096];
            uc.lpszHostName = host;  uc.dwHostNameLength = 256;
            uc.lpszUrlPath = path;   uc.dwUrlPathLength = 4096;
            uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 4096;
            if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) { r.error = "некорректный URL"; return r; }

            std::wstring wh(uc.lpszHostName, uc.dwHostNameLength);
            std::wstring wp(uc.lpszUrlPath, uc.dwUrlPathLength);
            wp.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);

            HINTERNET con = WinHttpConnect(session, wh.c_str(), uc.nPort, 0);
            if (!con) { r.error = "нет соединения (код " + std::to_string(GetLastError()) + ")"; return r; }
            HINTERNET req = WinHttpOpenRequest(con, W(method).c_str(), wp.c_str(), nullptr, WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES,
                                               uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
            if (!req) {
                r.error = "не удалось создать запрос";
                WinHttpCloseHandle(con);
                return r;
            }
            DWORD opt = WINHTTP_DISABLE_REDIRECTS;
            WinHttpSetOption(req, WINHTTP_OPTION_DISABLE_FEATURE, &opt, sizeof(opt));

            std::wstring hdr = L"User-Agent: ModBuilder\r\nAccept: application/vnd.github+json\r\n"
                               L"X-GitHub-Api-Version: 2022-11-28\r\n";
            if (wh == L"api.github.com" && !token.empty()) hdr += L"Authorization: Bearer " + W(token) + L"\r\n";
            if (!body.empty()) hdr += L"Content-Type: application/json\r\n";

            BOOL ok = WinHttpSendRequest(req, hdr.c_str(), (DWORD)-1L,
                                         body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
                                         (DWORD)body.size(), (DWORD)body.size(), 0) &&
                      WinHttpReceiveResponse(req, nullptr);
            if (!ok) {
                r.error = "ошибка сети (код " + std::to_string(GetLastError()) + ")";
                WinHttpCloseHandle(req);
                WinHttpCloseHandle(con);
                return r;
            }

            DWORD code = 0, sz = sizeof(code);
            WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);
            r.status = (int)code;

            std::string location;
            wchar_t loc[4096];
            DWORD ls = sizeof(loc);
            if (WinHttpQueryHeaders(req, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, loc, &ls,
                                    WINHTTP_NO_HEADER_INDEX))
                location = U(loc);

            r.body.clear();
            for (;;) {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0) break;
                std::string buf(avail, '\0');
                DWORD rd = 0;
                if (!WinHttpReadData(req, &buf[0], avail, &rd) || rd == 0) break;
                r.body.append(buf.data(), rd);
            }
            WinHttpCloseHandle(req);
            WinHttpCloseHandle(con);

            bool redirect = code == 301 || code == 302 || code == 303 || code == 307 || code == 308;
            if (redirect && !location.empty()) {
                cur = location;
                continue;
            }
            return r;
        }
        r.error = "слишком много перенаправлений";
        return r;
    }
};

GitHub::GitHub(GhConfig cfg) : d(new Impl), cfg_(std::move(cfg)) {}
GitHub::~GitHub() { delete d; }

std::string GitHub::base() const {
    return "https://api.github.com/repos/" + cfg_.owner + "/" + cfg_.repo;
}

static RunInfo parseRun(const json& j) {
    RunInfo r;
    r.id = j.value("id", 0LL);
    if (j.contains("status") && j["status"].is_string()) r.status = j["status"].get<std::string>();
    if (j.contains("conclusion") && j["conclusion"].is_string()) r.conclusion = j["conclusion"].get<std::string>();
    if (j.contains("html_url") && j["html_url"].is_string()) r.htmlUrl = j["html_url"].get<std::string>();
    return r;
}

bool GitHub::latestRun(RunInfo& out, std::string& err) {
    auto r = d->request("GET", base() + "/actions/workflows/" + cfg_.workflow +
                                   "/runs?per_page=1&event=workflow_dispatch",
                        "", cfg_.token);
    if (r.status != 200) { err = explain(r, "Список запусков"); return false; }
    try {
        auto j = json::parse(r.body);
        out = RunInfo{};
        if (j.contains("workflow_runs") && !j["workflow_runs"].empty()) out = parseRun(j["workflow_runs"][0]);
        return true;
    } catch (const std::exception& e) {
        err = std::string("Ответ GitHub не разобран: ") + e.what();
        return false;
    }
}

bool GitHub::dispatch(std::string& err) {
    json body = {{"ref", cfg_.ref}};
    auto r = d->request("POST", base() + "/actions/workflows/" + cfg_.workflow + "/dispatches", body.dump(),
                        cfg_.token);
    if (r.status != 204) { err = explain(r, "Запуск сборки"); return false; }
    return true;
}

bool GitHub::getRun(long long id, RunInfo& out, std::string& err) {
    auto r = d->request("GET", base() + "/actions/runs/" + std::to_string(id), "", cfg_.token);
    if (r.status != 200) { err = explain(r, "Статус сборки"); return false; }
    try {
        out = parseRun(json::parse(r.body));
        return true;
    } catch (const std::exception& e) {
        err = std::string("Ответ GitHub не разобран: ") + e.what();
        return false;
    }
}

bool GitHub::downloadLogs(long long runId, std::string& zipData, std::string& err) {
    auto r = d->request("GET", base() + "/actions/runs/" + std::to_string(runId) + "/logs", "", cfg_.token);
    if (r.status != 200) { err = explain(r, "Скачивание логов"); return false; }
    zipData = std::move(r.body);
    return true;
}

bool GitHub::listArtifacts(long long runId, std::vector<ArtifactInfo>& out, std::string& err) {
    auto r = d->request("GET", base() + "/actions/runs/" + std::to_string(runId) + "/artifacts", "", cfg_.token);
    if (r.status != 200) { err = explain(r, "Список артефактов"); return false; }
    try {
        auto j = json::parse(r.body);
        out.clear();
        for (const auto& a : j["artifacts"]) {
            ArtifactInfo i;
            i.id = a.value("id", 0LL);
            i.name = a.value("name", std::string());
            i.size = a.value("size_in_bytes", 0LL);
            i.expired = a.value("expired", false);
            i.downloadUrl = a.value("archive_download_url", std::string());
            out.push_back(i);
        }
        return true;
    } catch (const std::exception& e) {
        err = std::string("Ответ GitHub не разобран: ") + e.what();
        return false;
    }
}

bool GitHub::downloadArtifact(const ArtifactInfo& a, std::string& zipData, std::string& err) {
    auto r = d->request("GET", a.downloadUrl, "", cfg_.token);
    if (r.status != 200) { err = explain(r, "Скачивание артефакта"); return false; }
    zipData = std::move(r.body);
    return true;
}
