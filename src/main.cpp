#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <shellapi.h>
#include <tchar.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "config.h"
#include "github.h"
#include "logparse.h"
#include "zip_util.h"

namespace fs = std::filesystem;

// ---------------------------------------------------------------- состояние сборки

enum Phase { Idle = 0, Running, Success, Failed, Error };

struct AppState {
    std::mutex m;
    Phase phase = Idle;
    bool busy = false;
    std::string status = "Готов к сборке";
    RunInfo run;
    std::vector<std::string> lines;
    std::vector<Problem> problems;
    std::string resultDir;
    std::vector<std::string> resultFiles;
    std::chrono::steady_clock::time_point started;
};

static void setStatus(AppState& S, Phase p, const std::string& text) {
    std::lock_guard<std::mutex> g(S.m);
    S.phase = p;
    S.status = text;
}

static bool sleepCancelable(int ms, std::atomic<bool>& cancel) {
    for (int t = 0; t < ms; t += 100) {
        if (cancel) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return !cancel;
}

static std::string exeDir() {
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return fs::path(std::string(buf, n)).parent_path().string();
}

static void finish(AppState& S) {
    std::lock_guard<std::mutex> g(S.m);
    S.busy = false;
}

static void runBuild(GhConfig cfg, Lang lang, AppState* Sp, std::atomic<bool>* cancelp) {
    AppState& S = *Sp;
    std::atomic<bool>& cancel = *cancelp;
    {
        std::lock_guard<std::mutex> g(S.m);
        S.busy = true;
        S.run = RunInfo{};
        S.lines.clear();
        S.problems.clear();
        S.resultDir.clear();
        S.resultFiles.clear();
        S.started = std::chrono::steady_clock::now();
    }
    GitHub gh(cfg);
    std::string err;

    auto fail = [&](const std::string& msg) {
        setStatus(S, Error, msg);
        finish(S);
    };

    setStatus(S, Running, "Проверяю репозиторий...");
    RunInfo prev;
    if (!gh.latestRun(prev, err)) return fail(err);

    setStatus(S, Running, "Запускаю сборку на GitHub...");
    if (!gh.dispatch(err)) return fail(err);

    RunInfo run;
    for (int i = 0; i < 30 && run.id == 0; ++i) {
        if (!sleepCancelable(3000, cancel)) { setStatus(S, Idle, "Отменено (сборка на GitHub продолжает идти)"); return finish(S); }
        RunInfo cur;
        if (gh.latestRun(cur, err) && cur.id > prev.id) run = cur;
        setStatus(S, Running, "Жду появления запуска...");
    }
    if (run.id == 0) return fail("Запуск не появился за 90 секунд. Проверь, что в workflow есть `on: workflow_dispatch`.");

    {
        std::lock_guard<std::mutex> g(S.m);
        S.run = run;
    }

    int errorsInRow = 0;
    while (run.status != "completed") {
        if (!sleepCancelable(4000, cancel)) { setStatus(S, Idle, "Отменено (сборка на GitHub продолжает идти)"); return finish(S); }
        RunInfo cur;
        if (!gh.getRun(run.id, cur, err)) {
            if (++errorsInRow >= 5) return fail(err);
            continue;
        }
        errorsInRow = 0;
        run = cur;
        std::lock_guard<std::mutex> g(S.m);
        S.run = run;
        S.status = run.status == "queued" ? "В очереди на GitHub..." : "Сборка выполняется...";
    }

    if (run.conclusion == "success") {
        setStatus(S, Running, "Сборка успешна, скачиваю результат...");
        std::vector<ArtifactInfo> arts;
        if (!gh.listArtifacts(run.id, arts, err)) return fail(err);
        std::string dir = (fs::path(exeDir()) / "downloads" / ((lang == Lang::Cpp ? "cpp_run_" : "mod_run_") + std::to_string(run.id))).string();
        std::vector<std::string> files;
        for (const auto& a : arts) {
            if (a.expired) continue;
            std::string zip;
            if (!gh.downloadArtifact(a, zip, err)) return fail(err);
            if (!zipExtract(zip, (fs::path(dir) / a.name).string(), files, err)) return fail(err);
        }
        {
            std::lock_guard<std::mutex> g(S.m);
            S.resultDir = dir;
            S.resultFiles = files;
        }
        setStatus(S, Success, files.empty() ? "Сборка успешна, но артефактов нет" : "Сборка успешна, файлы скачаны");
        return finish(S);
    }

    setStatus(S, Running, "Сборка упала, читаю лог...");
    std::string zip, text;
    bool got = false;
    for (int t = 0; t < 5 && !got; ++t) {
        got = gh.downloadLogs(run.id, zip, err);
        if (!got && !sleepCancelable(3000, cancel)) return finish(S);
    }
    if (!got) {
        {
            std::lock_guard<std::mutex> g(S.m);
            S.phase = Failed;
        }
        return fail("Сборка завершилась: " + run.conclusion + ". Лог скачать не удалось: " + err);
    }
    if (!zipReadLogs(zip, text, err)) return fail(err);

    auto lines = cleanLines(text);
    auto problems = parseProblems(lines, lang);
    {
        std::lock_guard<std::mutex> g(S.m);
        S.lines = std::move(lines);
        S.problems = std::move(problems);
    }
    setStatus(S, Failed, "Сборка завершилась с ошибкой: " + run.conclusion);
    finish(S);
}

// ---------------------------------------------------------------- DirectX 11

static ID3D11Device* g_dev = nullptr;
static ID3D11DeviceContext* g_ctx = nullptr;
static IDXGISwapChain* g_swap = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;
static bool g_occluded = false;
static UINT g_resizeW = 0, g_resizeH = 0;

static void CreateRenderTarget() {
    ID3D11Texture2D* back = nullptr;
    g_swap->GetBuffer(0, IID_PPV_ARGS(&back));
    g_dev->CreateRenderTargetView(back, nullptr, &g_rtv);
    back->Release();
}

static void CleanupRenderTarget() {
    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
}

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL level;
    const D3D_FEATURE_LEVEL levels[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
                                                D3D11_SDK_VERSION, &sd, &g_swap, &g_dev, &level, &g_ctx);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
                                            &sd, &g_swap, &g_dev, &level, &g_ctx);
    if (res != S_OK) return false;
    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_swap) { g_swap->Release(); g_swap = nullptr; }
    if (g_ctx) { g_ctx->Release(); g_ctx = nullptr; }
    if (g_dev) { g_dev->Release(); g_dev = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED) return 0;
            g_resizeW = (UINT)LOWORD(lParam);
            g_resizeH = (UINT)HIWORD(lParam);
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------- интерфейс

struct Ui {
    char owner[128]{}, repo[128]{}, workflow[128]{}, ref[128]{}, token[512]{};
    int selected = -1;
    std::string note;  // «сохранено» и т.п.
    std::thread worker;
    std::atomic<bool> cancel{false};
};

static void copyStr(char* dst, size_t n, const std::string& s) {
    std::snprintf(dst, n, "%s", s.c_str());
}

static GhConfig uiToConfig(const Ui& u) {
    GhConfig c;
    c.owner = u.owner;
    c.repo = u.repo;
    c.workflow = u.workflow;
    c.ref = u.ref;
    c.token = u.token;
    return c;
}

static void joinWorker(Ui& ui) {
    if (ui.worker.joinable()) ui.worker.join();
}

// Один компилятор = одна вкладка: свои настройки репозитория, своя сборка, свои результаты.
struct Tab {
    const char* title;
    Lang lang;
    AppState state;
    Ui ui;
    Tab(const char* t, Lang l) : title(t), lang(l) {}
};

static Tab g_tabs[2] = {Tab("Мод (Java)", Lang::Java), Tab("C++", Lang::Cpp)};

static AppConfig uiToAppConfig() {
    AppConfig a;
    a.mod = uiToConfig(g_tabs[0].ui);
    a.cpp = uiToConfig(g_tabs[1].ui);
    return a;
}

static void drawPanel(Tab& tab) {
    AppState& S = tab.state;
    Ui& ui = tab.ui;
    const bool isCpp = tab.lang == Lang::Cpp;

    std::lock_guard<std::mutex> lock(S.m);

    // --- настройки
    if (ui.token[0] == 0) ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::CollapsingHeader("Настройки GitHub")) {
        ImGui::SetNextItemWidth(360);
        ImGui::InputText("Владелец (логин или организация)", ui.owner, sizeof(ui.owner));
        ImGui::SetNextItemWidth(360);
        ImGui::InputText(isCpp ? "Репозиторий с проектом C++" : "Репозиторий с модом", ui.repo, sizeof(ui.repo));
        ImGui::SetNextItemWidth(360);
        ImGui::InputText("Файл workflow", ui.workflow, sizeof(ui.workflow));
        ImGui::SetNextItemWidth(360);
        ImGui::InputText("Ветка", ui.ref, sizeof(ui.ref));
        ImGui::SetNextItemWidth(360);
        ImGui::InputText("Токен (PAT)", ui.token, sizeof(ui.token), ImGuiInputTextFlags_Password);
        if (ImGui::Button("Сохранить настройки")) {
            ui.note = saveConfig(uiToAppConfig()) ? "Сохранено (токен зашифрован Windows)" : "Не удалось сохранить";
        }
        if (!ui.note.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", ui.note.c_str());
        }
    }

    // --- управление
    ImGui::Spacing();
    bool busy = S.busy;
    if (busy) ImGui::BeginDisabled();
    if (ImGui::Button("Собрать", ImVec2(140, 0))) {
        GhConfig cfg = uiToConfig(ui);
        if (cfg.owner.empty() || cfg.repo.empty() || cfg.token.empty()) {
            S.phase = Error;
            S.status = "Заполни владельца, репозиторий и токен в настройках";
        } else {
            saveConfig(uiToAppConfig());
            ui.selected = -1;
            ui.cancel = false;
            S.busy = true;  // сразу, чтобы кнопка не нажалась дважды
            S.phase = Running;
            S.status = "Старт...";
            S.started = std::chrono::steady_clock::now();
            joinWorker(ui);
            ui.worker = std::thread(runBuild, cfg, tab.lang, &S, &ui.cancel);
        }
    }
    if (busy) ImGui::EndDisabled();
    if (busy) {
        ImGui::SameLine();
        if (ImGui::Button("Отмена")) ui.cancel = true;
    }

    ImVec4 col = ImVec4(0.8f, 0.8f, 0.8f, 1);
    if (S.phase == Success) col = ImVec4(0.4f, 0.9f, 0.4f, 1);
    if (S.phase == Failed || S.phase == Error) col = ImVec4(1.0f, 0.45f, 0.4f, 1);
    if (S.phase == Running) col = ImVec4(1.0f, 0.85f, 0.4f, 1);
    ImGui::SameLine();
    std::string line = S.status;
    if (busy) {
        static const char* spin = "|/-\\";
        int sec = (int)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - S.started).count();
        char buf[64];
        std::snprintf(buf, sizeof(buf), "  %c  %d с", spin[(int)(ImGui::GetTime() * 8) & 3], sec);
        line += buf;
    }
    ImGui::TextColored(col, "%s", line.c_str());
    ImGui::Separator();

    // --- вкладки
    int errors = 0, warnings = 0;
    for (const auto& p : S.problems) (p.severity == "error" ? errors : warnings)++;
    char tabProblems[96];
    std::snprintf(tabProblems, sizeof(tabProblems), "Проблемы (%d ош., %d пред.)###problems", errors, warnings);

    if (ImGui::BeginTabBar("tabs")) {
        if (ImGui::BeginTabItem(tabProblems)) {
            if (S.problems.empty()) {
                if (S.phase == Failed)
                    ImGui::TextWrapped("Ошибок в формате компилятора не найдено. Смотри вкладку «Вывод».");
                else
                    ImGui::TextDisabled("Здесь появятся ошибки компиляции, если сборка упадёт.");
            } else {
                if (ImGui::Button("Копировать всё")) ImGui::SetClipboardText(problemsToText(S.problems).c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("(можно вставить в чат)");
                float h = ImGui::GetContentRegionAvail().y;
                if (ImGui::BeginTable("pt", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                                      ImVec2(0, h * 0.62f))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Тип", ImGuiTableColumnFlags_WidthFixed, 80);
                    ImGui::TableSetupColumn("Файл", ImGuiTableColumnFlags_WidthStretch, 2.0f);
                    ImGui::TableSetupColumn("Стр.", ImGuiTableColumnFlags_WidthFixed, 50);
                    ImGui::TableSetupColumn("Сообщение", ImGuiTableColumnFlags_WidthStretch, 3.0f);
                    ImGui::TableHeadersRow();
                    for (int i = 0; i < (int)S.problems.size(); ++i) {
                        const Problem& p = S.problems[i];
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        bool isErr = p.severity == "error";
                        ImGui::PushStyleColor(ImGuiCol_Text, isErr ? ImVec4(1, 0.45f, 0.4f, 1) : ImVec4(1, 0.85f, 0.4f, 1));
                        char id[32];
                        std::snprintf(id, sizeof(id), "%s##r%d", isErr ? "Ошибка" : "Предупр.", i);
                        if (ImGui::Selectable(id, ui.selected == i, ImGuiSelectableFlags_SpanAllColumns)) ui.selected = i;
                        ImGui::PopStyleColor();
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(p.file.c_str());
                        ImGui::TableSetColumnIndex(2);
                        if (p.line > 0) ImGui::Text("%d", p.line);
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextWrapped("%s", p.message.c_str());
                    }
                    ImGui::EndTable();
                }
                ImGui::BeginChild("detail", ImVec2(0, 0), true);
                if (ui.selected >= 0 && ui.selected < (int)S.problems.size()) {
                    const Problem& p = S.problems[ui.selected];
                    if (!p.file.empty()) ImGui::Text("%s:%d", p.file.c_str(), p.line);
                    ImGui::TextWrapped("%s", p.message.c_str());
                    if (!p.context.empty()) {
                        ImGui::Separator();
                        ImGui::TextUnformatted(p.context.c_str());
                    }
                } else {
                    ImGui::TextDisabled("Выбери строку, чтобы увидеть детали");
                }
                ImGui::EndChild();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Вывод")) {
            if (ImGui::Button("Копировать лог")) {
                std::string all;
                for (const auto& l : S.lines) { all += l; all += '\n'; }
                ImGui::SetClipboardText(all.c_str());
            }
            ImGui::BeginChild("log", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
            ImGuiListClipper clip;
            clip.Begin((int)S.lines.size());
            while (clip.Step()) {
                for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                    const std::string& l = S.lines[i];
                    bool bad = l.find("error:") != std::string::npos || l.rfind("##[error]", 0) == 0;
                    bool warn = !bad && l.find("warning:") != std::string::npos;
                    if (bad) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.45f, 0.4f, 1));
                    else if (warn) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.85f, 0.4f, 1));
                    ImGui::TextUnformatted(l.c_str());
                    if (bad || warn) ImGui::PopStyleColor();
                }
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Результат")) {
            if (S.run.id) {
                ImGui::Text("Запуск #%lld", S.run.id);
                if (!S.run.htmlUrl.empty()) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Открыть на GitHub")) ShellExecuteA(nullptr, "open", S.run.htmlUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }
            }
            if (!S.resultDir.empty()) {
                ImGui::Text("Папка: %s", S.resultDir.c_str());
                if (ImGui::Button("Открыть папку")) ShellExecuteA(nullptr, "open", S.resultDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                ImGui::Separator();
                for (const auto& f : S.resultFiles) ImGui::BulletText("%s", f.c_str());
            } else {
                ImGui::TextDisabled(isCpp ? "После успешной сборки здесь появятся скачанные файлы (.exe, .dll, .lib)."
                                          : "После успешной сборки здесь появятся скачанные файлы (.jar).");
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

static void drawUI() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("main", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (ImGui::BeginTabBar("compilers")) {
        for (int i = 0; i < 2; ++i) {
            if (ImGui::BeginTabItem(g_tabs[i].title)) {
                ImGui::PushID(i);
                drawPanel(g_tabs[i]);
                ImGui::PopID();
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

// ---------------------------------------------------------------- main

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    ImGui_ImplWin32_EnableDpiAwareness();
    float scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY));

    WNDCLASSEXW wc = {sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInst, nullptr, nullptr, nullptr, nullptr, L"ModBuilderWnd", nullptr};
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Mod Builder", WS_OVERLAPPEDWINDOW, 100, 100, (int)(1100 * scale),
                                (int)(720 * scale), nullptr, nullptr, wc.hInstance, nullptr);
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(scale);
    const char* fontPath = "C:\\Windows\\Fonts\\segoeui.ttf";
    if (fs::exists(fontPath))
        io.Fonts->AddFontFromFileTTF(fontPath, 18.0f * scale, nullptr, io.Fonts->GetGlyphRangesCyrillic());
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);

    {
        AppConfig cfg = loadConfig();
        const GhConfig* src[2] = {&cfg.mod, &cfg.cpp};
        for (int i = 0; i < 2; ++i) {
            Ui& ui = g_tabs[i].ui;
            copyStr(ui.owner, sizeof(ui.owner), src[i]->owner);
            copyStr(ui.repo, sizeof(ui.repo), src[i]->repo);
            copyStr(ui.workflow, sizeof(ui.workflow), src[i]->workflow);
            copyStr(ui.ref, sizeof(ui.ref), src[i]->ref);
            copyStr(ui.token, sizeof(ui.token), src[i]->token);
        }
    }

    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        if (g_occluded && g_swap->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            ::Sleep(10);
            continue;
        }
        g_occluded = false;

        if (g_resizeW != 0 && g_resizeH != 0) {
            CleanupRenderTarget();
            g_swap->ResizeBuffers(0, g_resizeW, g_resizeH, DXGI_FORMAT_UNKNOWN, 0);
            g_resizeW = g_resizeH = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        drawUI();
        ImGui::Render();

        const float clear[4] = {0.08f, 0.08f, 0.1f, 1.0f};
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        HRESULT hr = g_swap->Present(1, 0);
        g_occluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    for (auto& t : g_tabs) t.ui.cancel = true;
    for (auto& t : g_tabs) joinWorker(t.ui);

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
