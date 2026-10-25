#pragma once
#include <string>
#include <vector>
#include "mycelia.hpp"
namespace krmemory {
struct ChatContext { std::string prompt; std::vector<std::string> source_signatures; };
// Retrieval-only stage of local RAG. Imported records are untrusted data and
// are explicitly labelled as reference material, never executable directives.
ChatContext build_chat_context(mycelia::DynamicAssociativeDatabase&, std::string_view question, std::size_t limit=8);
}
