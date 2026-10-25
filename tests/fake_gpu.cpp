#ifdef _WIN32
#define EXP extern "C" __declspec(dllexport)
#else
#define EXP extern "C" __attribute__((visibility("default")))
#endif
#include <algorithm>
#include <vector>
static int cap=0,active=0;
EXP int initialize_gpu(int index){return index==0;}
EXP void shutdown_gpu(int){cap=active=0;}
EXP int subqg_init_mycel(int,int t,int c,int k){cap=(t>0&&c>0&&k>0)?t:0;return cap>0;}
EXP int subqg_set_active_T(int,int n){if(n<0||n>cap)return 0;active=n;return 1;}
EXP int step_mycel_update(int,const float* a){return active>0&&a!=nullptr;}
EXP int read_nutrient(int,float* o){if(!o)return 0;std::fill_n(o,cap,0.25f);return 1;}
EXP const char* cc_get_last_error(){return "test driver";}

#include <cstdint>
#include <cstdio>
EXP int save_mycel_state(int, const char* name) {
    if(cap<=0||!name)return 0;
    FILE* f=std::fopen(name,"wb"); if(!f)return 0;
    const std::uint32_t h[7]={0x4D59434C,1,static_cast<unsigned>(cap),2,3,static_cast<unsigned>(active),0};
    std::fwrite(h,sizeof(h),1,f);
    const std::size_t n=static_cast<std::size_t>(cap),e=n*3,ph=e*2;
    const std::size_t rest=2*n+4*(n+n+n*2+2*2+e+2*e+ph+2*n+7);
    std::vector<unsigned char> payload(rest,0);
    std::fwrite(payload.data(),1,payload.size(),f);
    return std::fclose(f)==0;
}
EXP int load_mycel_state(int,const char* name) {
    if(!name)return 0;
    FILE* f=std::fopen(name,"rb"); if(!f)return 0;
    std::uint32_t h[7]{};
    const bool ok=std::fread(h,sizeof(h),1,f)==1&&h[0]==0x4D59434C&&h[1]==1;
    std::fclose(f);
    if(ok){cap=static_cast<int>(h[2]);active=static_cast<int>(h[5]);}
    return ok;
}
