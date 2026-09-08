#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include "l4n_plugin.h"

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cwctype>
#include <cwchar>
#include <string>
#include <string_view>

namespace {

constexpr wchar_t kConfigFileName[] = L"l4n_hlae_plugin.ini";
constexpr wchar_t kDefaultHookFileName[] = L"AfxHookSource.dll";
constexpr wchar_t kLogFileName[] = L"l4n_hlae_plugin.log";
constexpr wchar_t kDefaultRelativeHlaeRoot[] = L"..\\..\\..\\..\\hlae";

void ModuleMarker() {}

std::wstring Trim(std::wstring value) {
    const auto is_space = [](wchar_t character) {
        return character == L' ' || character == L'\t' || character == L'\r' ||
               character == L'\n';
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                                            [&](wchar_t character) {
                                                return !is_space(character);
                                            }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
                             [&](wchar_t character) {
                                 return !is_space(character);
                             })
                    .base(),
                value.end());

    if (value.size() >= 2 && value.front() == L'"' && value.back() == L'"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

bool EqualsInsensitive(std::wstring_view left, std::wstring_view right) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index) {
        if (towlower(left[index]) != towlower(right[index])) {
            return false;
        }
    }
    return true;
}

bool IsAbsolutePath(const std::wstring& path) {
    return (path.size() >= 2 && path[1] == L':') ||
           (path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\');
}

std::wstring JoinPath(const std::wstring& directory, const std::wstring& child) {
    if (directory.empty()) {
        return child;
    }
    if (child.empty()) {
        return directory;
    }
    if (directory.back() == L'\\' || directory.back() == L'/') {
        return directory + child;
    }
    return directory + L'\\' + child;
}

std::wstring GetFileName(const std::wstring& path) {
    const std::size_t separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) {
        return path;
    }
    return path.substr(separator + 1);
}

std::wstring MakeAbsolutePath(const std::wstring& path) {
    if (path.empty()) {
        return {};
    }

    wchar_t buffer[32768]{};
    const DWORD length = GetFullPathNameW(path.c_str(), ARRAYSIZE(buffer), buffer,
                                          nullptr);
    if (length == 0 || length >= ARRAYSIZE(buffer)) {
        return path;
    }
    return std::wstring(buffer, length);
}

std::wstring GetModuleDirectory() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&ModuleMarker), &module)) {
        return {};
    }

    wchar_t buffer[32768]{};
    const DWORD length = GetModuleFileNameW(module, buffer, ARRAYSIZE(buffer));
    if (length == 0 || length >= ARRAYSIZE(buffer)) {
        return {};
    }

    std::wstring module_path(buffer, length);
    const std::size_t separator = module_path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) {
        return {};
    }
    module_path.resize(separator);
    return module_path;
}

std::wstring ReadIniString(const std::wstring& config_path, const wchar_t* key,
                           const wchar_t* fallback) {
    wchar_t buffer[32768]{};
    const DWORD length = GetPrivateProfileStringW(
        L"HLAE", key, fallback, buffer, ARRAYSIZE(buffer), config_path.c_str());
    return Trim(std::wstring(buffer, length));
}

bool ReadIniBool(const std::wstring& config_path, const wchar_t* key,
                bool fallback) {
    const std::wstring value = ReadIniString(config_path, key, fallback ? L"1" : L"0");
    return EqualsInsensitive(value, L"1") || EqualsInsensitive(value, L"true") ||
           EqualsInsensitive(value, L"yes") || EqualsInsensitive(value, L"on");
}

std::wstring GetLogPath() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetTempPathW(ARRAYSIZE(buffer), buffer);
    if (length == 0 || length >= ARRAYSIZE(buffer)) {
        return JoinPath(GetModuleDirectory(), kLogFileName);
    }
    return JoinPath(std::wstring(buffer, length), kLogFileName);
}

void Log(const wchar_t* format, ...) {
    wchar_t message[2048]{};
    va_list arguments;
    va_start(arguments, format);
    _vsnwprintf_s(message, ARRAYSIZE(message), _TRUNCATE, format, arguments);
    va_end(arguments);

    SYSTEMTIME time{};
    GetLocalTime(&time);

    wchar_t line[2300]{};
    _snwprintf_s(line, ARRAYSIZE(line), _TRUNCATE,
                 L"[%04u-%02u-%02u %02u:%02u:%02u.%03u] pid=%lu %s\r\n",
                 time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
                 time.wSecond, time.wMilliseconds, GetCurrentProcessId(), message);
    OutputDebugStringW(line);

    const std::wstring log_path = GetLogPath();
    const HANDLE file = CreateFileW(log_path.c_str(), FILE_APPEND_DATA,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    char utf8[4600]{};
    const int byte_count = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8,
                                                ARRAYSIZE(utf8), nullptr, nullptr);
    if (byte_count > 1) {
        DWORD written = 0;
        WriteFile(file, utf8, static_cast<DWORD>(byte_count - 1), &written, nullptr);
    }
    CloseHandle(file);
}

bool ReadFileExact(HANDLE file, void* buffer, DWORD size) {
    DWORD bytes_read = 0;
    return ReadFile(file, buffer, size, &bytes_read, nullptr) && bytes_read == size;
}

bool Is32BitPeImage(const std::wstring& path, DWORD* failure) {
    *failure = ERROR_BAD_EXE_FORMAT;

    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        *failure = GetLastError();
        return false;
    }

    LARGE_INTEGER file_size{};
    if (!GetFileSizeEx(file, &file_size)) {
        *failure = GetLastError();
        CloseHandle(file);
        return false;
    }

    IMAGE_DOS_HEADER dos_header{};
    if (file_size.QuadPart < sizeof(dos_header) ||
        !ReadFileExact(file, &dos_header, sizeof(dos_header)) ||
        dos_header.e_magic != IMAGE_DOS_SIGNATURE) {
        CloseHandle(file);
        return false;
    }

    const ULONGLONG nt_header_end =
        static_cast<ULONGLONG>(dos_header.e_lfanew) + sizeof(DWORD) +
        sizeof(IMAGE_FILE_HEADER);
    if (dos_header.e_lfanew < 0 || nt_header_end >
                                      static_cast<ULONGLONG>(file_size.QuadPart)) {
        CloseHandle(file);
        return false;
    }

    LARGE_INTEGER nt_header_offset{};
    nt_header_offset.QuadPart = dos_header.e_lfanew;
    if (!SetFilePointerEx(file, nt_header_offset, nullptr, FILE_BEGIN)) {
        *failure = GetLastError();
        CloseHandle(file);
        return false;
    }

    DWORD signature = 0;
    IMAGE_FILE_HEADER file_header{};
    if (!ReadFileExact(file, &signature, sizeof(signature)) ||
        !ReadFileExact(file, &file_header, sizeof(file_header)) ||
        signature != IMAGE_NT_SIGNATURE ||
        file_header.Machine != IMAGE_FILE_MACHINE_I386) {
        CloseHandle(file);
        return false;
    }

    *failure = ERROR_SUCCESS;
    CloseHandle(file);
    return true;
}

struct LoadConfiguration {
    std::wstring config_path;
    std::wstring hlae_root;
    std::wstring hook_path;
    bool enabled = true;
};

LoadConfiguration ReadConfiguration() {
    LoadConfiguration configuration;
    const std::wstring module_directory = GetModuleDirectory();
    configuration.config_path = JoinPath(module_directory, kConfigFileName);

    std::wstring root = ReadIniString(configuration.config_path, L"HlaeRoot", L"");
    if (root.empty()) {
        root = kDefaultRelativeHlaeRoot;
    }
    if (!IsAbsolutePath(root)) {
        root = JoinPath(module_directory, root);
    }
    configuration.hlae_root = MakeAbsolutePath(root);

    std::wstring hook = ReadIniString(configuration.config_path, L"HookDll",
                                      kDefaultHookFileName);
    if (!IsAbsolutePath(hook)) {
        hook = JoinPath(configuration.hlae_root, hook);
    }
    configuration.hook_path = MakeAbsolutePath(hook);
    configuration.enabled = ReadIniBool(configuration.config_path, L"Enabled", true);
    return configuration;
}

class L4NHlaePlugin final : public IL4NPlugin {
public:
    unsigned int GetInterfaceVersion() override { return 1; }
    const char* GetName() override { return "l4n_hlae_plugin"; }
    const char* GetVersion() override { return "v0.1"; }

    void OnGameLaunch() override {
        Log(L"OnGameLaunch received");
        RequestLoad(L"OnGameLaunch");
    }

    void OnModuleLoaded(const char* module_name, std::uintptr_t handle) override {
        if (module_name == nullptr) {
            return;
        }

        const std::string_view name(module_name);
        if (name == "engine" || name == "client" || name == "shaderapidx9") {
            if (name == "client") {
                client_module_seen_.store(true);
            }
            Log(L"OnModuleLoaded received: %S handle=0x%p", module_name,
                reinterpret_cast<void*>(handle));

            // This only covers installations where L4N does not emit OnGameLaunch.
            // It remains a normal L4N callback and never starts a second attempt.
            RequestLoad(L"OnModuleLoaded");
        }
    }

private:
    static DWORD WINAPI LoadThreadProc(void* parameter) {
        auto* plugin = static_cast<L4NHlaePlugin*>(parameter);
        plugin->LoadHlae();
        return 0;
    }

    void RequestLoad(const wchar_t* trigger) {
        bool expected = false;
        if (!load_requested_.compare_exchange_strong(expected, true)) {
            return;
        }

        trigger_ = trigger;
        const HANDLE thread = CreateThread(nullptr, 0, LoadThreadProc, this, 0, nullptr);
        if (thread == nullptr) {
            Log(L"CreateThread failed: error=%lu", GetLastError());
            return;
        }
        CloseHandle(thread);
    }

    void LoadHlae() {
        Log(L"waiting for client module before HLAE load");
        constexpr DWORD kClientWaitMs = 20000;
        const DWORD wait_start = GetTickCount();
        while (!client_module_seen_.load() &&
               GetModuleHandleW(L"client.dll") == nullptr) {
            if (GetTickCount() - wait_start >= kClientWaitMs) {
                Log(L"client module did not appear within %lu ms", kClientWaitMs);
                return;
            }
            Sleep(50);
        }

        const LoadConfiguration configuration = ReadConfiguration();
        Log(L"load attempt trigger=%s config=%s root=%s hook=%s enabled=%u",
            trigger_.c_str(), configuration.config_path.c_str(),
            configuration.hlae_root.c_str(), configuration.hook_path.c_str(),
            configuration.enabled ? 1U : 0U);

        if (!configuration.enabled) {
            Log(L"loading disabled by configuration");
            return;
        }

        const DWORD attributes = GetFileAttributesW(configuration.hook_path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            Log(L"HLAE hook DLL not found: error=%lu", GetLastError());
            return;
        }

        DWORD pe_error = ERROR_SUCCESS;
        if (!Is32BitPeImage(configuration.hook_path, &pe_error)) {
            Log(L"HLAE hook is not a valid x86 PE32 DLL: error=%lu", pe_error);
            return;
        }

        const std::wstring hook_file_name = GetFileName(configuration.hook_path);
        HMODULE existing = hook_file_name.empty()
                               ? nullptr
                               : GetModuleHandleW(hook_file_name.c_str());
        if (existing != nullptr) {
            hlae_module_ = existing;
            Log(L"HLAE hook already loaded: module=0x%p", existing);
            return;
        }

        constexpr DWORD load_flags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                      LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
        HMODULE loaded = LoadLibraryExW(configuration.hook_path.c_str(), nullptr,
                                         load_flags);
        if (loaded == nullptr) {
            Log(L"LoadLibraryExW failed: error=%lu", GetLastError());
            return;
        }

        hlae_module_ = loaded;
        Log(L"HLAE hook loaded: module=0x%p path=%s", loaded,
            configuration.hook_path.c_str());
        Log(L"HLAE command verification is manual: check a mirv_* command in-game");
    }

    std::atomic<bool> load_requested_{false};
    std::atomic<bool> client_module_seen_{false};
    std::wstring trigger_ = L"unknown";
    HMODULE hlae_module_ = nullptr;
};

}  // namespace

extern "C" __declspec(dllexport) IL4NPlugin* GetL4NPluginInstance() {
    static L4NHlaePlugin plugin;
    return &plugin;
}
