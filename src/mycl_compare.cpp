#include "mycl_compare.hpp"
#include "gpu_bridge.hpp"
#include <fstream>
#include <stdexcept>
#include <vector>
#include <algorithm>
namespace mycelia {
namespace {
std::vector<unsigned char> read_all(const std::filesystem::path& path) {
    GpuBridge::validate_mycel_file(path);
    const auto size = std::filesystem::file_size(path);
    std::vector<unsigned char> result(static_cast<std::size_t>(size));
    std::ifstream stream(path,std::ios::binary);
    if (!stream.read(reinterpret_cast<char*>(result.data()),static_cast<std::streamsize>(result.size())))
        throw std::runtime_error("MYCL changed or became unreadable during comparison");
    return result;
}
std::uint64_t fnv(const std::vector<unsigned char>& bytes) {
    std::uint64_t hash=14695981039346656037ULL;
    for(auto b:bytes) { hash^=b; hash*=1099511628211ULL; }
    return hash;
}
}
MyclComparison compare_mycl(const std::filesystem::path& reference,const std::filesystem::path& resumed) {
    const auto a=read_all(reference), b=read_all(resumed);
    MyclComparison result{};
    result.bytes=a.size(); result.left_fnv64=fnv(a); result.right_fnv64=fnv(b);
    result.first_difference=static_cast<std::size_t>(-1);
    const auto common=std::min(a.size(),b.size());
    for(std::size_t i=0;i<common;++i) if(a[i]!=b[i]) {
        if(result.first_difference==static_cast<std::size_t>(-1)) result.first_difference=i;
        ++result.differences;
    }
    result.differences+=std::max(a.size(),b.size())-common;
    if(a.size()!=b.size() && result.first_difference==static_cast<std::size_t>(-1)) result.first_difference=common;
    result.identical=result.differences==0;
    return result;
}
}
