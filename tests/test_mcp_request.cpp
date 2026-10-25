#include "mcp_request.hpp"
#include <iostream>
int main(){auto raw=krmemory::json::parse("{\"jsonrpc\":\"2.0\",\"id\":12,\"method\":\"tools/call\",\"params\":{\"name\":\"memory_search\",\"arguments\":{\"query\":\"x\"}}}");auto r=krmemory::mcp::parse_request(raw);if(!r||r->id_json!="12"||r->name!="memory_search"||!r->arguments)return 1;std::cout<<"PASS: structured MCP request\n";}
