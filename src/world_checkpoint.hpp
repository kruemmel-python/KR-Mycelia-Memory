#pragma once
#include "mycelia_world.hpp"
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace mycelia {
// An atomic logical checkpoint of the CPU world and the associative database.
// This does not claim to serialize the GPU's VRAM / OpenCL handles.
struct WorldCheckpoint {
    MyceliaWorldCpu world;
    DynamicAssociativeDatabase memory;
};
class WorldCheckpointCodec final {
public:
    [[nodiscard]] static std::string encode(const MyceliaWorldCpu& world,
                                            const DynamicAssociativeDatabase& memory);
    [[nodiscard]] static WorldCheckpoint decode(std::string_view serialized);
    // Windows current-user DPAPI only. Never silently falls back to plaintext.
    static void save_secure(const std::filesystem::path& path,
                            const MyceliaWorldCpu& world,
                            const DynamicAssociativeDatabase& memory);
    [[nodiscard]] static WorldCheckpoint load_secure(const std::filesystem::path& path);
};
}
