#include "gpu_bridge.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
int parse_number(std::string_view token, int minimum, int maximum) {
    int result{};
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()
        || result < minimum || result > maximum)
        throw std::invalid_argument("Invalid numeric value: " + std::string(token));
    return result;
}
std::uint64_t observation_hash(const std::vector<float>& values) {
    // FNV-1a over float bit patterns for diagnostic comparisons, not authentication.
    std::uint64_t hash = 14695981039346656037ULL;
    for (float value : values) {
        if (!std::isfinite(value)) throw std::runtime_error("Non-finite GPU nutrient");
        auto bits = std::bit_cast<std::uint32_t>(value);
        for (int i = 0; i < 4; ++i) {
            hash ^= static_cast<unsigned char>((bits >> (8 * i)) & 0xffu);
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}
void help() {
    std::cout << "Mycelia original MYCL v1 process-restart probe (Windows GPU, controlled files)\n"
        << "  mycelia_mycel_resume_probe --save DLL GPU CAPACITY ACTIVE STEPS NEW_MYCL\n"
        << "  mycelia_mycel_resume_probe --resume DLL GPU EXISTING_MYCL STEPS NEW_MYCL\n"
        << "  mycelia_mycel_resume_probe --continuous DLL GPU CAPACITY ACTIVE STEPS NEW_MYCL\n"
        << "Each command is an independent OS process. MYCL files are UNENCRYPTED.\n"
        << "Only use local test data, never sensitive memory. Files are never overwritten.\n";
}
std::filesystem::path existing(const char* raw) {
    std::filesystem::path result(raw);
    if (!result.is_absolute() || !std::filesystem::is_regular_file(result))
        throw std::invalid_argument("Existing absolute file path required");
    return result;
}
std::filesystem::path fresh(const char* raw) {
    std::filesystem::path result(raw);
    if (!result.is_absolute() || result.extension() != ".mycl" ||
        !std::filesystem::is_directory(result.parent_path()) || std::filesystem::exists(result))
        throw std::invalid_argument("New absolute .mycl file path required");
    return result;
}
void step(mycelia::GpuBridge& gpu, int iterations) {
    if (gpu.active_cells() <= 0) throw std::runtime_error("MYCL has no active cells");
    std::vector<float> activity(static_cast<std::size_t>(gpu.active_cells()), 0.15f);
    for (int i = 0; i < iterations; ++i) {
        gpu.update(activity);
        auto readback = gpu.nutrient();
        const auto digest = observation_hash(readback);
        std::cout << "MYCL_CYCLE_PASS step=" << i+1 << " active=" << gpu.active_cells()
                  << " nutrient_fnv64=" << std::hex << std::setw(16) << std::setfill('0')
                  << digest << std::dec << '\n';
    }
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--help") { help(); return 0; }
        if (argc < 3) { help(); return 2; }
        const std::string_view mode(argv[1]);
        if (((mode == "--save" || mode == "--continuous") && argc != 8) || (mode == "--resume" && argc != 7) || (mode != "--save" && mode != "--resume" && mode != "--continuous")) {
            help(); return 2;
        }
        const auto dll = existing(argv[2]);
        if (dll.filename().string().find("test_gpu") != std::string::npos)
            throw std::invalid_argument("Real hardware runner refuses fake GPU library");
        const int gpu_id = parse_number(argv[3], 0, 32);
        mycelia::GpuBridge gpu;
        gpu.open(dll);
        if (!gpu.supports_mycel_persistence()) throw std::runtime_error("Original MYCL exports absent");
#ifdef _WIN32
        gpu.pin_until_process_exit();
        std::cout << "DLL_PINNED_UNTIL_PROCESS_EXIT\n";
#else
        throw std::runtime_error("This hardware probe runs only under Windows");
#endif
        gpu.initialize(gpu_id);
        if (mode == "--save" || mode == "--continuous") {
            const int capacity = parse_number(argv[4], 1, 65536);
            const int active = parse_number(argv[5], 1, capacity);
            const int count = parse_number(argv[6], 1, 100);
            const auto out = fresh(argv[7]);
            gpu.init_mycel(capacity, 2, 3);
            gpu.set_active(active);
            step(gpu, count);
            gpu.save_mycel_state(out);
            std::cout << (mode == "--continuous" ? "MYCL_CONTINUOUS_PASS file=" : "MYCL_SAVE_PASS file=") << out.string() << '\n';
        } else if (mode == "--resume") {
            const auto input = existing(argv[4]);
            const int count = parse_number(argv[5], 1, 100);
            const auto output = fresh(argv[6]);
            gpu.load_mycel_state(input);
            std::cout << "MYCL_LOAD_PASS capacity=" << gpu.capacity()
                      << " active=" << gpu.active_cells() << '\n';
            step(gpu, count);
            gpu.save_mycel_state(output);
            std::cout << "MYCL_RESUME_PASS file=" << output.string() << '\n';
        } else {
            help(); return 2;
        }
        gpu.close();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "MYCL_RESTART_FAIL: " << error.what() << '\n';
        return 1;
    }
}
