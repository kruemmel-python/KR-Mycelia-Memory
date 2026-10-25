#include "gpu_bridge.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected fake DLL path");
        const auto path = std::filesystem::absolute(argv[1]);
        mycelia::GpuBridge bridge;
        bridge.open(path);
        bridge.pin_until_process_exit();
        bridge.initialize(0);
        bridge.init_mycel(8,2,3);
        bridge.set_active(3);
        bridge.close();
        if (bridge.loaded()) throw std::runtime_error("bridge did not close");
#ifdef _WIN32
        // Module remains loaded after FreeLibrary, but its own resources have
        // already been released via shutdown_gpu().
        HMODULE h = GetModuleHandleW(path.c_str());
        if (!h) throw std::runtime_error("pinned DLL unexpectedly unloaded");
#endif
        std::cout << "GPU_PIN_LIFETIME_PASS\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "GPU_PIN_LIFETIME_FAIL: " << e.what() << '\n';
        return 1;
    }
}
