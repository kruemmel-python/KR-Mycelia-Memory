#include "mycel_bundle.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mycelia;
static void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
static void u32(std::string& s,std::size_t p,std::uint32_t n){for(int i=0;i<4;++i)s[p+i]=static_cast<char>(n>>(8*i));}
int main(){try{
 MyceliaWorldCpu world(4,3,1,4,0.1);DynamicAssociativeDatabase memory;
 auto frame=world.step();auto obs=ObserverNetwork::encode(frame.fields.pheromone,frame.fields.nutrient);
 (void)memory.observe(frame.fields,obs,obs.harmony_score,obs.tension_score,0.5);
 const std::size_t cap=64,channels=2,neigh=3,edges=cap*neigh;
 const std::size_t n=28+2*cap+4*(cap+cap+cap*channels+2*channels+edges+2*edges+edges*channels+2*cap+7);
 std::string mycl(n,'\0');u32(mycl,0,0x4D59434C);u32(mycl,4,1);u32(mycl,8,cap);u32(mycl,12,channels);u32(mycl,16,neigh);u32(mycl,20,8);u32(mycl,24,0);
 for(std::size_t i=28;i<n;++i)mycl[i]=static_cast<char>((i*7)%251);
 const auto data=MycelBundleCodec::encode(world,memory,mycl);
 auto restored=MycelBundleCodec::decode(data);
 require(restored.mycl==mycl,"MYCL mismatch");
 require(restored.checkpoint.world.serialize()==world.serialize(),"world mismatch");
 require(restored.checkpoint.memory.snapshot_bytes()==memory.snapshot_bytes(),"memory mismatch");
 auto changed=data;changed[40]^=1;bool rejected=false;
 try{(void)MycelBundleCodec::decode(changed);}catch(const std::exception&){rejected=true;}require(rejected,"mutation accepted");
 rejected=false;try{(void)MycelBundleCodec::decode(data.substr(0,data.size()-1));}catch(const std::exception&){rejected=true;}require(rejected,"truncation accepted");
 auto wrong=mycl;u32(wrong,8,65);rejected=false;
 try{(void)MycelBundleCodec::encode(world,memory,wrong);}catch(const std::exception&){rejected=true;}require(rejected,"bad MYCL dimension accepted");
 auto file=std::filesystem::temp_directory_path()/"kr_mycel_bundle_v18_encrypted_test.kmb";
 std::filesystem::remove(file);
#ifdef _WIN32
 MycelBundleCodec::save_secure(file,world,memory,mycl);
 {std::ifstream in(file,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(in)),{});require(bytes.starts_with("KRBNDENC"),"missing DPAPI envelope");require(bytes.find("KRMYBND1")==std::string::npos,"plaintext bundle leaked");}
 auto loaded=MycelBundleCodec::load_secure(file);require(loaded.mycl==mycl,"encrypted MYCL mismatch");
 rejected=false;try{MycelBundleCodec::save_secure(file,world,memory,mycl);}catch(const std::exception&){rejected=true;}require(rejected,"overwriting accepted");
 std::filesystem::remove(file);
#else
 rejected=false;try{MycelBundleCodec::save_secure(file,world,memory,mycl);}catch(const std::exception&){rejected=true;}require(rejected&&!std::filesystem::exists(file),"unencrypted fallback accepted");
#endif
 std::cout<<"PASS: combined MYCL/world/DAD bundle integrity, resume state, secure policy (NOT entire VRAM)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
