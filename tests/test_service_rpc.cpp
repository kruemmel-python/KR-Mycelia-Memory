#include "memory_service_rpc.hpp"
#include <iostream>
int main(){using krmemory::service_rpc::response_ok;
if(!response_ok("{\"version\":1,\"id\":\"a\",\"ok\":true}","a"))return 1;
if(response_ok("{\"version\":1,\"id\":\"wrong\",\"ok\":true}","a"))return 2;
if(response_ok("{\"version\":2,\"id\":\"a\",\"ok\":true}","a"))return 3;
if(response_ok("{\"version\":1,\"id\":\"a\",\"ok\":false,\"error\":\"x\"}","a"))return 4;
if(response_ok("{\"message\":\"\\\"ok\\\":true\",\"version\":1,\"id\":\"a\"}","a"))return 5;
if(response_ok("{\"version\":1,\"id\":\"a\",\"ok\":true", "a"))return 6;
std::cout<<"PASS: strict service RPC response validation\n";}
