#include "agent_audit.hpp"
#include <iostream>
int main(){mycelia::DynamicAssociativeDatabase db;krmemory::audit_agent_event(db,"codex","memory_search","query accepted",true);if(krmemory::agent_audit(db,"codex").size()!=1)return 1;std::cout<<"PASS: persistent agent audit\n";}
