#include "memory_sources.hpp"
namespace krmemory {std::vector<mycelia::DynamicAssociativeDatabase::ExternalRecord> memories_from_source(const mycelia::DynamicAssociativeDatabase& db,std::string_view source){mycelia::sql::Row f;f["source_path"]=std::string(source);return db.query_sql_like("imported_memory",f);}}
