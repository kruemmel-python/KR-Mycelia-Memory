#include "memory_service_rpc.hpp"
#include <iostream>
#include <string>
int main(int argc,char** argv){using krmemory::service_rpc::response_ok;
if(argc>1&&std::string_view(argv[1])=="--live"){std::string response;bool ok=krmemory::service_rpc::call("rpc-live-list","memory.list","{\"query\":\"\",\"source\":\"\",\"collection\":\"\",\"limit\":500,\"summary\":true}",response,30000);std::cout<<"ok="<<ok<<" bytes="<<response.size()<<" head="<<response.substr(0,300)<<"\n";return ok?0:20;}
if(!response_ok("{\"version\":1,\"id\":\"a\",\"ok\":true}","a"))return 1;
if(response_ok("{\"version\":1,\"id\":\"wrong\",\"ok\":true}","a"))return 2;
if(response_ok("{\"version\":2,\"id\":\"a\",\"ok\":true}","a"))return 3;
if(response_ok("{\"version\":1,\"id\":\"a\",\"ok\":false,\"error\":\"x\"}","a"))return 4;
if(response_ok("{\"message\":\"\\\"ok\\\":true\",\"version\":1,\"id\":\"a\"}","a"))return 5;
if(response_ok("{\"version\":1,\"id\":\"a\",\"ok\":true", "a"))return 6;
std::string large="{\"version\":1,\"id\":\"large\",\"ok\":true,\"payload\":\""+std::string(256*1024,'x')+"\"}";
if(!response_ok(large,"large"))return 7;
large.append(krmemory::service_rpc::maximum_response,'x');
if(response_ok(large,"large"))return 8;
std::cout<<"PASS: strict service RPC response validation\n";}
