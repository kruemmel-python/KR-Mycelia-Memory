#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "mycelia.hpp"
namespace krmemory {
std::string audit_agent_event(mycelia::DynamicAssociativeDatabase&, std::string_view agent, std::string_view action, std::string_view detail, bool success);
std::vector<mycelia::DynamicAssociativeDatabase::ExternalRecord> agent_audit(const mycelia::DynamicAssociativeDatabase&, std::string_view agent, std::size_t limit=100);
}
