#include "gpu_bridge.hpp"
#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

namespace mycelia {
namespace {
std::string native_path(const std::filesystem::path& path) {
#ifdef _WIN32
    // Original CC_OpenCL uses fopen(char*) and the active CRT codepage;
    // non-ASCII paths are not guaranteed to round-trip. Fail closed.
    const auto wide = path.wstring();
    if (wide.empty()) throw std::invalid_argument("Empty path");
    const int bytes = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS,wide.c_str(),-1,nullptr,0,nullptr,nullptr);
    if (!bytes) throw std::runtime_error("Path encoding failed");
    std::string result(static_cast<std::size_t>(bytes),'\0');
    BOOL lossy = FALSE;
    if (!WideCharToMultiByte(CP_ACP,WC_NO_BEST_FIT_CHARS,wide.c_str(),-1,result.data(),bytes,nullptr,&lossy)||lossy)
        throw std::runtime_error("Original CC_OpenCL cannot represent the file path losslessly");
    result.pop_back();
    return result;
#else
    return path.string();
#endif
}
std::uint32_t read_u32_le(const std::array<unsigned char,28>& a, std::size_t pos) {
    return std::uint32_t(a[pos]) | (std::uint32_t(a[pos+1])<<8) |
        (std::uint32_t(a[pos+2])<<16) | (std::uint32_t(a[pos+3])<<24);
}
}
bool GpuBridge::supports_mycel_persistence() const noexcept {
    return module_ && save_mycel_ && load_mycel_;
}
void GpuBridge::validate_mycel_file(const std::filesystem::path& file) {
    if (!file.is_absolute() || !std::filesystem::is_regular_file(file))
        throw std::invalid_argument("An existing absolute MYCL file path is required");
    // The original loader does not check most fread results or dimensional
    // limits. Never pass malformed/truncated files directly to that loader.
    constexpr std::uint64_t max_file_bytes = 512ULL*1024*1024;
    const auto size = std::filesystem::file_size(file);
    if (size < 28 || size > max_file_bytes) throw std::runtime_error("MYCL file size out of range");
    std::ifstream in(file,std::ios::binary);
    std::array<unsigned char,28> header{};
    if (!in.read(reinterpret_cast<char*>(header.data()),header.size()))
        throw std::runtime_error("MYCL header truncated");
    const std::uint64_t magic = read_u32_le(header,0), version = read_u32_le(header,4);
    const std::uint64_t cap = read_u32_le(header,8), channels=read_u32_le(header,12), neigh=read_u32_le(header,16);
    const std::uint64_t active=read_u32_le(header,20), free_head=read_u32_le(header,24);
    if (magic!=0x4D59434CUL || version!=1 || cap==0 || channels==0 || neigh==0 ||
        cap>1000000 || channels>4096 || neigh>4096 || active>cap || free_head>cap)
        throw std::runtime_error("Invalid MYCL header/dimensions");
    // Mirror the field order and element sizes of the original CC_OpenCL.c.
    // 2 byte fields, 3 int arrays, per-channel fields, edge fields,
    // pheromone tensor, 2 scalar fields and seven scalar extras.
    const auto edges=cap*neigh, pher=edges*channels;
    const std::uint64_t expected=28 + 2*cap + 4*(cap + cap + cap*channels + 2*channels +
        edges + 2*edges + pher + 2*cap + 7);
    if (size!=expected) throw std::runtime_error("MYCL length does not match dimensions");
}
void GpuBridge::save_mycel_state(const std::filesystem::path& file) {
    if (device_<0 || capacity_==0) throw std::logic_error("Mycel must be initialized");
    if (!supports_mycel_persistence()) throw std::runtime_error("Original MYCL persistence exports missing");
    if (!file.is_absolute() || std::filesystem::exists(file))
        throw std::invalid_argument("New absolute path required; refusing overwrite");
    const auto p=native_path(file);
    if (!save_mycel_(device_,p.c_str())) throw std::runtime_error("Original MYCL save failed: "+last_error());
    validate_mycel_file(file);
}
void GpuBridge::load_mycel_state(const std::filesystem::path& file) {
    if (device_<0) throw std::logic_error("GPU must be initialized");
    if (!supports_mycel_persistence()) throw std::runtime_error("Original MYCL persistence exports missing");
    validate_mycel_file(file);
    // Preserve dimensions validated from the trusted MYCL header. The original
    // load_mycel_state() calls mycel_initialize() and uploads its restored buffers.
    // The bridge must adopt these values so update() and nutrient() remain usable.
    std::ifstream header_stream(file, std::ios::binary);
    std::array<unsigned char, 28> header{};
    if (!header_stream.read(reinterpret_cast<char*>(header.data()), header.size()))
        throw std::runtime_error("MYCL header read failed after validation");
    const int restored_capacity = static_cast<int>(read_u32_le(header, 8));
    const int restored_active = static_cast<int>(read_u32_le(header, 20));
    const auto p=native_path(file);
    if (!load_mycel_(device_,p.c_str())) {
        // An unsuccessful native load may have replaced/freed the previous state.
        // Never allow further updates without explicit reinitialization.
        capacity_ = active_ = 0;
        throw std::runtime_error("Original MYCL load failed: "+last_error());
    }
    capacity_ = restored_capacity;
    active_ = restored_active;
}
}
