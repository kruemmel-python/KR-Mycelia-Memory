#include "memory_timeline.hpp"
#include <iostream>
#include <stdexcept>
int main(){try{mycelia::DynamicAssociativeDatabase db;for(int t:{10,30,20}){mycelia::sql::Row r;r["title"]=std::to_string(t);r["timestamp"]=(std::int64_t)t;db.store_sql_record("events",r);}auto rows=krmemory::timeline(db,15,30);if(rows.size()!=2||mycelia::sql::display(rows[0].data.at("title"))!="30")throw std::runtime_error("timeline ordering/filter failed");std::cout<<"PASS: chronological DAD timeline\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
