#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
namespace mycelia {
struct MyclComparison {
    bool identical{};
    std::size_t bytes{};
    std::size_t differences{};
    std::size_t first_difference{};
    std::uint64_t left_fnv64{};
    std::uint64_t right_fnv64{};
};
MyclComparison compare_mycl(const std::filesystem::path& reference,
                            const std::filesystem::path& resumed);
}
