#include "memory_delete.hpp"
#include "memory_relations.hpp"
namespace krmemory {DeleteResult delete_memory(mycelia::DynamicAssociativeDatabase& db,std::string_view sig){DeleteResult out;
for(const auto& child:db.query_sql_like("memory_media_segments")){auto source=child.data.find("source_signature");if(source!=child.data.end()&&mycelia::sql::display(source->second)==sig){auto removed=delete_memory(db,child.signature);out.relationships_deleted+=removed.relationships_deleted;}}
for(const auto& r:related(db,sig))if(db.delete_sql_record(r.signature))++out.relationships_deleted;
for(const auto& membership:db.query_sql_like("memory_relationships")){auto memory=membership.data.find("memory_signature");if(memory!=membership.data.end()&&mycelia::sql::display(memory->second)==sig&&db.delete_sql_record(membership.signature))++out.relationships_deleted;}
out.deleted=db.delete_sql_record(sig);return out;}}
