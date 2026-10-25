#pragma once
#include <cstddef>
#include <string_view>
#include "mycelia.hpp"
namespace krmemory {struct DeleteResult{bool deleted{};std::size_t relationships_deleted{};};DeleteResult delete_memory(mycelia::DynamicAssociativeDatabase&, std::string_view signature);}
