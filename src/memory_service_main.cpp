#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>
#include <shlobj.h>
#include <atomic>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#include <unordered_map>
#include <bcrypt.h>
#include "agent_audit.hpp"
#include "collector_runtime.hpp"
#include "json_value.hpp"
#include "memory_chat.hpp"
#include "memory_collections.hpp"
#include "memory_delete.hpp"
#include "memory_export.hpp"
#include "memory_import.hpp"
#include "memory_relations.hpp"
#include "memory_sources.hpp"
#include "memory_store.hpp"
#include "memory_service_rpc.hpp"
#include "memory_timeline.hpp"
#include "local_ai.hpp"
#include "process_hardening.hpp"

namespace fs = std::filesystem;
namespace {
struct ProcessHardening { ProcessHardening(){krmemory::apply_process_hardening();} } process_hardening;
constexpr wchar_t pipe_name[] = L"\\\\.\\pipe\\KRMyceliaMemoryService-v1";
constexpr wchar_t service_mutex[] = L"Local\\KRMyceliaMemoryService-v1";
constexpr DWORD max_message = 64 * 1024;
std::atomic_bool running{true};
std::mutex import_jobs_mutex;
struct ImportJob { std::shared_ptr<std::atomic_bool> cancelled; std::string owner; };
std::unordered_map<std::string,ImportJob> import_jobs;
constexpr std::string_view permissions_table="mcp_agent_permissions";
std::int64_t now_epoch(){return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
std::string random_token(){std::array<unsigned char,32> bytes{};if(BCryptGenRandom(nullptr,bytes.data(),ULONG(bytes.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0)throw std::runtime_error("random generation failed");static constexpr char hex[]="0123456789abcdef";std::string out;out.reserve(64);for(auto b:bytes){out+=hex[b>>4];out+=hex[b&15];}return out;}
std::string ensure_local_capability(const fs::path& data){if(auto current=krmemory::service_rpc::local_capability())return *current;const auto secret=random_token();DATA_BLOB source{DWORD(secret.size()),reinterpret_cast<BYTE*>(const_cast<char*>(secret.data()))},encrypted{};if(!CryptProtectData(&source,L"KR Mycelia local RPC capability",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&encrypted))throw std::runtime_error("RPC capability protection failed");const auto path=data/L"rpc.capability.dpapi",temp=data/L"rpc.capability.dpapi.tmp";{std::ofstream out(temp,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(encrypted.pbData),encrypted.cbData);if(!out){LocalFree(encrypted.pbData);throw std::runtime_error("RPC capability write failed");}}LocalFree(encrypted.pbData);if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("RPC capability replace failed");return secret;}

std::string esc(std::string_view s) { std::string out; out.reserve(s.size()+8); for (unsigned char c:s) { if(c=='"'||c=='\\') {out+='\\';out+=char(c);} else if(c=='\n')out+="\\n"; else if(c=='\r')out+="\\r"; else if(c=='\t')out+="\\t"; else if(c<32) out+="?"; else out+=char(c); } return out; }
std::string text(const krmemory::json::Value* v) { if(!v || !v->string()) return {}; return *v->string(); }
std::string field(const krmemory::json::Value& params, std::string_view name) { return text(params.get(name)); }
std::optional<double> number(const krmemory::json::Value* v) { return v&&std::holds_alternative<double>(v->data)?std::optional<double>(std::get<double>(v->data)):std::nullopt; }
bool boolean(const krmemory::json::Value* v,bool fallback=true) { return v&&std::holds_alternative<bool>(v->data)?std::get<bool>(v->data):fallback; }
bool equal_constant(std::string_view a,std::string_view b){if(a.size()!=b.size())return false;unsigned diff{};for(size_t i=0;i<a.size();++i)diff|=unsigned((unsigned char)a[i]^(unsigned char)b[i]);return diff==0;}
bool mcp_authorized(const krmemory::json::Value* auth,const mycelia::DynamicAssociativeDatabase& db,std::string_view required){if(!auth||!auth->is_object()||field(*auth,"mode")!="mcp")return false;auto agent=field(*auth,"agent"),token=field(*auth,"token");if(agent.empty()||token.empty())return false;const auto hash=mycelia::DynamicAssociativeDatabase::sha256(token);for(const auto&r:db.query_sql_like(permissions_table)){auto a=r.data.find("agent"),s=r.data.find("scope"),e=r.data.find("enabled"),h=r.data.find("token_sha256");if(a==r.data.end()||s==r.data.end()||e==r.data.end()||h==r.data.end())continue;if(mycelia::sql::display(a->second)!=agent||mycelia::sql::display(s->second)!=required||mycelia::sql::display(e->second)!="True"||!equal_constant(mycelia::sql::display(h->second),hash))continue;auto expiry=r.data.find("expires_timestamp");if(expiry!=r.data.end()&&std::get_if<std::int64_t>(&expiry->second)&&*std::get_if<std::int64_t>(&expiry->second)<now_epoch())continue;return true;}return false;}
bool local_authorized(const krmemory::json::Value* auth,std::string_view secret){return auth&&auth->is_object()&&field(*auth,"mode")=="local"&&equal_constant(field(*auth,"token"),secret);}
std::string reply(std::string_view id, bool ok, std::string_view payload = {}) { return "{\"version\":1,\"id\":\""+esc(id)+"\",\"ok\":"+(ok?"true":"false")+(payload.empty()?"":","+std::string(payload))+"}"; }
std::string failure(std::string_view id, std::string_view code) { return reply(id,false,"\"error\":\""+esc(code)+"\""); }

bool summary_field(std::string_view key){return key=="title"||key=="question"||key=="source_path"||key=="collection"||key=="application"||key=="timestamp"||key=="model";}
std::string record_json(const mycelia::DynamicAssociativeDatabase::ExternalRecord& r,bool summary=false) {
    std::string out="{\"signature\":\""+esc(r.signature)+"\",\"table\":\""+esc(r.table)+"\",\"stability\":"+std::to_string(r.stability)+",\"visits\":"+std::to_string(r.visits)+",\"fields\":{";
    bool first=true; for(const auto& [k,v]:r.data) {if(summary&&!summary_field(k))continue;if(!first)out+=',';first=false;out+="\""+esc(k)+"\":\""+esc(mycelia::sql::display(v))+"\"";} return out+"}}";
}
std::string records_json(const std::vector<mycelia::DynamicAssociativeDatabase::ExternalRecord>& rows,bool summary=false) { std::string out="\"records\":["; for(size_t i=0;i<rows.size();++i){if(i)out+=',';out+=record_json(rows[i],summary);}return out+"]"; }

bool same_user(HANDLE pipe) {
    ULONG pid{}; if(!GetNamedPipeClientProcessId(pipe,&pid)) return false;
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid); if(!process) return false;
    HANDLE client{}, self{}; bool answer=false;
    if(OpenProcessToken(process,TOKEN_QUERY,&client) && OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&self)) {
        DWORD a{},b{}; GetTokenInformation(client,TokenUser,nullptr,0,&a); GetTokenInformation(self,TokenUser,nullptr,0,&b);
        std::vector<std::byte> ca(a), cb(b);
        if(a&&b&&GetTokenInformation(client,TokenUser,ca.data(),a,&a)&&GetTokenInformation(self,TokenUser,cb.data(),b,&b)) answer=EqualSid(reinterpret_cast<TOKEN_USER*>(ca.data())->User.Sid,reinterpret_cast<TOKEN_USER*>(cb.data())->User.Sid)!=FALSE;
    }
    if(client)CloseHandle(client);if(self)CloseHandle(self);CloseHandle(process);return answer;
}
void write_reply(HANDLE pipe,const std::string& body){DWORD n{};WriteFile(pipe,body.data(),static_cast<DWORD>(body.size()),&n,nullptr);FlushFileBuffers(pipe);}

fs::path utf8_path(std::string_view s){if(s.empty())return{};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0);if(n<=0)return{};std::wstring w(size_t(n),L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),w.data(),n);return fs::path(w);}
struct Settings{bool capture{};std::vector<fs::path> roots;};
Settings load_settings(const fs::path& file){Settings x;std::ifstream in(file,std::ios::binary);std::string ver,line;if(!in||!std::getline(in,ver))return x;if(ver=="2"||ver=="3"){std::getline(in,line);x.capture=line=="1";std::getline(in,line);}if(ver!="3")return x;while(std::getline(in,line))if(line.rfind("folder:",0)==0){auto p=utf8_path(line.substr(7));if(fs::is_directory(p)&&x.roots.size()<16)x.roots.push_back(p);}return x;}
bool same_settings(const Settings&a,const Settings&b){return a.capture==b.capture&&a.roots==b.roots;}

std::string dispatch(const fs::path& snapshot,std::string_view local_secret,const std::string& raw) {
    std::string id;
    try {
        if(raw.size()>max_message)return failure(id,"request_too_large");
        auto request=krmemory::json::parse(raw); if(!request.is_object())return failure(id,"invalid_request");
        id=text(request.get("id")); const auto op=text(request.get("operation")); const auto ver=number(request.get("version"));
        const auto* params=request.get("params"); if(!ver||*ver!=1||id.empty()||id.size()>128||op.empty()||op.size()>128||!params||!params->is_object())return failure(id,"invalid_request");
        if(op=="service.health") return reply(id,true,"\"service\":\"KRMyceliaMemory\",\"protocol\":1");
        krmemory::MemoryStore store(snapshot);const auto* auth=request.get("auth");const bool local=local_authorized(auth,local_secret);
        auto scope_for=[&]()->std::string_view{if(op.rfind("memory.import",0)==0)return"import";if(op=="memory.export")return"export";if(op=="memory.delete"||op=="memory.collections.delete")return"delete";if(op=="memory.chat.complete")return"ai";if(op=="memory.remember"||op=="memory.chat.save"||op=="memory.relations.create"||op=="memory.collections.create"||op=="memory.collections.update"||op=="memory.collections.merge"||op=="memory.collections.archive"||op=="memory.collections.assign"||op=="memory.collections.remove")return"write";return"read";};
        const bool local_only=op=="service.shutdown"||op.rfind("memory.agents.",0)==0;
        if(local_only?!local:(!local&&!mcp_authorized(auth,store.read(),scope_for()))){try{auto denied_agent=auth&&auth->is_object()?field(*auth,"agent"):std::string{};if(!denied_agent.empty())store.transaction([&](auto&db){krmemory::audit_agent_event(db,denied_agent,op,"service authorization denied",false);});}catch(...){}return failure(id,"unauthorized");}
        if(op=="service.shutdown"){running=false;return reply(id,true,"\"stopping\":true");}
        auto require_mcp=[&](std::string_view scope){return local||mcp_authorized(auth,store.read(),scope);};
        if(op=="memory.status") {auto db=store.read();return reply(id,true,"\"records\":"+std::to_string(db.external_record_count())+",\"attractors\":"+std::to_string(db.attractor_count())+",\"collections\":"+std::to_string(krmemory::collections(db).size())+",\"relations\":"+std::to_string(db.query_sql_like("memory_relations").size())+",\"agents\":"+std::to_string(db.query_sql_like(permissions_table).size()));}
        if(op=="memory.remember") {auto title=field(*params,"text");auto app=field(*params,"application"),table=field(*params,"table");if(title.empty()||title.size()>8192)return failure(id,"invalid_memory_text");if(app.empty())app="user";if(table.empty())table=app.rfind("mcp:",0)==0?"agent_memory":"manual_memory";if(app.rfind("mcp:",0)==0&&!require_mcp("write"))return failure(id,"unauthorized");if(app.size()>128||(table!="manual_memory"&&table!="agent_memory"&&table!="windows_context"&&table!="agent_registry"&&table!="search_history"))return failure(id,"invalid_memory_metadata");std::string sig;store.transaction([&](auto&db){mycelia::sql::Row row;row["application"]=app;row["title"]=title;row["timestamp"]=(std::int64_t)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();sig=db.store_sql_record(table,row);});return reply(id,true,"\"signature\":\""+esc(sig)+"\"");}
        if(op=="memory.get") {auto sig=field(*params,"signature");if(sig.empty())return failure(id,"invalid_signature");auto db=store.read();auto r=db.get_sql_record(sig);return r?reply(id,true,"\"record\":"+record_json(*r)):failure(id,"not_found");}
        if(op=="memory.list"||op=="memory.search") {auto db=store.read();auto query=field(*params,"query"),table=field(*params,"table"),source=field(*params,"source"),collection=field(*params,"collection"),application=field(*params,"application");auto limit=number(params->get("limit")).value_or(100);auto from=number(params->get("from"));auto to=number(params->get("to"));bool summary=boolean(params->get("summary"),false);if(limit<1||limit>500)return failure(id,"invalid_limit");std::vector<mycelia::DynamicAssociativeDatabase::ExternalRecord> candidates=op=="memory.search"&&!query.empty()?db.associative_sql_lookup(query,1.0,500):db.query_sql_like({}, {},500),rows;for(auto& row:candidates){if(!table.empty()&&row.table!=table)continue;auto matches=[&](std::string_view key,std::string_view expected){if(expected.empty())return true;auto value=row.data.find(std::string(key));return value!=row.data.end()&&mycelia::sql::display(value->second).find(expected)!=std::string::npos;};if(!matches("source_path",source)||!matches("collection",collection)||!matches("application",application))continue;auto timestamp=row.data.find("timestamp");double stamp{};if(timestamp!=row.data.end())try{stamp=std::stod(mycelia::sql::display(timestamp->second));}catch(...){stamp=0;}if((from&&stamp<*from)||(to&&stamp>*to))continue;rows.push_back(std::move(row));if(rows.size()>=size_t(limit))break;}return reply(id,true,records_json(rows,summary));}
        if(op=="memory.delete") {if(auth&&!require_mcp("delete"))return failure(id,"unauthorized");auto sig=field(*params,"signature");if(sig.empty())return failure(id,"invalid_signature");krmemory::DeleteResult result;store.transaction([&](auto&db){result=krmemory::delete_memory(db,sig);});return reply(id,true,std::string("\"deleted\":")+(result.deleted?"true":"false")+",\"relations_deleted\":"+std::to_string(result.relationships_deleted));}
        const auto caller = local ? std::string("<local>") : field(*auth,"agent");
        if(op=="memory.import.cancel"){auto job=field(*params,"job_id");if(job.empty()||job.size()>128)return failure(id,"invalid_job_id");bool found{};{std::lock_guard lock(import_jobs_mutex);auto it=import_jobs.find(job);if(it!=import_jobs.end()&&(local||it->second.owner==caller)){it->second.cancelled->store(true);found=true;}}return reply(id,true,std::string("\"cancel_requested\":")+(found?"true":"false"));}
        if(op=="memory.import") {if(auth&&!require_mcp("import"))return failure(id,"unauthorized");auto source=utf8_path(field(*params,"path"));if(source.empty()||!fs::is_regular_file(source))return failure(id,"invalid_import_path");auto job=field(*params,"job_id");if(job.empty())job=id;if(job.size()>128)return failure(id,"invalid_job_id");auto cancelled=std::make_shared<std::atomic_bool>(false);{std::lock_guard lock(import_jobs_mutex);if(import_jobs.contains(job))return failure(id,"job_already_running");import_jobs.emplace(job,ImportJob{cancelled,caller});}krmemory::ImportResult result;try{store.transaction([&](auto&db){result=krmemory::import_file(db,source,cancelled.get());});}catch(const std::exception& error){{std::lock_guard lock(import_jobs_mutex);import_jobs.erase(job);}if(cancelled->load()||std::string_view(error.what())=="import_cancelled")return failure(id,"cancelled");throw;}{std::lock_guard lock(import_jobs_mutex);import_jobs.erase(job);}return reply(id,true,"\"signature\":\""+esc(result.signature)+"\",\"collection\":\""+esc(result.collection)+"\"");}
        if(op=="memory.export") {if(!require_mcp("export"))return failure(id,"unauthorized");auto destination=utf8_path(field(*params,"path"));if(destination.empty()||!destination.is_absolute())return failure(id,"invalid_export_path");auto table=field(*params,"table");auto db=store.read();auto count=krmemory::export_memories_json(db,destination,table.empty()?std::optional<std::string_view>{}:std::optional<std::string_view>{table});return reply(id,true,"\"exported\":"+std::to_string(count));}
        if(op=="memory.collections.list") {auto db=store.read();return reply(id,true,records_json(krmemory::collections(db)));}
        if(op=="memory.collections.create") {if(auth&&!require_mcp("write"))return failure(id,"unauthorized");auto name=field(*params,"name");if(name.empty()||name.size()>256)return failure(id,"invalid_collection_name");auto kind=field(*params,"kind");std::string sig;store.transaction([&](auto&db){sig=krmemory::create_collection(db,name,kind.empty()?"manual":kind);});return reply(id,true,"\"signature\":\""+esc(sig)+"\"");}
        if(op=="memory.collections.update") {if(auth&&!require_mcp("write"))return failure(id,"unauthorized");auto sig=field(*params,"signature"),name=field(*params,"name");if(sig.empty()||name.empty())return failure(id,"invalid_collection");bool ok{};store.transaction([&](auto&db){ok=krmemory::rename_collection(db,sig,name);});return reply(id,true,std::string("\"updated\":")+(ok?"true":"false"));}
        if(op=="memory.collections.delete") {if(auth&&!require_mcp("delete"))return failure(id,"unauthorized");auto sig=field(*params,"signature");if(sig.empty())return failure(id,"invalid_collection");bool ok{};store.transaction([&](auto&db){ok=krmemory::delete_collection(db,sig);});return reply(id,true,std::string("\"deleted\":")+(ok?"true":"false"));}
        if(op=="memory.collections.merge") {if(auth&&!require_mcp("write"))return failure(id,"unauthorized");auto source=field(*params,"source"),target=field(*params,"target");if(source.empty()||target.empty())return failure(id,"invalid_collection");std::size_t moved{};store.transaction([&](auto&db){moved=krmemory::merge_collections(db,source,target);});return reply(id,true,"\"moved\":"+std::to_string(moved));}
        if(op=="memory.collections.archive") {auto sig=field(*params,"signature");if(sig.empty())return failure(id,"invalid_collection");bool ok{};store.transaction([&](auto&db){ok=krmemory::archive_collection(db,sig,boolean(params->get("archived"),true));});return reply(id,true,std::string("\"updated\":")+(ok?"true":"false"));}
        if(op=="memory.collections.members") {auto sig=field(*params,"signature");if(sig.empty())return failure(id,"invalid_collection");auto db=store.read();return reply(id,true,records_json(krmemory::collection_members(db,sig)));}
        if(op=="memory.collections.assign") {auto memory=field(*params,"memory"),collection=field(*params,"collection");if(memory.empty()||collection.empty())return failure(id,"invalid_assignment");std::string sig;store.transaction([&](auto&db){sig=krmemory::assign_to_collection(db,memory,collection,"user");});return reply(id,true,"\"signature\":\""+esc(sig)+"\"");}
        if(op=="memory.collections.remove") {auto relation=field(*params,"relation");if(relation.empty())return failure(id,"invalid_assignment");bool removed{};store.transaction([&](auto&db){auto r=db.get_sql_record(relation);removed=r&&r->table=="memory_relationships"&&db.delete_sql_record(relation);});return reply(id,true,std::string("\"removed\":")+(removed?"true":"false"));}
        if(op=="memory.relations.list") {auto sig=field(*params,"signature");auto db=store.read();return reply(id,true,records_json(sig.empty()?db.query_sql_like("memory_relations",{},500):krmemory::related(db,sig)));}
        if(op=="memory.relations.create") {if(auth&&!require_mcp("write"))return failure(id,"unauthorized");krmemory::Relation r{field(*params,"from"),field(*params,"to"),field(*params,"type"),field(*params,"origin"),field(*params,"evidence"),number(params->get("confidence")).value_or(-1)};std::string sig;store.transaction([&](auto&db){sig=krmemory::relate(db,r);});return reply(id,true,"\"signature\":\""+esc(sig)+"\"");}
        if(op=="memory.timeline") {auto db=store.read();return reply(id,true,records_json(krmemory::timeline(db,{}, {},size_t(number(params->get("limit")).value_or(100)))));}
        if(op=="memory.sources") {auto source=field(*params,"path");if(source.empty())return failure(id,"invalid_source");auto db=store.read();return reply(id,true,records_json(krmemory::memories_from_source(db,source)));}
        if(op=="memory.agents.list") {auto db=store.read();return reply(id,true,records_json(db.query_sql_like(permissions_table,{},size_t(number(params->get("limit")).value_or(100)))));}
        if(op=="memory.agents.audit.list") {auto agent=field(*params,"agent");auto db=store.read();return reply(id,true,records_json(agent.empty()?db.query_sql_like("agent_audit",{},size_t(number(params->get("limit")).value_or(200))):krmemory::agent_audit(db,agent,size_t(number(params->get("limit")).value_or(200)))));}
        if(op=="memory.agents.audit") {auto agent=field(*params,"agent"),action=field(*params,"action"),detail=field(*params,"detail");if(agent.empty()||action.empty()||agent.size()>128||action.size()>128||detail.size()>8192)return failure(id,"invalid_audit_event");bool success=boolean(params->get("success"));store.transaction([&](auto&db){krmemory::audit_agent_event(db,agent,action,detail,success);});return reply(id,true);}
        if(op=="memory.agents.revoke") {auto agent=field(*params,"agent");if(agent.empty()||agent.size()>128)return failure(id,"invalid_agent");std::size_t changed{};store.transaction([&](auto&db){for(const auto&r:db.query_sql_like(permissions_table)){auto a=r.data.find("agent");if(a==r.data.end()||mycelia::sql::display(a->second)!=agent)continue;auto row=r.data;row["enabled"]=false;row["revoked_timestamp"]=now_epoch();if(db.update_sql_record(r.signature,row))++changed;}krmemory::audit_agent_event(db,agent,"revoke","all tokens revoked",true);});return reply(id,true,"\"revoked\":"+std::to_string(changed));}
        if(op=="memory.agents.rotate") {auto agent=field(*params,"agent"),scopes=field(*params,"scopes");const auto expires=number(params->get("expires_seconds"));if(agent.empty()||agent.size()>128||scopes.empty()||(expires&&(*expires<1||*expires>365.0*24*60*60)))return failure(id,"invalid_agent_or_scopes");const auto token=random_token();std::size_t count{};store.transaction([&](auto&db){for(const auto&r:db.query_sql_like(permissions_table)){auto a=r.data.find("agent");if(a==r.data.end()||mycelia::sql::display(a->second)!=agent)continue;auto row=r.data;row["enabled"]=false;row["revoked_timestamp"]=now_epoch();db.update_sql_record(r.signature,row);}for(size_t begin=0;begin<=scopes.size();){auto end=scopes.find(',',begin);auto scope=scopes.substr(begin,end==std::string::npos?scopes.size()-begin:end-begin);if(scope!="read"&&scope!="write"&&scope!="import"&&scope!="delete"&&scope!="export"&&scope!="ai")throw std::invalid_argument("invalid scope");mycelia::sql::Row row;row["agent"]=agent;row["scope"]=scope;row["enabled"]=true;row["token_sha256"]=mycelia::DynamicAssociativeDatabase::sha256(token);row["timestamp"]=now_epoch();if(expires)row["expires_timestamp"]=now_epoch()+std::int64_t(*expires);db.store_sql_record(permissions_table,row);++count;if(end==std::string::npos)break;begin=end+1;}krmemory::audit_agent_event(db,agent,"rotate",scopes,true);});return reply(id,true,"\"token\":\""+token+"\",\"scopes\":"+std::to_string(count));}
        if(op=="memory.chat"||op=="memory.chat.context") {auto q=field(*params,"question");if(q.empty()||q.size()>8192)return failure(id,"invalid_question");auto db=store.read();auto result=krmemory::build_chat_context(db,q,size_t(number(params->get("limit")).value_or(8)));std::string refs="\"sources\":[";for(size_t i=0;i<result.source_signatures.size();++i){if(i)refs+=',';refs+="\""+esc(result.source_signatures[i])+"\"";}return reply(id,true,"\"prompt\":\""+esc(result.prompt)+"\","+refs+"]");}
        if(op=="memory.chat.models") {auto models=krmemory::list_lm_studio_models();if(!models.success)return failure(id,"local_model_unavailable");std::string data="\"models\":[";for(size_t i=0;i<models.models.size();++i){if(i)data+=',';data+="\""+esc(models.models[i])+"\"";}return reply(id,true,data+"]");}
        if(op=="memory.chat.history") {auto db=store.read();return reply(id,true,records_json(db.query_sql_like("memory_chat_history",{},size_t(number(params->get("limit")).value_or(100)))));}
        if(op=="memory.chat.save") {auto q=field(*params,"question"),answer=field(*params,"answer"),model=field(*params,"model"),sources=field(*params,"sources");if(q.empty()||q.size()>8192||answer.empty()||answer.size()>49152||model.size()>512||sources.size()>8192)return failure(id,"invalid_chat_record");std::string signature;store.transaction([&](auto& database){mycelia::sql::Row row;row["question"]=q;row["answer"]=answer;row["model"]=model;row["sources"]=sources;row["timestamp"]=now_epoch();signature=database.store_sql_record("memory_chat_history",row);});return reply(id,true,"\"signature\":\""+esc(signature)+"\"");}
        if(op=="memory.chat.complete") {auto q=field(*params,"question"),model=field(*params,"model");if(q.empty()||q.size()>8192)return failure(id,"invalid_question");auto timeout=number(params->get("timeout_ms")).value_or(60000);if(timeout<1000||timeout>300000)return failure(id,"invalid_timeout");auto db=store.read();auto context=krmemory::build_chat_context(db,q,size_t(number(params->get("limit")).value_or(8)));auto completion=krmemory::complete_lm_studio(context.prompt,model,"127.0.0.1",1234,unsigned(timeout));if(!completion.success)return failure(id,"local_model_unavailable");std::string refs="\"sources\":[";for(size_t i=0;i<context.source_signatures.size();++i){if(i)refs+=',';refs+="\""+esc(context.source_signatures[i])+"\"";}std::string history;store.transaction([&](auto& database){mycelia::sql::Row row;row["question"]=q;row["answer"]=completion.text;row["model"]=model;row["sources"]=refs.substr(10);row["timestamp"]=now_epoch();history=database.store_sql_record("memory_chat_history",row);});return reply(id,true,"\"answer\":\""+esc(completion.text)+"\",\"history_signature\":\""+esc(history)+"\","+refs+"]");}
        return failure(id,"unknown_operation");
    } catch(const std::exception&) { return failure(id,"operation_failed"); }
}

void serve_client(HANDLE pipe,fs::path snapshot,std::string secret){std::string raw(max_message,'\0');DWORD n{};if(same_user(pipe)&&ReadFile(pipe,raw.data(),DWORD(raw.size()),&n,nullptr)){raw.resize(n);write_reply(pipe,dispatch(snapshot,secret,raw));}else write_reply(pipe,failure({},"unauthorized_or_invalid_request"));FlushFileBuffers(pipe);DisconnectNamedPipe(pipe);CloseHandle(pipe);}
void pipe_server(const fs::path& snapshot,std::string secret){PSECURITY_DESCRIPTOR descriptor{};SECURITY_ATTRIBUTES security{sizeof(security)};if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;GA;;;OW)",SDDL_REVISION_1,&descriptor,nullptr))return;security.lpSecurityDescriptor=descriptor;while(running){HANDLE pipe=CreateNamedPipeW(pipe_name,PIPE_ACCESS_DUPLEX,PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT,PIPE_UNLIMITED_INSTANCES,max_message,max_message,0,&security);if(pipe==INVALID_HANDLE_VALUE)break;BOOL connected=ConnectNamedPipe(pipe,nullptr)?TRUE:GetLastError()==ERROR_PIPE_CONNECTED;if(connected)std::thread(serve_client,pipe,snapshot,secret).detach();else CloseHandle(pipe);}LocalFree(descriptor);}
}
int main(){HANDLE instance=CreateMutexW(nullptr,TRUE,service_mutex);if(!instance)return 1;if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(instance);return 0;}try{wchar_t* local{};if(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local)!=S_OK)throw std::runtime_error("LocalAppData inaccessible");fs::path data=fs::path(local)/L"KRMyceliaNativeMemory";CoTaskMemFree(local);fs::create_directories(data);const auto snapshot=data/L"memory.dpapi",preferences=data/L"settings.txt";const auto capability=ensure_local_capability(data);krmemory::MemoryStore store(snapshot);std::thread server(pipe_server,snapshot,capability);Settings active;std::unique_ptr<krmemory::CollectorRuntime> collector;while(running){auto requested=load_settings(preferences);if(!same_settings(active,requested)){if(collector){collector->stop();collector.reset();}active=std::move(requested);if(active.capture&&!active.roots.empty())try{collector=std::make_unique<krmemory::CollectorRuntime>(store,active.roots);collector->start();}catch(...){collector.reset();}}std::this_thread::sleep_for(std::chrono::seconds(1));}if(collector)collector->stop();if(server.joinable()){HANDLE wake=CreateFileW(pipe_name,GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);if(wake!=INVALID_HANDLE_VALUE){DWORD n{};WriteFile(wake,"{}",2,&n,nullptr);CloseHandle(wake);}server.join();}CloseHandle(instance);return 0;}catch(...){CloseHandle(instance);return 1;}}
#else
int main(){return 1;}
#endif
