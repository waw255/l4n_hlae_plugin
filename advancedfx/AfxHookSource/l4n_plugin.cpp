#include "stdafx.h"

#include "l4n_plugin.h"

#include "../shared/AfxCommandLine.h"

#include <Windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>

// Defined by main.cpp. L4N supplies the module handle after LoadLibrary has
// completed, so this reuses HLAE's normal per-module initialization path.
extern void LibraryHooksA(HMODULE hModule, LPCSTR lpLibFileName);
extern advancedfx::CCommandLine* g_CommandLine;

#ifdef AFX_L4N_PLUGIN

bool L4nHlaeLaunchSwitchPresent() {
    return g_CommandLine != nullptr &&
           g_CommandLine->FindParam(L"-l4n_hlae") != 0;
}

namespace {

std::mutex g_callback_mutex;
std::mutex g_log_mutex;
std::mutex g_disabled_warning_mutex;
bool g_disabled_warning_printed = false;

using Tier0Printf = void (*)(const char* format, ...);

constexpr char kDisabledWarning[] =
    "[WARNING] L4N_HLAE plugin is not started. Add -l4n_hlae to the game "
    "launch options to start the plugin.";

std::wstring GetLogPath() {
    wchar_t temp_path[MAX_PATH]{};
    const DWORD length = GetTempPathW(ARRAYSIZE(temp_path), temp_path);
    if (length == 0 || length >= ARRAYSIZE(temp_path)) {
        return L"l4n_hlae_plugin.log";
    }
    return std::wstring(temp_path, length) + L"l4n_hlae_plugin.log";
}

void Log(const char* format, ...) {
    char message[2048]{};
    va_list args;
    va_start(args, format);
    const int message_length = vsnprintf_s(
        message, sizeof(message), _TRUNCATE, format, args);
    va_end(args);

    if (message_length < 0) {
        return;
    }

    SYSTEMTIME now{};
    GetLocalTime(&now);

    char line[2304]{};
    const int line_length = snprintf(
        line, sizeof(line), "[%04u-%02u-%02u %02u:%02u:%02u.%03u] pid=%lu %s\r\n",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
        now.wMilliseconds, GetCurrentProcessId(), message);
    if (line_length <= 0) {
        return;
    }

    OutputDebugStringA(line);

    std::lock_guard<std::mutex> lock(g_log_mutex);
    const std::wstring path = GetLogPath();
    const HANDLE file = CreateFileW(
        path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>(line_length), &written, nullptr);
    CloseHandle(file);
}

void PrintDisabledWarning() {
    static constexpr DWORD kRetryDelayMilliseconds = 250;
    static constexpr unsigned int kRetryCount = 12;

    for (unsigned int attempt = 0; attempt < kRetryCount; ++attempt) {
        if (HMODULE tier0 = GetModuleHandleA("tier0.dll")) {
            if (Tier0Printf message = reinterpret_cast<Tier0Printf>(
                    GetProcAddress(tier0, "Msg"))) {
                message("%s\n", kDisabledWarning);
                Log("disabled warning printed through tier0.dll!Msg");
                return;
            }
            if (Tier0Printf warning = reinterpret_cast<Tier0Printf>(
                    GetProcAddress(tier0, "Warning"))) {
                warning("%s\n", kDisabledWarning);
                Log("disabled warning printed through tier0.dll!Warning");
                return;
            }
        }

        Sleep(kRetryDelayMilliseconds);
    }

    OutputDebugStringA(kDisabledWarning);
    Log("could not find tier0.dll!Warning or tier0.dll!Msg for console output");
}

DWORD WINAPI DisabledWarningThreadProc(void*) {
    // L4N can call the plugin before Source has finished opening its console.
    Sleep(3000);
    PrintDisabledWarning();
    return 0;
}

void ReportDisabled() {
    std::lock_guard<std::mutex> lock(g_disabled_warning_mutex);
    if (g_disabled_warning_printed) {
        return;
    }

    g_disabled_warning_printed = true;
    Log("HLAE disabled: launch switch -l4n_hlae is missing");

    HANDLE thread = CreateThread(nullptr, 0, &DisabledWarningThreadProc,
                                 nullptr, 0, nullptr);
    if (thread != nullptr) {
        CloseHandle(thread);
        return;
    }

    OutputDebugStringA(kDisabledWarning);
    Log("CreateThread failed for delayed console warning error=%lu",
        GetLastError());
}

const char* NormalizeModuleName(const char* module_name) {
    if (module_name == nullptr) {
        return nullptr;
    }

    const char* base_name = module_name;
    for (const char* cursor = module_name; *cursor != '\0'; ++cursor) {
        if (*cursor == '\\' || *cursor == '/') {
            base_name = cursor + 1;
        }
    }

    struct ModuleAlias {
        const char* reported;
        const char* hlae_name;
    };

    static constexpr ModuleAlias aliases[] = {
        {"launcher", "launcher.dll"},
        {"filesystem_steam", "filesystem_steam.dll"},
        {"filesystem_stdio", "filesystem_stdio.dll"},
        {"engine", "engine.dll"},
        {"inputsystem", "inputsystem.dll"},
        {"materialsystem", "materialsystem.dll"},
        {"shaderapidx9", "shaderapidx9.dll"},
        {"client", "client.dll"},
        {"client_panorama", "client_panorama.dll"},
        {"panorama", "panorama.dll"},
        {"stdshader_dx9", "stdshader_dx9.dll"},
        {"vgui2", "vgui2.dll"},
    };

    for (const ModuleAlias& alias : aliases) {
        if (_stricmp(base_name, alias.reported) == 0) {
            return alias.hlae_name;
        }

        char with_extension[MAX_PATH]{};
        snprintf(with_extension, sizeof(with_extension), "%s.dll", alias.reported);
        if (_stricmp(base_name, with_extension) == 0) {
            return alias.hlae_name;
        }
    }

    return nullptr;
}

void ProcessModuleLocked(const char* reported_name, HMODULE module,
                         const char* trigger) {
    const char* hlae_name = NormalizeModuleName(reported_name);
    if (hlae_name == nullptr || module == nullptr) {
        Log("ignored module trigger=%s name=%s handle=0x%p", trigger,
            reported_name == nullptr ? "<null>" : reported_name,
            static_cast<void*>(module));
        return;
    }

    Log("LibraryHooksA begin trigger=%s reported=%s normalized=%s handle=0x%p",
        trigger, reported_name, hlae_name, static_cast<void*>(module));
    LibraryHooksA(module, hlae_name);
    Log("LibraryHooksA complete trigger=%s normalized=%s handle=0x%p", trigger,
        hlae_name, static_cast<void*>(module));
}

void ProcessModule(const char* reported_name, HMODULE module,
                   const char* trigger) {
    std::lock_guard<std::mutex> lock(g_callback_mutex);
    ProcessModuleLocked(reported_name, module, trigger);
}

void ProcessExistingModuleLocked(const char* module_name) {
    HMODULE module = GetModuleHandleA(module_name);
    if (module != nullptr) {
        ProcessModuleLocked(module_name, module, "OnGameLaunch-existing");
    }
}

void ProcessExistingModules() {
    // Engine must be processed before client so its existing GetProcAddress
    // hook can replace client.dll!CreateInterface.
    static constexpr const char* modules[] = {
        "engine.dll",
        "shaderapidx9.dll",
        "client.dll",
        "client_panorama.dll",
        "materialsystem.dll",
        "inputsystem.dll",
        "filesystem_steam.dll",
        "filesystem_stdio.dll",
        "stdshader_dx9.dll",
        "vgui2.dll",
        "panorama.dll",
        "launcher.dll",
    };

    std::lock_guard<std::mutex> lock(g_callback_mutex);
    for (const char* module_name : modules) {
        ProcessExistingModuleLocked(module_name);
    }
}

class L4NHlaePlugin final : public IL4NPlugin {
public:
    unsigned int GetInterfaceVersion() override { return 1; }
    const char* GetName() override { return "l4n_hlae_plugin"; }
    const char* GetVersion() override { return "v0.1"; }

    void OnGameLaunch() override {
        if (!L4nHlaeLaunchSwitchPresent()) {
            ReportDisabled();
            return;
        }

        Log("OnGameLaunch received");
        ProcessExistingModules();
    }

    void OnModuleLoaded(const char* module_name,
                        std::uintptr_t handle) override {
        if (!L4nHlaeLaunchSwitchPresent()) {
            ReportDisabled();
            return;
        }

        ProcessModule(module_name, reinterpret_cast<HMODULE>(handle),
                      "OnModuleLoaded");
    }
};

}  // namespace

void L4nPluginLogEvent(const char* event_name) {
    Log("event=%s", event_name == nullptr ? "<null>" : event_name);
}

extern "C" __declspec(dllexport) IL4NPlugin* GetL4NPluginInstance() {
    Log("GetL4NPluginInstance called");
    static L4NHlaePlugin plugin;
    return &plugin;
}

#endif  // AFX_L4N_PLUGIN
