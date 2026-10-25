#include "mycl_compare.hpp"
#include "gpu_bridge.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <chrono>
static void check(bool okay,const char* message) {if(!okay)throw std::runtime_error(message);}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    const auto dir=std::filesystem::temp_directory_path()/("mycelia_parity_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    try {
        const auto a=dir/"a.mycl", b=dir/"b.mycl";
        mycelia::GpuBridge bridge;
        bridge.open(std::filesystem::absolute(argv[1]));
        bridge.initialize(0); bridge.init_mycel(24,2,3); bridge.set_active(6);
        bridge.update(std::vector<float>(6,0.15f)); bridge.save_mycel_state(a);
        std::filesystem::copy_file(a,b);
        auto same=mycelia::compare_mycl(a,b);
        check(same.identical && same.differences==0,"same file failed equality");
        {
            std::fstream f(b,std::ios::in|std::ios::out|std::ios::binary);
            f.seekp(45); char changed='\x7f'; f.write(&changed,1);
        }
        auto diff=mycelia::compare_mycl(a,b);
        check(!diff.identical && diff.first_difference==45 && diff.differences==1,"mutation not detected");
        {
            std::ofstream truncate(b,std::ios::binary|std::ios::trunc); truncate<<"MYCL";
        }
        bool rejected=false;
        try{(void)mycelia::compare_mycl(a,b);}catch(const std::exception&){rejected=true;}
        check(rejected,"truncated file accepted");
        bridge.close();
        std::filesystem::remove_all(dir);
        std::cout<<"PASS: MYCL exact comparison, mutation detection, malformed rejection\n";
        return 0;
    }catch(const std::exception& e){std::filesystem::remove_all(dir);std::cerr<<e.what()<<'\n';return 1;}
}
