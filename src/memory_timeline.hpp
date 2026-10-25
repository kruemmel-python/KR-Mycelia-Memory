#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include "mycelia.hpp"
namespace krmemory {
std::vector<mycelia::DynamicAssociativeDatabase::ExternalRecord> timeline(const mycelia::DynamicAssociativeDatabase&, std::optional<std::int64_t> from={}, std::optional<std::int64_t> to={}, std::size_t limit=100);
}
