#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include "mycelia.hpp"
namespace krmemory {
// Explicit user/agent export. The output is intentionally plaintext JSON and
// must only be written to a path chosen by an authorized caller.
std::size_t export_memories_json(const mycelia::DynamicAssociativeDatabase&, const std::filesystem::path&, std::optional<std::string_view> table={});
}
