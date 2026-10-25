#pragma once
#include "world_checkpoint.hpp"
#include "gpu_bridge.hpp"
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mycelia {
// Captures the observable portion of GPU state. DOES NOT restore hidden VRAM buffers.
struct GpuObservation {
    std::uint64_t simulation_step{};
    std::uint32_t capacity{};
    std::uint32_t active_cells{};
    std::vector<float> activity;
    std::vector<float> nutrient;
};
struct ObservedWorldCheckpoint {
    WorldCheckpoint checkpoint;
    GpuObservation gpu;
};
class GpuObservationCodec final {
public:
    static GpuObservation capture(const GpuBridge& gpu, std::span<const float> activity,
                                  std::uint32_t capacity, std::uint64_t simulation_step);
    static std::string encode(const MyceliaWorldCpu& world,
                              const DynamicAssociativeDatabase& memory,
                              const GpuObservation& gpu);
    static ObservedWorldCheckpoint decode(std::string_view bytes);
    // DPAPI current-user bound, fail-closed on platforms without DPAPI.
    static void save_secure(const std::filesystem::path& path,
                            const MyceliaWorldCpu& world,
                            const DynamicAssociativeDatabase& memory,
                            const GpuObservation& gpu);
    static ObservedWorldCheckpoint load_secure(const std::filesystem::path& path);
};
}
