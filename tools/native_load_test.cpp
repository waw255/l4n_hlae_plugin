#include <windows.h>

#include <cstdio>
#include <cstring>

using GetL4NPluginInstanceFunc = void* (*)();

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr,
            "usage: native_load_test <plugin.dll> -insecure [-plain]\n");
        return 2;
    }

    const wchar_t* path = L"";
    wchar_t wide_path[32768]{};
    if (MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, wide_path,
            static_cast<int>(sizeof(wide_path) / sizeof(wide_path[0]))) == 0) {
        std::fprintf(stderr, "MultiByteToWideChar failed: %lu\n",
            GetLastError());
        return 3;
    }
    path = wide_path;

    const DWORD flags = (argc >= 4 && std::strcmp(argv[3], "-plain") == 0)
        ? 0
        : LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
    HMODULE module = LoadLibraryExW(path, nullptr, flags);
    if (module == nullptr) {
        std::fprintf(stderr, "LoadLibraryExW failed: %lu\n", GetLastError());
        return 4;
    }

    auto factory = reinterpret_cast<GetL4NPluginInstanceFunc>(
        GetProcAddress(module, "GetL4NPluginInstance"));
    if (factory == nullptr) {
        std::fprintf(stderr, "GetProcAddress failed: %lu\n", GetLastError());
        return 5;
    }

    void* instance = factory();
    if (instance == nullptr) {
        std::fprintf(stderr, "GetL4NPluginInstance returned null\n");
        return 6;
    }

    std::printf("LoadLibraryExW OK module=%p factory=%p instance=%p\n",
        static_cast<void*>(module), reinterpret_cast<void*>(factory), instance);
    Sleep(2000);
    return 0;
}
