#include "gpu_bridge.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using mycelia::GpuBridge;
static void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(int argc,char**argv){
    try{
        check(argc==2,"need fake DLL path");
        auto lib=std::filesystem::absolute(argv[1]);
        auto target=std::filesystem::temp_directory_path()/"kr_mycelia_v015_test_state.mycl";
        std::filesystem::remove(target);
        GpuBridge g;g.open(lib);
        check(g.supports_mycel_persistence(),"persistence exports not found");
        g.initialize(0);g.init_mycel(8,2,3);g.set_active(3);
        g.save_mycel_state(target);
        GpuBridge::validate_mycel_file(target);
        bool rejected=false;
        try{g.save_mycel_state(target);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"overwrite accepted");
        g.load_mycel_state(target);
        check(g.capacity()==8 && g.active_cells()==3,"loaded dimensions lost");
        g.update(std::vector<float>{1,2,3});
        check(g.nutrient().size()==3,"GPU readback unavailable after restore");
        auto bad=target;bad+=".bad";
        std::filesystem::copy_file(target,bad,std::filesystem::copy_options::overwrite_existing);
        std::filesystem::resize_file(bad,std::filesystem::file_size(bad)-4);
        rejected=false;
        try{GpuBridge::validate_mycel_file(bad);}catch(const std::runtime_error&){rejected=true;}
        check(rejected,"truncated original state accepted");
        std::filesystem::remove(bad);
        std::filesystem::remove(target);
        g.close();
        std::cout<<"PASS: original MYCL state save/load ABI, bounds and fail-closed validation\n";
        return 0;
    }catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
