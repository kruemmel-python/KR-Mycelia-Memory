#pragma once
#include "gpu_bridge.hpp"
#include "mycelia.hpp"
#include <cstddef>
#include <span>
#include <string>

namespace mycelia {
// Bounded integration layer. The caller supplies original world energy and
// pheromone fields; the original CC_OpenCL DLL supplies the nutrient field.
// This does not substitute for the not-yet-ported MyceliaWorld simulation.
struct CycleResult {
    std::string signature;
    Observation observation;
    double qualia{};
    std::size_t observed_cells{};
};
class RuntimeCycle final {
public:
    RuntimeCycle(GpuBridge& gpu, DynamicAssociativeDatabase& memory)
        : gpu_(gpu), memory_(memory) {}
    [[nodiscard]] CycleResult step(std::span<const float> activity,
                                   std::span<const double> energy,
                                   std::span<const double> pheromone);
private:
    GpuBridge& gpu_;
    DynamicAssociativeDatabase& memory_;
};
}
