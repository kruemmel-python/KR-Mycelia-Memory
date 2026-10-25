#include "mycelia.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <iostream>

void require(bool value, const char* msg){if(!value)throw std::runtime_error(msg);}
int main(){
    using namespace mycelia;
    DynamicAssociativeDatabase db;
    Pattern p; p.signature="test-pattern";p.energy_mean=.75;db.store_pattern(p);
    const auto encoded=db.snapshot_bytes();
    auto restored=DynamicAssociativeDatabase::from_snapshot_bytes(encoded);
    require(restored.attractor_count()==1,"memory snapshot failed");
    require(restored.get_pattern("test-pattern")!=nullptr,"pattern missing");
    auto broken=encoded;broken[20]^=1;
    bool rejected=false;
    try{auto bad=DynamicAssociativeDatabase::from_snapshot_bytes(broken);(void)bad;}catch(const std::exception&){rejected=true;}
    require(rejected,"modified snapshot accepted");
    const auto path=std::filesystem::temp_directory_path()/"kr_mycelia_secure_test.bin";
#ifdef _WIN32
    db.save_secure_snapshot(path);
    {std::ifstream f(path,std::ios::binary);std::string disk((std::istreambuf_iterator<char>(f)),{});
     require(disk.find("MYCELIA6")==std::string::npos,"plaintext header leaked");
     require(disk.find("test-pattern")==std::string::npos,"plaintext pattern leaked");
     require(disk.starts_with("KRMSEC02"),"secure envelope header missing");}
    auto reload=DynamicAssociativeDatabase::load_secure_snapshot(path);
    require(reload.get_pattern("test-pattern")!=nullptr,"secure restore failed");
    {std::fstream f(path,std::ios::binary|std::ios::in|std::ios::out);f.seekg(10);char b=0;f.read(&b,1);b^=0x5a;f.seekp(10);f.write(&b,1);}
    rejected=false;
    try{auto bad=DynamicAssociativeDatabase::load_secure_snapshot(path);(void)bad;}catch(const std::exception&){rejected=true;}
    require(rejected,"tampered ciphertext accepted");
    std::filesystem::remove(path);
    // Truncation and invalid headers must also fail closed.
    {std::ofstream f(path,std::ios::binary|std::ios::trunc);f<<"KRMSEC02";}
    rejected=false;
    try{auto bad=DynamicAssociativeDatabase::load_secure_snapshot(path);(void)bad;}catch(const std::exception&){rejected=true;}
    require(rejected,"truncated envelope accepted");
    std::filesystem::remove(path);
#else
    rejected=false;
    try{db.save_secure_snapshot(path);}catch(const std::exception&){rejected=true;}
    require(rejected,"Linux must fail closed for DPAPI");
    require(!std::filesystem::exists(path),"plaintext file leaked");
#endif
    std::cout<<"secure snapshot tests PASS\n";
}
