#pragma once

#include <cstdint>

// Public L4N plugin ABI v1. Keep this header in sync with the L4N distribution.
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
