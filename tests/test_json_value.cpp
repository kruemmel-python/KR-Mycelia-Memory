#include "json_value.hpp"
#include <iostream>
int main(){try{auto v=krmemory::json::parse("{\"jsonrpc\":\"2.0\",\"id\":7,\"params\":{\"name\":\"memory_search\",\"arguments\":{\"query\":\"native \\u00e4 \\ud83e\\udd20\"}}}");auto p=v.get("params");auto a=p?p->get("arguments"):nullptr;if(!p||!a||!p->get("name")->string()||*a->get("query")->string()!="native \xC3\xA4 \xF0\x9F\xA4\xA0")return 1;std::cout<<"PASS: structured JSON-RPC parser\n";return 0;}catch(...){return 1;}}
