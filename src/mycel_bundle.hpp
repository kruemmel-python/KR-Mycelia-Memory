#pragma once
#include "world_checkpoint.hpp"
#include <filesystem>
#include <string>
#include <string_view>
namespace mycelia {
// MYCL v1 is the original engine's serialized Mycel subset, NOT all VRAM.
struct MycelBundle {
    WorldCheckpoint checkpoint;
    std::string mycl;
};
class MycelBundleCodec final {
public:
    static std::string encode(const MyceliaWorldCpu&, const DynamicAssociativeDatabase&, std::string_view mycl);
    static MycelBundle decode(std::string_view bytes);
    static void save_secure(const std::filesystem::path&, const MyceliaWorldCpu&, const DynamicAssociativeDatabase&, std::string_view mycl);
    static MycelBundle load_secure(const std::filesystem::path&);
};
}
