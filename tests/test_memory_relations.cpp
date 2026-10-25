#include "memory_relations.hpp"
#include <iostream>
#include <stdexcept>
int main(){try{mycelia::DynamicAssociativeDatabase db;mycelia::sql::Row a;a["title"]=std::string("failure");auto x=db.store_sql_record("events",a);a["title"]=std::string("fix");auto y=db.store_sql_record("events",a);krmemory::relate(db,{x,y,"resolved_by","test","regression evidence",.9});if(krmemory::related(db,x).size()!=1)throw std::runtime_error("relation absent");std::cout<<"PASS: evidence-backed DAD memory relations\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
