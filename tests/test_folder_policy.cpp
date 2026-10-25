#include "folder_policy.hpp"
#include <iostream>
int main(){auto root=std::filesystem::temp_directory_path()/"kr_mycelia_allowed";std::filesystem::create_directories(root/"src");bool ok=krmemory::permitted_folder(root/"src",{root})&&!krmemory::permitted_folder(root/"secret",{root});std::filesystem::remove_all(root);if(!ok)return 1;std::cout<<"PASS: opt-in folder policy\n";}
