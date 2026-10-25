#pragma once
#include <filesystem>
#include <vector>
#include "memory_import.hpp"
namespace krmemory {ImportResult collect_changed_file(mycelia::DynamicAssociativeDatabase&, const std::filesystem::path&, const std::vector<std::filesystem::path>& approved_roots);}
