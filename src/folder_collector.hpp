#pragma once
#include <atomic>
#include <filesystem>
#include <vector>
#include "memory_import.hpp"
namespace krmemory {
struct FolderReconcileResult {std::size_t imported{},removed{},failed{};bool truncated{};};
ImportResult collect_changed_file(mycelia::DynamicAssociativeDatabase&, const std::filesystem::path&, const std::vector<std::filesystem::path>& approved_roots,const std::atomic_bool* cancel=nullptr);
std::size_t remove_collected_source(mycelia::DynamicAssociativeDatabase&,const std::filesystem::path&);
FolderReconcileResult reconcile_collected_root(mycelia::DynamicAssociativeDatabase&,const std::filesystem::path&,const std::vector<std::filesystem::path>& approved_roots,std::size_t maximum_files=20000,const std::atomic_bool* cancel=nullptr);
}
