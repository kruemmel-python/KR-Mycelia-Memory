#include "folder_collector.hpp"
#include <fstream>
#include <iostream>
int main(){try{auto root=std::filesystem::temp_directory_path()/"kr_mycelia_collector";std::filesystem::create_directories(root);auto f=root/"build.log";{std::ofstream o(f);o<<"build passed";}mycelia::DynamicAssociativeDatabase db;auto r=krmemory::collect_changed_file(db,f,{root});std::filesystem::remove(f);std::filesystem::remove(root);if(!db.get_sql_record(r.signature))return 1;std::cout<<"PASS: approved folder collector imports changed file\n";return 0;}catch(...){return 1;}}
