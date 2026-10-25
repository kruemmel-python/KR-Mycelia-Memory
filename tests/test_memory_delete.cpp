#include "memory_delete.hpp"
#include "memory_relations.hpp"
#include <iostream>
int main(){mycelia::DynamicAssociativeDatabase db;mycelia::sql::Row r;r["title"]=std::string("a");auto a=db.store_sql_record("m",r);r["title"]=std::string("b");auto b=db.store_sql_record("m",r);krmemory::relate(db,{a,b,"related_to","test","",.5});auto x=krmemory::delete_memory(db,a);if(!x.deleted||x.relationships_deleted!=1||db.get_sql_record(a)||!db.get_sql_record(b))return 1;std::cout<<"PASS: privacy memory deletion removes relationships\n";}
