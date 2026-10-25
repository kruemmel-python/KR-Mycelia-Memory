#include "gpu_bridge.hpp"
#include "runtime_cycle.hpp"
#include "mycelia.hpp"
#include <array>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
int integer(std::string_view s, int lower, int upper) {
    int value = 0;
    const auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    if (result.ec != std::errc{} || result.ptr != s.data() + s.size() || value < lower || value > upper)
        throw std::invalid_argument("Invalid numeric option: " + std::string(s));
    return value;
}
void usage() {
    std::cout << "KR Mycelia native original-CC_OpenCL integration probe\n"
              << "  mycelia_real_gpu_probe --probe <ABSOLUTE_DLL_PATH>\n"
              << "  mycelia_real_gpu_probe --run <ABSOLUTE_DLL_PATH> <GPU_INDEX> <CAPACITY> <ACTIVE> <STEPS> <SNAPSHOT_PATH>\n"
              << "--probe loads the genuine DLL and verifies required exports; no GPU computation.\n"
              << "--run performs explicit real hardware cycles and writes a user-DPAPI-encrypted snapshot.\n"
              << "Run with a copy of the trusted original DLL, never the fake test DLL.\n";
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--help") { usage(); return 0; }
        if (argc < 3) { usage(); return 2; }
        const std::string_view mode(argv[1]);
        if ((mode == "--probe" && argc != 3) || (mode == "--run" && argc != 8) ||
            (mode != "--probe" && mode != "--run")) { usage(); return 2; }
        const std::filesystem::path dll_path(argv[2]);
        if (!dll_path.is_absolute()) throw std::invalid_argument("DLL path must be absolute");
        // Refuse the test DLL by name: this runner is for actual CC_OpenCL verification.
        auto filename = dll_path.filename().string();
        for (auto& c : filename) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        if (filename.find("test_gpu") != std::string::npos || filename.find("fake") != std::string::npos)
            throw std::invalid_argument("Test DLL refused: use the real CC_OpenCL library");
        mycelia::GpuBridge gpu;
        gpu.open(dll_path);
        std::cout << "ABI_PROBE_PASS: required CC_OpenCL exports resolved\n";
        if (mode == "--probe") return 0;
#ifdef _WIN32
        // Workaround for confirmed post-FreeLibrary crash in CC_OpenCl.dll_unloaded.
        // This pins the original DLL until process exit, while still invoking
        // shutdown_gpu() to release OpenCL resources.
        gpu.pin_until_process_exit();
        std::cout << "DLL_PINNED_UNTIL_PROCESS_EXIT\n";
#endif
#ifndef _WIN32
        throw std::runtime_error("Real Windows hardware run requires Windows and DPAPI");
#else
        const int device = integer(argv[3], 0, 32);
        const int capacity = integer(argv[4], 1, 1048576);
        const int active = integer(argv[5], 1, capacity);
        const int steps = integer(argv[6], 1, 1000);
        const std::filesystem::path snapshot(argv[7]);
        if (!snapshot.is_absolute() || snapshot.extension() != ".kms")
            throw std::invalid_argument("Snapshot must have absolute path and .kms extension");
        if (!std::filesystem::exists(snapshot.parent_path()))
            throw std::invalid_argument("Snapshot destination directory does not exist");
        // Never overwrite existing user snapshots in this hardware smoke test.
        if (std::filesystem::exists(snapshot))
            throw std::invalid_argument("Snapshot already exists; choose a new filename");
        gpu.initialize(device);
        gpu.init_mycel(capacity, 2, 3);
        gpu.set_active(active);
        mycelia::DynamicAssociativeDatabase db;
        mycelia::RuntimeCycle cycle(gpu, db);
        std::vector<float> activity(static_cast<std::size_t>(active), 0.15f);
        std::vector<double> energy(static_cast<std::size_t>(active), 0.2);
        std::vector<double> pheromone(static_cast<std::size_t>(active), 0.4);
        for (int i = 0; i < steps; ++i) {
            const auto result = cycle.step(activity, energy, pheromone);
            if (result.observed_cells != static_cast<std::size_t>(active))
                throw std::runtime_error("Inconsistent nutrient readback");
            if (!std::isfinite(result.qualia)) throw std::runtime_error("Non-finite qualia");
            if (!db.get_pattern(result.signature)) throw std::runtime_error("Attractor not stored");
            std::cout << "REAL_GPU_CYCLE_PASS step=" << i + 1
                      << " cells=" << result.observed_cells << " harmony="
                      << result.observation.harmony_score << " qualia=" << result.qualia
                      << " signature=" << result.signature << '\n';
        }
        db.save_secure_snapshot(snapshot);
        auto restored = mycelia::DynamicAssociativeDatabase::load_secure_snapshot(snapshot);
        if (restored.attractor_count() != db.attractor_count() ||
            restored.history_size() != db.history_size())
            throw std::runtime_error("Secure snapshot roundtrip mismatch");
        std::cout << "REAL_GPU_PERSISTENCE_PASS attractors=" << db.attractor_count()
                  << " history=" << db.history_size() << '\n';
        return 0;
#endif
    } catch (const std::exception& error) {
        std::cerr << "REAL_GPU_PROBE_FAIL: " << error.what() << '\n';
        return 1;
    }
}
