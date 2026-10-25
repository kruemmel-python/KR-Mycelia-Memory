#include "memory_relations.hpp"
#include <chrono>
#include <algorithm>
#include <stdexcept>
namespace krmemory { namespace {bool valid(std::string_view t){for(auto x:{"related_to","derived_from","supersedes","contradicts","caused_by","resolved_by","belongs_to","verified_by"})if(t==x)return true;return false;}}
std::string relate(mycelia::DynamicAssociativeDatabase& db,const Relation& r){if(!db.get_sql_record(r.from)||!db.get_sql_record(r.to)||r.from==r.to||!valid(r.type)||r.origin.empty()||r.confidence<0||r.confidence>1)throw std::invalid_argument("invalid memory relation");mycelia::sql::Row row;row["from_signature"]=r.from;row["to_signature"]=r.to;row["relation_type"]=r.type;row["origin"]=r.origin;row["evidence"]=r.evidence;row["confidence"]=r.confidence;row["timestamp"]=(std::int64_t)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();return db.store_sql_record("memory_relations",row);}
std::vector<mycelia::DynamicAssociativeDatabase::ExternalRecord> related(const mycelia::DynamicAssociativeDatabase& db,std::string_view signature){std::vector<mycelia::DynamicAssociativeDatabase::ExternalRecord> out;for(auto& r:db.query_sql_like("memory_relations")){auto a=r.data.find("from_signature"),b=r.data.find("to_signature");if((a!=r.data.end()&&mycelia::sql::display(a->second)==signature)||(b!=r.data.end()&&mycelia::sql::display(b->second)==signature))out.push_back(r);}return out;}
}
