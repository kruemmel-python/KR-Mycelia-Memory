#include "mycelia_world.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace mycelia {
namespace {
constexpr double pi=3.141592653589793238462643383279502884;
void ensure(bool condition,const char* reason) { if(!condition) throw std::invalid_argument(reason); }
template<typename T> void put(std::string& output,T value) {
    static_assert(std::is_trivially_copyable_v<T>);
    const auto* ptr=reinterpret_cast<const char*>(&value);
    output.append(ptr,sizeof(T));
}
template<typename T> T take(std::string_view bytes,std::size_t& pos) {
    if(bytes.size()-pos<sizeof(T)) throw std::invalid_argument("Truncated world snapshot");
    T v{}; std::memcpy(&v,bytes.data()+pos,sizeof(v)); pos+=sizeof(v); return v;
}
}
MyceliaWorldCpu::MyceliaWorldCpu(std::size_t width,std::size_t height,std::size_t depth,
                                 std::size_t neighbor_degree,double time_step)
 : width_(width),height_(height),depth_(depth),degree_(neighbor_degree),dt_(time_step) {
    ensure(width>0 && height>0 && depth>0,"Invalid grid dimensions");
    ensure(width<=std::numeric_limits<std::size_t>::max()/height,"Grid overflow");
    ensure(width*height<=static_cast<std::size_t>(std::numeric_limits<int>::max()),"Grid index overflow");
    ensure(neighbor_degree>0 && neighbor_degree<=8,"Neighbor degree must be 1..8");
    ensure(std::isfinite(dt_) && dt_>0,"Invalid time step");
    const auto count=width_*height_;
    const std::array<std::array<int,2>,8> offsets{{{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,1},{-1,1},{1,-1}}};
    neighbors_.reserve(count*degree_);
    for(std::size_t y=0;y<height_;++y) for(std::size_t x=0;x<width_;++x) {
        std::size_t found=0;
        for(const auto& pair:offsets) {
            if(found==degree_) break;
            const auto nx=static_cast<std::int64_t>(x)+pair[0];
            const auto ny=static_cast<std::int64_t>(y)+pair[1];
            if(nx>=0 && ny>=0 && nx<static_cast<std::int64_t>(width_) && ny<static_cast<std::int64_t>(height_)) {
                neighbors_.push_back(static_cast<int>(static_cast<std::size_t>(ny)*width_+static_cast<std::size_t>(nx)));
                ++found;
            }
        }
        while(found++<degree_) neighbors_.push_back(-1);
    }
    refresh_fields();
}
void MyceliaWorldCpu::refresh_fields() {
    const auto count=width_*height_;
    frame_.fields.energy.assign(count,fallback_[0]);
    frame_.fields.pheromone.assign(count,fallback_[1]);
    frame_.fields.nutrient.assign(count,fallback_[2]);
    frame_.activity.assign(count,static_cast<float>(fallback_[0]));
}
std::vector<float> MyceliaWorldCpu::derive_activity(std::span<const double> energy) const {
    if(energy.empty()) return std::vector<float>(width_*height_,0.f);
    for(double v:energy) ensure(std::isfinite(v),"Nonfinite energy");
    const auto [lo,hi]=std::minmax_element(energy.begin(),energy.end());
    const double span=std::max(*hi-*lo,1e-6);
    std::vector<float> result; result.reserve(energy.size());
    for(double v:energy) result.push_back(static_cast<float>((v-*lo)/span));
    return result;
}
WorldFrame MyceliaWorldCpu::step() {
    ensure(frame_.step<std::numeric_limits<std::uint64_t>::max(),"Step overflow");
    const double phase=phase_+dt_;
    ensure(std::isfinite(phase),"Phase overflow");
    phase_=phase;
    fallback_[0]=std::clamp(0.5+0.5*std::sin(phase_),0.,1.);
    fallback_[1]=std::clamp(0.5+0.5*std::sin(phase_*0.5+pi/4),0.,1.);
    fallback_[2]=std::clamp(0.5+0.5*std::cos(phase_*0.33),0.,1.);
    refresh_fields();
    WorldFrame result=frame_;
    ++frame_.step;
    return result;
}
void MyceliaWorldCpu::inject_agents(std::span<const std::array<double,4>> descriptors) {
    auto updated=fallback_;
    for(const auto& d:descriptors) {
        for(double v:d) ensure(std::isfinite(v),"Nonfinite agent value");
        updated[0]=std::clamp(updated[0]+d[2]*0.05,0.,1.);
        updated[1]=std::clamp(updated[1]+d[3]*0.03,0.,1.);
        updated[2]=std::clamp(updated[2]+d[2]*0.02,0.,1.);
    }
    fallback_=updated;
    // Original updates fallback state but DOES NOT refresh the current frame
    // until a subsequent CPU evolve() step.
}
std::string MyceliaWorldCpu::serialize() const {
    std::string out="MYCW011";
    for(auto n:{width_,height_,depth_,degree_}) put<std::uint64_t>(out,static_cast<std::uint64_t>(n));
    put(out,dt_);put(out,phase_);put(out,frame_.step);
    for(double x:fallback_) put(out,x);
    return out;
}
MyceliaWorldCpu MyceliaWorldCpu::deserialize(std::string_view bytes) {
    ensure(bytes.starts_with("MYCW011"),"Invalid world snapshot magic");
    std::size_t pos=7;
    const auto w=take<std::uint64_t>(bytes,pos),h=take<std::uint64_t>(bytes,pos);
    const auto d=take<std::uint64_t>(bytes,pos),degree=take<std::uint64_t>(bytes,pos);
    const auto dt=take<double>(bytes,pos),phase=take<double>(bytes,pos);
    const auto step=take<std::uint64_t>(bytes,pos);
    std::array<double,3> fallback{};
    for(double& v:fallback) v=take<double>(bytes,pos);
    ensure(pos==bytes.size(),"Unexpected world snapshot payload");
    ensure(std::isfinite(phase),"Invalid world phase");
    for(double v:fallback) ensure(std::isfinite(v)&&v>=0&&v<=1,"Invalid fallback value");
    MyceliaWorldCpu world(w,h,d,degree,dt);
    world.phase_=phase;world.frame_.step=step;world.fallback_=fallback;world.refresh_fields();
    return world;
}
}
