#include "gpu_bridge.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
using mycelia::GpuBridge;
static void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
int main(int argc,char** argv){
 try{
 check(argc==2,"missing test library path");
 GpuBridge g;
 try{g.open("relative-library.dll");throw std::runtime_error("relative path accepted");}
 catch(const std::invalid_argument&){}
 g.open(std::filesystem::absolute(argv[1]));
 check(g.loaded(),"not loaded");
 try{g.update(std::vector<float>{1});throw std::runtime_error("uninitialized accepted");}
 catch(const std::logic_error&){}
 g.initialize(0);g.init_mycel(8,2,3);g.set_active(3);
 try{g.update(std::vector<float>{1,2});throw std::runtime_error("bad length accepted");}
 catch(const std::invalid_argument&){}
 g.update(std::vector<float>{1,2,3});
 const auto n=g.nutrient();check(n.size()==3,"nutrient length");
 for(float x:n)check(std::abs(x-0.25f)<1e-6f,"nutrient value");
 try{g.set_active(9);throw std::runtime_error("capacity breach accepted");}
 catch(const std::out_of_range&){}
 g.close();check(!g.loaded(),"close failed");
 std::cout<<"native CC_OpenCL ABI bridge contract PASS\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
