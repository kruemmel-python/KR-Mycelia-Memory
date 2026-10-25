#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <shlobj.h>
#include <wincrypt.h>
#include <array>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "agent_audit.hpp"
#include "json_value.hpp"
#include "memory_collections.hpp"
#include "memory_delete.hpp"
#include "memory_export.hpp"
#include "memory_import.hpp"
#include "memory_chat.hpp"
#include "memory_relations.hpp"
#include "memory_sources.hpp"
#include "memory_store.hpp"
#include "memory_service_rpc.hpp"
#include "process_hardening.hpp"
#include "memory_timeline.hpp"
#include "local_ai.hpp"
#include "context_policy.hpp"

// Local stdio MCP server. No network listener is opened; the hosting MCP
// client must start this executable locally with an agent id and token.
namespace fs = std::filesystem;
namespace {
constexpr std::string_view permissions_table = "mcp_agent_permissions";
fs::path memory_file;
thread_local std::string forwarded_agent, forwarded_token;

fs::path utf8_path(std::string_view value){if(value.empty())return{};int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);if(count<=0)throw std::invalid_argument("credential path is not valid UTF-8");std::wstring wide(std::size_t(count),L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),wide.data(),count);return fs::path(std::move(wide));}

std::string read_protected_credential(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::string encrypted((std::istreambuf_iterator<char>(input)), {});
    if (encrypted.empty() || encrypted.size() > 4096) throw std::runtime_error("invalid protected credential file");
    DATA_BLOB source{DWORD(encrypted.size()), reinterpret_cast<BYTE*>(encrypted.data())}, plain{};
    if (!CryptUnprotectData(&source, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &plain))
        throw std::runtime_error("protected credential belongs to another Windows user or is damaged");
    std::string token(reinterpret_cast<char*>(plain.pbData), plain.cbData);
    SecureZeroMemory(plain.pbData, plain.cbData); LocalFree(plain.pbData);
    if (token.size() != 64) throw std::runtime_error("invalid protected credential payload");
    return token;
}

std::string json_escape(std::string_view input) {
    std::string out;
    for (unsigned char c : input) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: if (c < 32) out += ' '; else out += static_cast<char>(c);
        }
    }
    return out;
}

std::string json_quote(std::string_view text) { return "\"" + json_escape(text) + "\""; }

const krmemory::json::Value* find_value(const krmemory::json::Value& value, std::string_view key) {
    if (auto direct = value.get(key)) return direct;
    for (auto branch : {"params", "arguments"}) {
        if (auto nested = value.get(branch); nested && nested->is_object()) {
            if (auto found = find_value(*nested, key)) return found;
        }
    }
    return nullptr;
}

std::optional<std::string> string_value(const krmemory::json::Value& root, std::string_view key) {
    if (auto value = find_value(root, key); value && value->string()) return *value->string();
    return std::nullopt;
}

std::optional<long long> integer_value(const krmemory::json::Value& root, std::string_view key) {
    if (auto value = find_value(root, key)) {
        if (auto number = std::get_if<double>(&value->data)) return static_cast<long long>(*number);
    }
    return std::nullopt;
}

std::string request_id(const krmemory::json::Value& root) {
    auto id = root.get("id");
    if (!id) return "null";
    if (auto text = id->string()) return json_quote(*text);
    if (auto number = std::get_if<double>(&id->data)) return std::to_string(static_cast<long long>(*number));
    return "null";
}

long long now_epoch() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

fs::path user_memory_file() {
    wchar_t* local{};
    if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local) != S_OK)
        throw std::runtime_error("LocalAppData inaccessible");
    fs::path file = fs::path(local) / L"KRMyceliaNativeMemory" / L"memory.dpapi";
    CoTaskMemFree(local);
    return file;
}

std::string random_token() {
    std::array<unsigned char, 32> bytes{};
    if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        throw std::runtime_error("Windows random source unavailable");
    constexpr char hex[] = "0123456789abcdef";
    std::string token;
    token.reserve(64);
    for (auto byte : bytes) { token += hex[byte >> 4]; token += hex[byte & 15]; }
    return token;
}

bool row_is(const mycelia::sql::Row& row, std::string_view key, std::string_view expected) {
    auto it = row.find(std::string(key));
    return it != row.end() && mycelia::sql::display(it->second) == expected;
}

bool permission_is_current(const mycelia::sql::Row& row) {
    auto expiry = row.find("expires_timestamp");
    if (expiry == row.end()) return true;
    if (auto stamp = std::get_if<std::int64_t>(&expiry->second)) return *stamp >= now_epoch();
    return false;
}

bool constant_equal(std::string_view first, std::string_view second) {
    if (first.size() != second.size()) return false;
    unsigned diff{};
    for (std::size_t i{}; i < first.size(); ++i)
        diff |= static_cast<unsigned>(static_cast<unsigned char>(first[i]) ^ static_cast<unsigned char>(second[i]));
    return diff == 0;
}

bool has_scope(const mycelia::DynamicAssociativeDatabase& db, std::string_view agent,
               std::string_view token, std::string_view scope) {
    const auto hash = mycelia::DynamicAssociativeDatabase::sha256(token);
    for (const auto& permission : db.query_sql_like(permissions_table)) {
        auto stored = permission.data.find("token_sha256");
        if (stored != permission.data.end() && row_is(permission.data, "agent", agent) &&
            row_is(permission.data, "scope", scope) && row_is(permission.data, "enabled", "True") && permission_is_current(permission.data) &&
            constant_equal(mycelia::sql::display(stored->second), hash)) return true;
    }
    return false;
}

void reply(std::string_view id, std::string_view result) {
    std::cout << "{\"jsonrpc\":\"2.0\",\"id\":" << id << ",\"result\":" << result << "}\n" << std::flush;
}

void fail(std::string_view id, int code, std::string_view message) {
    std::cout << "{\"jsonrpc\":\"2.0\",\"id\":" << id << ",\"error\":{\"code\":" << code
              << ",\"message\":" << json_quote(message) << "}}\n" << std::flush;
}

std::string content(std::string_view text) {
    return "{\"content\":[{\"type\":\"text\",\"text\":" + json_quote(text) + "}]}";
}

std::string record_text(const mycelia::DynamicAssociativeDatabase::ExternalRecord& record) {
    std::string title;
    if (auto it = record.data.find("title"); it != record.data.end()) title = mycelia::sql::display(it->second);
    if (title.empty()) if (auto it = record.data.find("action"); it != record.data.end()) title = mycelia::sql::display(it->second);
    if (auto it = record.data.find("detail"); it != record.data.end() && !mycelia::sql::display(it->second).empty()) title += ": " + mycelia::sql::display(it->second);
    return "[" + record.table + "] " + title + " (" + record.signature + ")";
}

std::string render_records(const std::vector<mycelia::DynamicAssociativeDatabase::ExternalRecord>& records) {
    std::string output;
    for (const auto& record : records) {
        if (!output.empty()) output += '\n';
        output += record_text(record);
    }
    return output.empty() ? "No matching memory." : output;
}

std::string render_record_detail(const mycelia::DynamicAssociativeDatabase::ExternalRecord& record) {
    std::string output = record_text(record);
    for (const auto& [key, value] : record.data)
        output += "\n" + key + ": " + mycelia::sql::display(value);
    return output;
}

void audit(std::string_view agent, std::string_view action, std::string_view detail, bool success) {
    std::string ignored;
    krmemory::service_rpc::call("mcp-audit", "memory.agents.audit",
        "{\"agent\":" + krmemory::service_rpc::quote(agent) + ",\"action\":" +
        krmemory::service_rpc::quote(action) + ",\"detail\":" + krmemory::service_rpc::quote(detail) +
        ",\"success\":" + (success ? "true" : "false") + "}", ignored, 1000);
}

bool service_agent_memory(std::string_view agent, std::string_view token, std::string_view text) {
    if (agent.empty() || agent.size() > 128 || token.empty() || text.empty() || text.size() > 3072) return false;
    std::string response;
    return krmemory::service_rpc::call_authenticated("mcp-remember", "memory.remember",
        "{\"text\":" + krmemory::service_rpc::quote(text) + ",\"application\":" +
        krmemory::service_rpc::quote("mcp:" + std::string(agent)) + ",\"table\":\"agent_memory\"}",agent,token,"write",response,1000);
}
bool service_operation(std::string_view operation, std::string params, std::string* response = nullptr) {
    std::string local;
    std::string_view scope="write";
    if(operation=="memory.import")scope="import";
    else if(operation=="memory.export")scope="export";
    else if(operation=="memory.delete"||operation=="memory.collections.delete")scope="delete";
    const bool ok = forwarded_agent.empty()
        ? krmemory::service_rpc::call("mcp-operation", operation, params, local, 30000)
        : krmemory::service_rpc::call_authenticated("mcp-operation",operation,params,forwarded_agent,forwarded_token,scope,local,30000);
    if (response) *response = std::move(local);
    return ok;
}

void grant(std::string_view agent, std::string_view scopes, std::optional<long long> lifetime_seconds = {}) {
    if (agent.empty() || agent.size() > 128) throw std::runtime_error("invalid agent id");
    if (lifetime_seconds && (*lifetime_seconds < 1 || *lifetime_seconds > 365ll * 24 * 60 * 60)) throw std::runtime_error("expiration must be 1 second through 365 days");
    std::string response;
    std::string params="{\"agent\":"+krmemory::service_rpc::quote(agent)+",\"scopes\":"+krmemory::service_rpc::quote(scopes);
    if(lifetime_seconds)params+=",\"expires_seconds\":"+std::to_string(*lifetime_seconds);params+="}";
    if (!service_operation("memory.agents.rotate", std::move(params), &response)) throw std::runtime_error("local memory service unavailable or grant rejected");
    const auto root=krmemory::json::parse(response);const auto value=root.get("token");if(!value||!value->string()||value->string()->empty())throw std::runtime_error("service returned no token");const auto token=*value->string();
    std::cout << "Agent: " << agent << "\nScopes: " << scopes << "\nToken (only shown once): " << token << "\n";
}

void revoke(std::string_view agent) {
    if (!service_operation("memory.agents.revoke", "{\"agent\":"+krmemory::service_rpc::quote(agent)+"}")) throw std::runtime_error("local memory service unavailable or revoke rejected");
    std::cout << "Agent revoked: " << agent << "\n";
}

void rotate(std::string_view agent, std::string_view scopes, std::optional<long long> lifetime_seconds) {
    revoke(agent);
    grant(agent, scopes, lifetime_seconds);
}

bool require_scope(const mycelia::DynamicAssociativeDatabase& db, std::string_view agent,
                   std::string_view token, std::string_view scope, std::string_view id, std::string_view action) {
    if (has_scope(db, agent, token, scope)) return true;
    audit(agent, action, "permission denied", false);
    fail(id, -32001, "agent lacks required permission");
    return false;
}

std::string tool(std::string_view name, std::string_view description, std::string_view properties,
                 std::string_view required = "") {
    std::string value = "{\"name\":" + json_quote(name) + ",\"description\":" + json_quote(description) +
        ",\"inputSchema\":{\"type\":\"object\",\"properties\":" + std::string(properties);
    if (!required.empty()) value += ",\"required\":" + std::string(required);
    return value + "}}";
}

void list_tools(std::string_view id, const mycelia::DynamicAssociativeDatabase& db,
                std::string_view agent, std::string_view token) {
    std::vector<std::string> tools;
    const auto read = has_scope(db, agent, token, "read");
    const auto write = has_scope(db, agent, token, "write");
    if (read) {
        tools.push_back(tool("memory_status", "Read DAD memory status.", "{}"));
        tools.push_back(tool("memory_search", "Search associative local memory.", "{\"query\":{\"type\":\"string\"},\"limit\":{\"type\":\"integer\"}}", "[\"query\"]"));
        tools.push_back(tool("memory_get", "Read one memory record by its source signature.", "{\"signature\":{\"type\":\"string\"}}", "[\"signature\"]"));
        tools.push_back(tool("memory_list", "List records, optionally restricted to one DAD table.", "{\"table\":{\"type\":\"string\"},\"limit\":{\"type\":\"integer\"}}"));
        tools.push_back(tool("memory_timeline", "Read chronologically ordered local activity.", "{\"limit\":{\"type\":\"integer\"}}"));
        tools.push_back(tool("memory_collections", "List local memory collections.", "{}"));
        tools.push_back(tool("memory_collection_search", "Associatively search local memory collections.", "{\"query\":{\"type\":\"string\"},\"limit\":{\"type\":\"integer\"}}", "[\"query\"]"));
        tools.push_back(tool("memory_relations", "List evidence-backed links for a memory.", "{\"signature\":{\"type\":\"string\"}}", "[\"signature\"]"));
        tools.push_back(tool("memory_related", "Alias for memory_relations.", "{\"signature\":{\"type\":\"string\"}}", "[\"signature\"]"));
        tools.push_back(tool("memory_source", "Find memories imported from an exact source path.", "{\"path\":{\"type\":\"string\"}}", "[\"path\"]"));
        tools.push_back(tool("memory_sources", "List imported memory sources, optionally for an exact path.", "{\"path\":{\"type\":\"string\"},\"limit\":{\"type\":\"integer\"}}"));
        tools.push_back(tool("memory_context", "Build safe untrusted retrieval context without calling a model.", "{\"question\":{\"type\":\"string\"},\"limit\":{\"type\":\"integer\"}}", "[\"question\"]"));
        tools.push_back(tool("memory_audit", "Read this agent's own recent access audit entries.", "{\"limit\":{\"type\":\"integer\"}}"));
        tools.push_back(tool("memory_agent_status", "Read this agent's own active permissions and recent accesses.", "{}"));
    }
    if (write) {
        tools.push_back(tool("memory_remember", "Store explicit text as agent memory.", "{\"text\":{\"type\":\"string\"}}", "[\"text\"]"));
        tools.push_back(tool("memory_relate", "Create an evidence-backed relationship.", "{\"from\":{\"type\":\"string\"},\"to\":{\"type\":\"string\"},\"type\":{\"type\":\"string\"},\"evidence\":{\"type\":\"string\"}}", "[\"from\",\"to\",\"type\"]"));
        tools.push_back(tool("memory_collection_create", "Create a named memory collection.", "{\"name\":{\"type\":\"string\"},\"kind\":{\"type\":\"string\"}}", "[\"name\"]"));
        tools.push_back(tool("memory_collection_rename", "Rename a memory collection.", "{\"signature\":{\"type\":\"string\"},\"name\":{\"type\":\"string\"}}", "[\"signature\",\"name\"]"));
        tools.push_back(tool("memory_collection_merge", "Merge collection memberships into another collection.", "{\"source\":{\"type\":\"string\"},\"target\":{\"type\":\"string\"}}", "[\"source\",\"target\"]"));
    }
    if (has_scope(db, agent, token, "import"))
        tools.push_back(tool("memory_import", "Import one explicitly specified local file into memory.", "{\"path\":{\"type\":\"string\"}}", "[\"path\"]"));
    if (has_scope(db, agent, token, "export"))
        tools.push_back(tool("memory_export", "Write an explicit plaintext JSON export to an absolute local path.", "{\"path\":{\"type\":\"string\"},\"table\":{\"type\":\"string\"}}", "[\"path\"]"));
    if (has_scope(db, agent, token, "delete"))
    {   tools.push_back(tool("memory_delete", "Permanently delete a memory and its links.", "{\"signature\":{\"type\":\"string\"}}", "[\"signature\"]"));
        tools.push_back(tool("memory_collection_delete", "Permanently delete a collection and its memberships.", "{\"signature\":{\"type\":\"string\"}}", "[\"signature\"]")); }
    if (has_scope(db, agent, token, "ai"))
        tools.push_back(tool("memory_answer", "Ask the configured local LM Studio model using retrieved local memory.", "{\"question\":{\"type\":\"string\"},\"model\":{\"type\":\"string\"}}", "[\"question\"]"));
    std::string body;
    for (const auto& entry : tools) { if (!body.empty()) body += ','; body += entry; }
    reply(id, "{\"tools\":[" + body + "]}");
}

void serve(std::string agent, std::string token) {
    if (agent.empty() || token.empty()) throw std::runtime_error("--agent and --token (or KR_MYCELIA_MCP_TOKEN) are required");
    forwarded_agent=agent; forwarded_token=token;
    std::string line;
    while (std::getline(std::cin, line)) {
        krmemory::json::Value root;
        try { root = krmemory::json::parse(line); }
        catch (...) { fail("null", -32700, "invalid JSON"); continue; }
        const auto id = request_id(root);
        const auto method = string_value(root, "method");
        if (!method) { fail(id, -32600, "invalid JSON-RPC request"); continue; }
        if (*method == "initialize") { reply(id, "{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"kr-mycelia-memory\",\"version\":\"1.1\"}}"); continue; }
        if (*method == "notifications/initialized") continue;
        auto db = krmemory::MemoryStore(memory_file).read();
        if (*method == "tools/list") { list_tools(id, db, agent, token); continue; }
        if (*method != "tools/call") { fail(id, -32601, "method not found"); continue; }
        const auto name = string_value(root, "name");
        if (!name) { fail(id, -32602, "tool name missing"); continue; }

        try {
            if (*name == "memory_status") {
                if (!require_scope(db, agent, token, "read", id, *name)) continue;
                audit(agent, *name, "status", true);
                reply(id, content("DAD attractors: " + std::to_string(db.attractor_count()) + ", external records: " + std::to_string(db.external_record_count())));
            } else if (*name == "memory_search") {
                if (!require_scope(db, agent, token, "read", id, *name)) continue;
                const auto query = string_value(root, "query");
                if (!query || query->empty()) { fail(id, -32602, "query missing"); continue; }
                const auto limit = std::clamp<long long>(integer_value(root, "limit").value_or(20), 1, 100);
                auto matches = db.associative_sql_lookup(*query, 1.0, static_cast<std::size_t>(limit));
                audit(agent, *name, *query, true);
                reply(id, content(render_records(matches)));
            } else if (*name == "memory_get") {
                if (!require_scope(db, agent, token, "read", id, *name)) continue;
                const auto signature = string_value(root, "signature");
                if (!signature) { fail(id, -32602, "signature missing"); continue; }
                const auto record = db.get_sql_record(*signature);
                audit(agent, *name, *signature, record.has_value());
                reply(id, content(record ? render_record_detail(*record) : "Memory not found."));
            } else if (*name == "memory_list") {
                if (!require_scope(db, agent, token, "read", id, *name)) continue;
                const auto table = string_value(root, "table");
                const auto limit = std::clamp<long long>(integer_value(root, "limit").value_or(50), 1, 100);
                auto records = table ? db.query_sql_like(std::optional<std::string_view>{*table}, {}, static_cast<std::size_t>(limit)) : db.query_sql_like({}, {}, static_cast<std::size_t>(limit));
                audit(agent, *name, table.value_or("all"), true);
                reply(id, content(render_records(records)));
            } else if (*name == "memory_timeline") {
                if (!require_scope(db, agent, token, "read", id, *name)) continue;
                const auto limit = std::clamp<long long>(integer_value(root, "limit").value_or(50), 1, 100);
                auto records = krmemory::timeline(db, {}, {}, static_cast<std::size_t>(limit));
                audit(agent, *name, "timeline", true);
                reply(id, content(render_records(records)));
            } else if (*name == "memory_collections") {
                if (!require_scope(db, agent, token, "read", id, *name)) continue;
                auto records = krmemory::collections(db);
                audit(agent, *name, "collections", true);
                reply(id, content(render_records(records)));
            } else if (*name == "memory_collection_search") {
                if (!require_scope(db, agent, token, "read", id, *name)) continue;
                const auto query = string_value(root, "query");
                if (!query || query->empty()) { fail(id, -32602, "query missing"); continue; }
                const auto limit = std::clamp<long long>(integer_value(root, "limit").value_or(20), 1, 100);
                std::vector<mycelia::DynamicAssociativeDatabase::ExternalRecord> collections;
                for (const auto& record : db.associative_sql_lookup(*query, 1.0, static_cast<std::size_t>(limit)))
                    if (record.table == "memory_collections") collections.push_back(record);
                audit(agent, *name, *query, true);
                reply(id, content(render_records(collections)));
            } else if (*name == "memory_relations" || *name == "memory_related") {
                if (!require_scope(db, agent, token, "read", id, *name)) continue;
                const auto signature = string_value(root, "signature");
                if (!signature) { fail(id, -32602, "signature missing"); continue; }
                auto records = krmemory::related(db, *signature);
                audit(agent, *name, *signature, true);
                reply(id, content(render_records(records)));
            } else if (*name == "memory_source" || *name == "memory_sources") {
                if (!require_scope(db, agent, token, "read", id, *name)) continue;
                const auto path = string_value(root, "path");
                if (*name == "memory_source" && !path) { fail(id, -32602, "path missing"); continue; }
                const auto limit = std::clamp<long long>(integer_value(root, "limit").value_or(100), 1, 100);
                auto records = path ? krmemory::memories_from_source(db, *path) : db.query_sql_like(std::optional<std::string_view>{"imported_memory"}, {}, static_cast<std::size_t>(limit));
                audit(agent, *name, path.value_or("all sources"), true);
                reply(id, content(render_records(records)));
            } else if (*name == "memory_context") {
                if (!require_scope(db, agent, token, "read", id, *name)) continue;
                const auto question = string_value(root, "question");
                if (!question || question->empty() || question->size() > 4096) { fail(id, -32602, "question must be 1..4096 bytes"); continue; }
                const auto limit = std::clamp<long long>(integer_value(root, "limit").value_or(8), 1, 32);
                const auto context = krmemory::build_chat_context(db, *question, static_cast<std::size_t>(limit));
                audit(agent, *name, *question, true);
                reply(id, content(context.prompt));
            } else if (*name == "memory_audit") {
                if (!require_scope(db, agent, token, "read", id, *name)) continue;
                const auto limit = std::clamp<long long>(integer_value(root, "limit").value_or(50), 1, 100);
                auto records = krmemory::agent_audit(db, agent, static_cast<std::size_t>(limit));
                audit(agent, *name, "audit", true);
                reply(id, content(render_records(records)));
            } else if (*name == "memory_agent_status") {
                if (!require_scope(db, agent, token, "read", id, *name)) continue;
                std::string result = "Agent: " + agent + "\nActive permissions:";
                bool any{};
                for (const auto& permission : db.query_sql_like(permissions_table)) {
                    if (!row_is(permission.data, "agent", agent) || !row_is(permission.data, "enabled", "True")) continue;
                    auto scope = permission.data.find("scope");
                    if (scope != permission.data.end()) { result += " " + mycelia::sql::display(scope->second); any = true; }
                }
                if (!any) result += " none";
                auto records = krmemory::agent_audit(db, agent, 5);
                if (!records.empty()) result += "\nRecent access:\n" + render_records(records);
                audit(agent, *name, "status", true);
                reply(id, content(result));
            } else if (*name == "memory_remember") {
                if (!require_scope(db, agent, token, "write", id, *name)) continue;
                const auto text = string_value(root, "text");
                if (!text || text->empty() || text->size() > 4096) { fail(id, -32602, "text must be 1..4096 bytes"); continue; }
                const auto remembered = krmemory::truncate_utf8(*text, 3072);
                if (!service_agent_memory(agent, token, remembered)) { fail(id, -32010, "local memory service unavailable or token rejected"); continue; }
                reply(id, content("Memory stored."));
            } else if (*name == "memory_relate") {
                if (!require_scope(db, agent, token, "write", id, *name)) continue;
                const auto from = string_value(root, "from"); const auto to = string_value(root, "to"); const auto type = string_value(root, "type");
                if (!from || !to || !type) { fail(id, -32602, "from, to and type are required"); continue; }
                const auto evidence = string_value(root, "evidence").value_or("");
                if (!service_operation("memory.relations.create", "{\"from\":"+krmemory::service_rpc::quote(*from)+",\"to\":"+krmemory::service_rpc::quote(*to)+",\"type\":"+krmemory::service_rpc::quote(*type)+",\"origin\":"+krmemory::service_rpc::quote("mcp:"+agent)+",\"evidence\":"+krmemory::service_rpc::quote(evidence)+",\"confidence\":0.8}")) { fail(id,-32010,"local memory service unavailable"); continue; }
                audit(agent, *name, *from + " -> " + *to, true);
                reply(id, content("Relationship stored."));
            } else if (*name == "memory_collection_create") {
                if (!require_scope(db, agent, token, "write", id, *name)) continue;
                const auto collection_name = string_value(root, "name");
                if (!collection_name || collection_name->empty()) { fail(id, -32602, "name missing"); continue; }
                const auto kind = string_value(root, "kind").value_or("manual");
                std::string response;
                if (!service_operation("memory.collections.create", "{\"name\":"+krmemory::service_rpc::quote(*collection_name)+",\"kind\":"+krmemory::service_rpc::quote(kind)+"}", &response)) { fail(id,-32010,"local memory service unavailable"); continue; }
                audit(agent,*name,*collection_name,true); reply(id, content("Collection created."));
            } else if (*name == "memory_collection_rename") {
                if (!require_scope(db, agent, token, "write", id, *name)) continue;
                const auto signature = string_value(root, "signature"); const auto collection_name = string_value(root, "name");
                if (!signature || !collection_name) { fail(id, -32602, "signature and name are required"); continue; }
                if (!service_operation("memory.collections.update", "{\"signature\":"+krmemory::service_rpc::quote(*signature)+",\"name\":"+krmemory::service_rpc::quote(*collection_name)+"}")) { fail(id,-32010,"local memory service unavailable"); continue; }
                audit(agent,*name,*signature,true); reply(id, content("Collection updated."));
            } else if (*name == "memory_collection_merge") {
                if (!require_scope(db, agent, token, "write", id, *name)) continue;
                const auto source = string_value(root, "source"); const auto target = string_value(root, "target");
                if (!source || !target) { fail(id, -32602, "source and target are required"); continue; }
                if (!service_operation("memory.collections.merge", "{\"source\":"+krmemory::service_rpc::quote(*source)+",\"target\":"+krmemory::service_rpc::quote(*target)+"}")) { fail(id,-32010,"local memory service unavailable"); continue; }
                audit(agent,*name,*source+" -> "+*target,true); reply(id, content("Collection memberships merged."));
            } else if (*name == "memory_import") {
                if (!require_scope(db, agent, token, "import", id, *name)) continue;
                const auto path = string_value(root, "path");
                if (!path) { fail(id, -32602, "path missing"); continue; }
                if (!service_operation("memory.import", "{\"path\":"+krmemory::service_rpc::quote(*path)+"}")) { fail(id,-32010,"local memory service unavailable"); continue; }
                audit(agent,*name,*path,true); reply(id, content("Imported through the local memory service."));
            } else if (*name == "memory_export") {
                if (!require_scope(db, agent, token, "export", id, *name)) continue;
                const auto path = string_value(root, "path");
                if (!path) { fail(id, -32602, "path missing"); continue; }
                const auto table = string_value(root, "table");
                std::string response;std::string params="{\"path\":"+krmemory::service_rpc::quote(*path);if(table)params+=",\"table\":"+krmemory::service_rpc::quote(*table);params+="}";
                if(!service_operation("memory.export",std::move(params),&response)){fail(id,-32010,"local memory service unavailable");continue;}
                std::size_t count{};try{auto result=krmemory::json::parse(response);auto value=result.get("exported");if(!value||!std::holds_alternative<double>(value->data))throw std::runtime_error("missing export count");count=static_cast<std::size_t>(std::get<double>(value->data));}catch(...){fail(id,-32010,"invalid service response");continue;}
                audit(agent, *name, *path, true);
                reply(id, content("Exported records: " + std::to_string(count)));
            } else if (*name == "memory_delete") {
                if (!require_scope(db, agent, token, "delete", id, *name)) continue;
                const auto signature = string_value(root, "signature");
                if (!signature) { fail(id, -32602, "signature missing"); continue; }
                if (!service_operation("memory.delete", "{\"signature\":"+krmemory::service_rpc::quote(*signature)+"}")) { fail(id,-32010,"local memory service unavailable"); continue; }
                audit(agent,*name,*signature,true); reply(id, content("Memory deletion requested."));
            } else if (*name == "memory_collection_delete") {
                if (!require_scope(db, agent, token, "delete", id, *name)) continue;
                const auto signature = string_value(root, "signature");
                if (!signature) { fail(id, -32602, "signature missing"); continue; }
                if (!service_operation("memory.collections.delete", "{\"signature\":"+krmemory::service_rpc::quote(*signature)+"}")) { fail(id,-32010,"local memory service unavailable"); continue; }
                audit(agent,*name,*signature,true); reply(id, content("Collection deletion requested."));
            } else if (*name == "memory_answer") {
                if (!require_scope(db, agent, token, "ai", id, *name)) continue;
                const auto question = string_value(root, "question");
                if (!question || question->empty() || question->size() > 4096) { fail(id, -32602, "question must be 1..4096 bytes"); continue; }
                const auto model = string_value(root, "model").value_or("");
                const auto context = krmemory::build_chat_context(db, *question);
                const auto answer = krmemory::complete_lm_studio(context.prompt, model);
                audit(agent, *name, *question, answer.success);
                if (!answer.success) { fail(id, -32002, answer.detail); continue; }
                std::string response = answer.text;
                if (!context.source_signatures.empty()) { response += "\n\nSources: "; for (std::size_t i{}; i < context.source_signatures.size(); ++i) { if (i) response += ", "; response += context.source_signatures[i]; } }
                reply(id, content(response));
            } else {
                fail(id, -32601, "tool not found");
            }
        } catch (const std::exception& error) {
            audit(agent, name ? *name : "unknown", error.what(), false);
            fail(id, -32000, error.what());
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        krmemory::apply_process_hardening();
        memory_file = user_memory_file();
        std::string agent, token, grant_agent, grant_scopes, revoke_agent, rotate_agent, rotate_scopes;
        fs::path credential;
        std::optional<long long> lifetime_seconds;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--agent" && i + 1 < argc) agent = argv[++i];
            else if (argument == "--token" && i + 1 < argc) token = argv[++i];
            else if (argument == "--credential" && i + 1 < argc) credential = utf8_path(argv[++i]);
            else if (argument == "--grant" && i + 2 < argc) { grant_agent = argv[++i]; grant_scopes = argv[++i]; }
            else if (argument == "--revoke" && i + 1 < argc) revoke_agent = argv[++i];
            else if (argument == "--rotate" && i + 2 < argc) { rotate_agent = argv[++i]; rotate_scopes = argv[++i]; }
            else if (argument == "--expires-hours" && i + 1 < argc) lifetime_seconds = std::stoll(argv[++i]) * 3600;
            else if (argument == "--expires-seconds" && i + 1 < argc) lifetime_seconds = std::stoll(argv[++i]);
            else throw std::runtime_error("usage: --grant <agent> <scopes> [--expires-hours N|--expires-seconds N] | --rotate <agent> <scopes> [--expires-hours N|--expires-seconds N] | --revoke <agent> | --agent <agent> [--credential <dpapi-file> | --token <token>]");
        }
        if (!grant_agent.empty()) { grant(grant_agent, grant_scopes, lifetime_seconds); return 0; }
        if (!rotate_agent.empty()) { rotate(rotate_agent, rotate_scopes, lifetime_seconds); return 0; }
        if (!revoke_agent.empty()) { revoke(revoke_agent); return 0; }
        if (token.empty() && !credential.empty()) token = read_protected_credential(credential);
        if (token.empty()) if (auto environment_token = std::getenv("KR_MYCELIA_MCP_TOKEN")) token = environment_token;
        serve(agent, token);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "KR Mycelia MCP: " << error.what() << '\n';
        return 1;
    }
}
#else
int main() { return 1; }
#endif
