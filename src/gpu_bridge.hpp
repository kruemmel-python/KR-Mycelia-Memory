#pragma once
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace mycelia {
// Adapter for the ORIGINAL CC_OpenCL library: no simulation logic is replaced.
// Library identity and bitness must be verified by the deployment installer.
class GpuBridge final {
public:
    GpuBridge() = default;
    ~GpuBridge() noexcept;
    GpuBridge(const GpuBridge&) = delete;
    GpuBridge& operator=(const GpuBridge&) = delete;
    void open(const std::filesystem::path& absolute_library_path);
    void close() noexcept;
    // Windows only: pin loaded DLL for the lifetime of the process. Irreversible.
    // Use for real CC_OpenCL if its shutdown leaves DLL-owned callbacks alive.
    void pin_until_process_exit();
    [[nodiscard]] bool loaded() const noexcept { return module_ != nullptr; }
    [[nodiscard]] int capacity() const noexcept { return capacity_; }
    [[nodiscard]] int active_cells() const noexcept { return active_; }
    void initialize(int gpu_index);
    void init_mycel(int capacity, int channels, int neighbors);
    void set_active(int active);
    void update(std::span<const float> activity);
    [[nodiscard]] std::vector<float> nutrient() const;
    [[nodiscard]] std::string last_error() const;
    // Original MYCL v1 state. These exports exist in CC_OpenCL.c.
    [[nodiscard]] bool supports_mycel_persistence() const noexcept;
    void save_mycel_state(const std::filesystem::path& new_file);
    void load_mycel_state(const std::filesystem::path& verified_file);
    // Explicit, bounded parser prevents unsafe original loader from seeing malformed files.
    static void validate_mycel_file(const std::filesystem::path& file);

private:
    void* module_ = nullptr;
    int device_ = -1;
    int capacity_ = 0;
    int active_ = 0;
    using InitGpu = int (*)(int);
    using ShutdownGpu = void (*)(int);
    using InitMycel = int (*)(int,int,int,int);
    using SetActive = int (*)(int,int);
    using StepMycel = int (*)(int,const float*);
    using ReadNutrient = int (*)(int,float*);
    using LastError = const char* (*)();
    using SaveMycel = int (*)(int,const char*);
    using LoadMycel = int (*)(int,const char*);
    InitGpu initialize_gpu_ = nullptr;
    ShutdownGpu shutdown_gpu_ = nullptr;
    InitMycel init_mycel_ = nullptr;
    SetActive set_active_ = nullptr;
    StepMycel update_ = nullptr;
    ReadNutrient nutrient_ = nullptr;
    LastError error_ = nullptr;
    SaveMycel save_mycel_ = nullptr;
    LoadMycel load_mycel_ = nullptr;
};
}
