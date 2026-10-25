#include "local_ai.hpp"
#include <iostream>
#include <string>
int main(){auto blocked=krmemory::probe_lm_studio("example.invalid",1234);if(blocked.available)return 1;auto oversized=krmemory::complete_lm_studio(std::string(32769,'x'));if(oversized.success)return 1;std::cout<<"PASS: local AI accepts loopback only\n";}
