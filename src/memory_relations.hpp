#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "mycelia.hpp"
namespace krmemory {
struct Relation { std::string from,to,type,origin,evidence; double confidence{}; };
std::string relate(mycelia::DynamicAssociativeDatabase&, const Relation&);
std::vector<mycelia::DynamicAssociativeDatabase::ExternalRecord> related(const mycelia::DynamicAssociativeDatabase&, std::string_view signature);
}
