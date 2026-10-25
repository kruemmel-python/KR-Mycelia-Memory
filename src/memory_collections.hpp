#pragma once
#include <string>
#include <vector>
#include "mycelia.hpp"
namespace krmemory {
std::string create_collection(mycelia::DynamicAssociativeDatabase&, std::string name, std::string kind="manual");
std::string assign_to_collection(mycelia::DynamicAssociativeDatabase&, std::string_view memory_signature, std::string_view collection_signature, std::string origin="user");
bool rename_collection(mycelia::DynamicAssociativeDatabase&, std::string_view collection_signature, std::string name);
bool archive_collection(mycelia::DynamicAssociativeDatabase&, std::string_view collection_signature, bool archived=true);
std::size_t merge_collections(mycelia::DynamicAssociativeDatabase&, std::string_view source_signature, std::string_view target_signature);
bool delete_collection(mycelia::DynamicAssociativeDatabase&, std::string_view collection_signature);
std::vector<mycelia::DynamicAssociativeDatabase::ExternalRecord> collections(const mycelia::DynamicAssociativeDatabase&);
std::vector<mycelia::DynamicAssociativeDatabase::ExternalRecord> collection_members(const mycelia::DynamicAssociativeDatabase&, std::string_view collection_signature);
}
