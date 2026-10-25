#include "memory_export.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
int main(){try{mycelia::DynamicAssociativeDatabase db;mycelia::sql::Row row;row["title"]=std::string("Export \"memory\"");db.store_sql_record("manual_memory",row);auto path=std::filesystem::temp_directory_path()/"kr_mycelia_export_test.json";std::error_code error;std::filesystem::remove(path,error);if(krmemory::export_memories_json(db,path)!=1)return 1;std::ifstream input(path);std::string body((std::istreambuf_iterator<char>(input)),{});std::filesystem::remove(path,error);if(body.find("kr-mycelia-memory-export-v1")==std::string::npos||body.find("Export \\\"memory\\\"")==std::string::npos)return 1;std::cout<<"PASS: explicit DAD JSON export\n";return 0;}catch(...){return 1;}}
