#include "runtime_cycle.hpp"
#include <cmath>
#include <stdexcept>
#include <vector>

namespace mycelia {
namespace {
template<class T> void validate_finite(std::span<const T> values) {
    for (const auto value : values)
        if (!std::isfinite(static_cast<double>(value)))
            throw std::invalid_argument("Non-finite world/activity value");
}
}
CycleResult RuntimeCycle::step(std::span<const float> activity,
                               std::span<const double> energy,
                               std::span<const double> pheromone) {
    if (activity.empty() || energy.size() != activity.size() ||
        pheromone.size() != activity.size())
        throw std::invalid_argument("Activity/energy/pheromone lengths must match and be nonempty");
    validate_finite(activity);
    validate_finite(energy);
    validate_finite(pheromone);

    // Update runs the original exported CC_OpenCL mycel step.
    gpu_.update(activity);
    const std::vector<float> nutrient_raw = gpu_.nutrient();
    if (nutrient_raw.size() != activity.size())
        throw std::runtime_error("Native GPU nutrient vector length mismatch");
    validate_finite(std::span<const float>(nutrient_raw));

    WorldFields world{
        std::vector<double>(energy.begin(), energy.end()),
        std::vector<double>(pheromone.begin(), pheromone.end()),
        std::vector<double>(nutrient_raw.begin(), nutrient_raw.end())
    };
    // Equivalent to CognitiveCore.reflect's observer/qualia/observe chain,
    // excluding its quantum oracle and action planning, which remain unported.
    const Observation observation = ObserverNetwork::encode(world.pheromone, world.nutrient);
    const double qualia = (observation.harmony_score - observation.tension_score) * 0.5;
    const std::string signature = memory_.observe(world, observation,
        observation.harmony_score, observation.tension_score, qualia);
    return {signature, observation, qualia, world.nutrient.size()};
}
}
