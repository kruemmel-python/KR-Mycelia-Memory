#include "gpu_bridge.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static void ensure(bool condition, const char* msg) {
    if (!condition) throw std::runtime_error(msg);
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try {
        auto dll = std::filesystem::absolute(argv[1]);
        const auto saved = std::filesystem::temp_directory_path() / "mycelia_resume_test_state.mycl";
        std::filesystem::remove(saved);
        {
            mycelia::GpuBridge first;
            first.open(dll);
            first.initialize(0);
            first.init_mycel(24,2,3);
            first.set_active(6);
            first.update(std::vector<float>(6,0.15f));
            first.save_mycel_state(saved);
            ensure(first.capacity()==24 && first.active_cells()==6, "bad initial dimensions");
        }
        {
            mycelia::GpuBridge second;
            second.open(dll);
            second.initialize(0);
            second.load_mycel_state(saved);
            ensure(second.capacity()==24 && second.active_cells()==6,
                   "restored dimensions missing in C++ bridge");
            second.update(std::vector<float>(6,0.15f));
            const auto nutrient = second.nutrient();
            ensure(nutrient.size()==6, "nutrient readback after load failed");
            std::cout << "PASS: restored native MYCL state remains step-able and readable\n";
        }
        std::filesystem::remove(saved);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
