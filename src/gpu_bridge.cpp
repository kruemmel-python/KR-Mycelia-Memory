#include "gpu_bridge.hpp"
#include <filesystem>
#include <algorithm>
#include <limits>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace mycelia {
namespace {
void* load_native(const std::filesystem::path& p) {
#ifdef _WIN32
    // Disable current-directory and PATH DLL searches for dependencies.
    return reinterpret_cast<void*>(LoadLibraryExW(p.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS));
#else
    return dlopen(p.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}
void unload_native(void* handle) noexcept {
    if (!handle) return;
#ifdef _WIN32
    FreeLibrary(reinterpret_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}
void* symbol(void* h, const char* name) {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(h), name));
#else
    return dlsym(h, name);
#endif
}
template<typename T> T required(void* h, const char* name) {
    auto* p = symbol(h, name);
    if (!p) throw std::runtime_error(std::string("Original CC_OpenCL export missing: ") + name);
    return reinterpret_cast<T>(p);
}
}
void GpuBridge::pin_until_process_exit() {
#ifdef _WIN32
    if (!module_) throw std::logic_error("CC_OpenCL not loaded");
    // Pin by an address inside the actual loaded module, not by a name that
    // could resolve to a different DLL. Windows keeps the image mapped until
    // process termination, even after our explicit FreeLibrary call.
    HMODULE pinned = nullptr;
    const auto address = reinterpret_cast<LPCWSTR>(
        reinterpret_cast<const void*>(initialize_gpu_));
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_PIN,
                            address, &pinned) ||
        pinned != reinterpret_cast<HMODULE>(module_)) {
        throw std::runtime_error("Could not pin CC_OpenCL module");
    }
#else
    throw std::runtime_error("DLL pinning is only available on Windows");
#endif
}
GpuBridge::~GpuBridge() noexcept { close(); }
void GpuBridge::close() noexcept {
    if (module_ && device_ >= 0 && shutdown_gpu_) shutdown_gpu_(device_);
    device_ = -1;
    capacity_ = active_ = 0;
    unload_native(module_);
    module_ = nullptr;
    initialize_gpu_ = nullptr; shutdown_gpu_ = nullptr; init_mycel_ = nullptr;
    set_active_ = nullptr; update_ = nullptr; nutrient_ = nullptr; error_ = nullptr;
    save_mycel_ = nullptr; load_mycel_ = nullptr;
}
void GpuBridge::open(const std::filesystem::path& p) {
    if (module_) throw std::logic_error("Close active engine before loading another");
    if (!p.is_absolute() || !std::filesystem::is_regular_file(p))
        throw std::invalid_argument("Explicit existing absolute library path required");
    void* h = load_native(p);
    if (!h) throw std::runtime_error("Could not load original CC_OpenCL library");
    try {
        const auto a = required<InitGpu>(h,"initialize_gpu");
        const auto b = required<ShutdownGpu>(h,"shutdown_gpu");
        const auto c = required<InitMycel>(h,"subqg_init_mycel");
        const auto d = required<SetActive>(h,"subqg_set_active_T");
        const auto e = required<StepMycel>(h,"step_mycel_update");
        const auto f = required<ReadNutrient>(h,"read_nutrient");
        const auto g = required<LastError>(h,"cc_get_last_error");
        module_=h; initialize_gpu_=a; shutdown_gpu_=b; init_mycel_=c;
        set_active_=d; update_=e; nutrient_=f; error_=g;
        save_mycel_ = reinterpret_cast<SaveMycel>(symbol(h,"save_mycel_state"));
        load_mycel_ = reinterpret_cast<LoadMycel>(symbol(h,"load_mycel_state"));
    } catch (...) { unload_native(h); throw; }
}
std::string GpuBridge::last_error() const {
    if (!module_ || !error_) return "CC_OpenCL not loaded";
    const char* msg = error_();
    return msg ? std::string(msg) : "Unknown CC_OpenCL error";
}
void GpuBridge::initialize(int gpu_index) {
    if (!module_) throw std::logic_error("CC_OpenCL not loaded");
    if (device_ >= 0) throw std::logic_error("Device already initialized");
    if (gpu_index < 0) throw std::invalid_argument("Negative GPU index");
    if (!initialize_gpu_(gpu_index)) throw std::runtime_error("GPU initialization failed: " + last_error());
    device_ = gpu_index;
}
void GpuBridge::init_mycel(int capacity,int channels,int neighbors) {
    if (device_ < 0) throw std::logic_error("GPU not initialized");
    if (capacity <= 0 || channels <= 0 || neighbors <= 0)
        throw std::invalid_argument("Mycel dimensions must be positive");
    if (!init_mycel_(device_,capacity,channels,neighbors))
        throw std::runtime_error("Original mycel initialization failed: " + last_error());
    capacity_=capacity; active_=0;
}
void GpuBridge::set_active(int active) {
    if (capacity_ == 0) throw std::logic_error("Mycel not initialized");
    if (active < 0 || active > capacity_) throw std::out_of_range("Active mycel cells exceed capacity");
    if (!set_active_(device_,active)) throw std::runtime_error("set_active failed: " + last_error());
    active_=active;
}
void GpuBridge::update(std::span<const float> activity) {
    if (active_ == 0) throw std::logic_error("No active mycel cells");
    if (activity.size() != static_cast<size_t>(active_)) throw std::invalid_argument("Activity length mismatch");
    std::vector<float> full(static_cast<size_t>(capacity_), 0.0f);
    std::copy(activity.begin(), activity.end(), full.begin());
    if (!update_(device_,full.data())) throw std::runtime_error("Mycel step failed: " + last_error());
}
std::vector<float> GpuBridge::nutrient() const {
    if (capacity_ == 0) throw std::logic_error("Mycel not initialized");
    std::vector<float> out(static_cast<size_t>(capacity_));
    if (!nutrient_(device_,out.data())) throw std::runtime_error("Read nutrient failed: " + last_error());
    out.resize(static_cast<size_t>(active_));
    return out;
}
}
