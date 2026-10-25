#pragma once
#include "json_value.hpp"
#include <optional>
#include <string>
namespace krmemory::mcp {
struct Request { std::string id_json{"null"},method,name; const json::Value* arguments{}; bool notification{}; };
inline std::optional<Request> parse_request(const json::Value& root){if(!root.is_object())return{};auto version=root.get("jsonrpc"),method=root.get("method");if(!version||!version->string()||*version->string()!="2.0"||!method||!method->string())return{};Request r;r.method=*method->string();r.notification=!root.get("id");if(auto id=root.get("id")){if(auto s=id->string())r.id_json="\""+*s+"\"";else if(auto n=std::get_if<double>(&id->data))r.id_json=std::to_string((long long)*n);}if(auto params=root.get("params");params&&params->is_object()){if(auto name=params->get("name");name&&name->string())r.name=*name->string();r.arguments=params->get("arguments");}return r;}
}
