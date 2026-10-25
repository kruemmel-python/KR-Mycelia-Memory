#include "agent_audit.hpp"
#include <chrono>
#include <stdexcept>
namespace krmemory { std::string audit_agent_event(mycelia::DynamicAssociativeDatabase& db,std::string_view agent,std::string_view action,std::string_view detail,bool success){if(agent.empty()||action.empty()||agent.size()>128||detail.size()>2048)throw std::invalid_argument("invalid agent audit event");mycelia::sql::Row row;row["agent"]=std::string(agent);row["action"]=std::string(action);row["detail"]=std::string(detail);row["success"]=success;row["timestamp"]=(std::int64_t)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();return db.store_sql_record("agent_audit",row);}
std::vector<mycelia::DynamicAssociativeDatabase::ExternalRecord> agent_audit(const mycelia::DynamicAssociativeDatabase& db,std::string_view agent,std::size_t limit){mycelia::sql::Row f;f["agent"]=std::string(agent);return db.query_sql_like("agent_audit",f,limit);}}
