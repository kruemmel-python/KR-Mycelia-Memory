#include "media_metadata.hpp"
#include "memory_import.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
int main(){try{auto p=std::filesystem::temp_directory_path()/"kr_mycelia_test.png";const unsigned char png[]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,3,8,2,0,0,0};{std::ofstream f(p,std::ios::binary);f.write((const char*)png,sizeof(png));}auto meta=krmemory::inspect_media(p);if(meta.format!="png"||meta.width!=2||meta.height!=3){std::cerr<<"metadata mismatch: "<<meta.format<<' '<<meta.width<<'x'<<meta.height<<'\n';return 1;}mycelia::DynamicAssociativeDatabase db;auto r=krmemory::import_file(db,p);if(!db.get_sql_record(r.signature)){std::cerr<<"record missing\n";return 1;}std::filesystem::remove(p);std::cout<<"PASS: native media metadata import\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
