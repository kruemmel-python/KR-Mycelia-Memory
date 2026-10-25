#pragma once
#include <string_view>
#include <vector>
#include "mycelia.hpp"
namespace krmemory {
std::vector<mycelia::DynamicAssociativeDatabase::ExternalRecord> memories_from_source(const mycelia::DynamicAssociativeDatabase&, std::string_view source_path);
}
