#include "memory_delete.hpp"
#include "memory_relations.hpp"
namespace krmemory {DeleteResult delete_memory(mycelia::DynamicAssociativeDatabase& db,std::string_view sig){DeleteResult out;for(const auto& r:related(db,sig))if(db.delete_sql_record(r.signature))++out.relationships_deleted;out.deleted=db.delete_sql_record(sig);return out;}}
