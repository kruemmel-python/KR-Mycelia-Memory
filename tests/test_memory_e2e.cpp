#include "memory_import.hpp"
#include "memory_collections.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
static void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){try{auto dir=std::filesystem::temp_directory_path()/"kr_mycelia_e2e_project";std::filesystem::create_directories(dir);auto source=dir/"architecture.md";{std::ofstream f(source);f<<"Native Mycelia memory architecture uses a durable DAD collection.";}mycelia::DynamicAssociativeDatabase db;auto imported=krmemory::import_file(db,source);auto bytes=db.snapshot_bytes();auto restored=mycelia::DynamicAssociativeDatabase::from_snapshot_bytes(bytes);require(restored.get_sql_record(imported.signature).has_value(),"import absent after restore");auto cols=krmemory::collections(restored);require(cols.size()==1,"automatic collection absent after restore");require(krmemory::collection_members(restored,cols.front().signature).size()==1,"relationship absent after restore");auto matches=restored.associative_sql_lookup("Native Mycelia memory architecture",1.0,10);bool found=false;for(const auto& m:matches)if(m.signature==imported.signature)found=true;require(found,"associative search did not recover imported memory");std::filesystem::remove(source);std::filesystem::remove(dir);std::cout<<"PASS: import -> DAD -> collection -> restart -> associative retrieval\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
