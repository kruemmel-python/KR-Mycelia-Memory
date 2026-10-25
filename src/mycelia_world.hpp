#pragma once
#include "mycelia.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace mycelia {
// CPU branch and field transformation of the ORIGINAL MyceliaWorld.
// GPU physics/reinforcement/reproduction methods are intentionally NOT faked.
struct WorldFrame {
    std::uint64_t step{};
    WorldFields fields;
    std::vector<float> activity;
};
class MyceliaWorldCpu final {
public:
    MyceliaWorldCpu(std::size_t width, std::size_t height, std::size_t depth,
                    std::size_t neighbor_degree, double time_step);
    [[nodiscard]] const WorldFrame& frame() const noexcept { return frame_; }
    [[nodiscard]] const std::vector<int>& neighbors() const noexcept { return neighbors_; }
    [[nodiscard]] WorldFrame step();
    [[nodiscard]] std::vector<float> derive_activity(std::span<const double> energy) const;
    void inject_agents(std::span<const std::array<double, 4>> descriptors);
    [[nodiscard]] std::string serialize() const;
    static MyceliaWorldCpu deserialize(std::string_view bytes);
private:
    std::size_t width_,height_,depth_,degree_;
    double dt_,phase_{};
    std::array<double,3> fallback_{0.5,0.5,0.5};
    WorldFrame frame_;
    std::vector<int> neighbors_;
    void refresh_fields();
};
}
