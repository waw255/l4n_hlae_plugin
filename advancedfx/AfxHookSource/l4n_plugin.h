#pragma once

#include <cstdint>

// This is the public L4N v1 ABI shipped with the L4N distribution.
class IL4NPlugin {
public:
    virtual ~IL4NPlugin() = default;

    virtual unsigned int GetInterfaceVersion() { return 1; }
    virtual const char* GetName() { return "MyPlugin"; }
    virtual const char* GetVersion() { return "1.0"; }

    virtual void OnModuleLoaded(const char*, std::uintptr_t) {}
    virtual void OnGameLaunch() {}

    virtual void OnD3DCreated(void*) {}
    virtual void OnD3DDeviceCreated(void*, bool) {}
};

typedef IL4NPlugin* (*GetL4NPluginInstanceFunc)();

#ifdef AFX_L4N_PLUGIN
bool L4nHlaeLaunchSwitchPresent();
void L4nPluginLogEvent(const char* event_name);
void L4nPluginLogCursorState(
    int cursor_ok,
    int showing,
    unsigned long cursor_error,
    std::uintptr_t foreground_window,
    unsigned long foreground_pid,
    int game_foreground,
    int game_focus,
    int camera_enabled,
    std::uintptr_t capture_window,
    int clip_ok,
    int clip_left,
    int clip_top,
    int clip_right,
    int clip_bottom,
    int suspend_candidate);
void L4nPluginLogMouseControlState(int suspended);
#endif
