#pragma once
#include <filesystem>
#include <atomic>
#include <string>
#include <vector>
#include "mycelia.hpp"

namespace krmemory {
struct ImportResult { std::string signature, collection; std::filesystem::path source; std::string format; std::size_t bytes{}; };
bool import_format_supported(const std::filesystem::path&);
// Automatic folder monitoring is deliberately limited to deterministic,
// inexpensive text/source formats. Binary Office, PDF and media analysis is
// available through explicit import, but never launched by a recursive scan.
bool collector_format_supported(const std::filesystem::path&);
std::string normalized_source_key(const std::filesystem::path&);
// Imports only formats that can be parsed safely with the native runtime.
// Binary office/media formats remain visible to callers as unsupported rather
// than being misrepresented as extracted content.
ImportResult import_file(mycelia::DynamicAssociativeDatabase&, const std::filesystem::path&,const std::atomic_bool* cancel=nullptr);
std::vector<mycelia::DynamicAssociativeDatabase::ExternalRecord> list_collection(const mycelia::DynamicAssociativeDatabase&, std::string_view collection);
}
