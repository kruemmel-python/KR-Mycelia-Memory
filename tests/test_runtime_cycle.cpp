#include "runtime_cycle.hpp"
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace mycelia;
static void check(bool b, const char* msg) { if (!b) throw std::runtime_error(msg); }
int main(int argc, char** argv) {
    try {
        check(argc == 2, "test DLL path required");
        GpuBridge gpu;
        gpu.open(std::filesystem::absolute(argv[1]));
        gpu.initialize(0);
        gpu.init_mycel(8, 2, 3);
        gpu.set_active(3);
        DynamicAssociativeDatabase db;
        RuntimeCycle cycle(gpu, db);
        const std::array<float,3> activity{0.1f,0.2f,0.3f};
        const std::array<double,3> energy{0.0,1.0,2.0};
        const std::array<double,3> pheromone{0.2,0.4,0.6};
        auto result = cycle.step(activity, energy, pheromone);
        check(result.observed_cells==3, "wrong number of cells");
        check(db.attractor_count()==1 && db.history_size()==1, "not observed");
        check(db.get_pattern(result.signature)!=nullptr, "missing attractor");
        check(std::abs(result.observation.harmony_score-0.4)<1e-12, "harmony mismatch");
        check(std::abs(result.observation.tension_score-0.15)<1e-12, "tension mismatch");
        check(std::abs(result.qualia-0.125)<1e-12, "qualia mismatch");
        check(std::abs(db.get_pattern(result.signature)->nutrient_mean-0.25)<1e-12,
              "GPU nutrient not reflected into DAD");
        auto second = cycle.step(activity, energy, pheromone);
        check(db.get_pattern(second.signature)!=nullptr, "second cycle missing attractor");
        check(second.signature != result.signature, "noise evolution unexpectedly static");
        check(db.history_size()==2, "second observation not recorded");
        try {
            const std::array<float,3> invalid{NAN, 0, 0};
            (void)cycle.step(invalid, energy, pheromone);
            throw std::runtime_error("NaN accepted");
        } catch (const std::invalid_argument&) {}
        check(db.history_size()==2, "invalid input changed history");
        const std::string bytes=db.snapshot_bytes();
        auto restored=DynamicAssociativeDatabase::from_snapshot_bytes(bytes);
        check(restored.attractor_count()==db.attractor_count(), "roundtrip lost attractors");
        check(restored.history_size()==db.history_size(), "roundtrip lost history");
        std::cout << "PASS: CC_OpenCL ABI -> nutrient -> observer -> DAD -> snapshot\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
