#include <windows.h>

#include <cstdint>

class IL4NPlugin {
public:
    virtual ~IL4NPlugin() = default;
    virtual unsigned int GetInterfaceVersion() { return 1; }
    virtual const char* GetName() { return "L4N loader probe"; }
    virtual const char* GetVersion() { return "1"; }
    virtual void OnModuleLoaded(const char*, std::uintptr_t) {}
    virtual void OnGameLaunch() {}
    virtual void OnD3DCreated(void*) {}
    virtual void OnD3DDeviceCreated(void*, bool) {}
};

namespace {

void Log(const char* event_name) {
    char path[MAX_PATH]{};
    const DWORD length = GetTempPathA(sizeof(path), path);
    if (length == 0 || length >= sizeof(path) - 32) {
        return;
    }
    lstrcatA(path, "l4n_loader_probe.log");

    HANDLE file = CreateFileA(path, FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    char line[256]{};
    const int count = wsprintfA(line, "pid=%lu %s\r\n",
        GetCurrentProcessId(), event_name);
    DWORD written = 0;
    if (count > 0) {
        WriteFile(file, line, static_cast<DWORD>(count), &written, nullptr);
    }
    CloseHandle(file);
}

class LoaderProbe final : public IL4NPlugin {
public:
    unsigned int GetInterfaceVersion() override {
        Log("GetInterfaceVersion");
        return 1;
    }

    const char* GetName() override {
        Log("GetName");
        return "L4N loader probe";
    }

    const char* GetVersion() override {
        Log("GetVersion");
        return "1";
    }

    void OnModuleLoaded(const char*, std::uintptr_t) override {
        Log("OnModuleLoaded");
    }

    void OnGameLaunch() override {
        Log("OnGameLaunch");
    }
};

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        Log("DllMain");
    }
    return TRUE;
}

extern "C" __declspec(dllexport) IL4NPlugin* GetL4NPluginInstance() {
    Log("GetL4NPluginInstance");
    static LoaderProbe probe;
    return &probe;
}
