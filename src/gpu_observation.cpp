#include "gpu_observation.hpp"
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace mycelia {
namespace {
constexpr std::string_view magic = "KRGPUOBS1";
constexpr std::size_t max_world = 256ull * 1024 * 1024 + 4096;
constexpr std::size_t max_cells = 1'000'000;
constexpr std::size_t digest_length = 64;
void write_u32(std::string& dst, std::uint32_t value) {
    for (int i = 0; i != 4; ++i) dst.push_back(static_cast<char>(value >> (8*i)));
}
void write_u64(std::string& dst, std::uint64_t value) {
    for (int i = 0; i != 8; ++i) dst.push_back(static_cast<char>(value >> (8*i)));
}
std::uint32_t read_u32(std::string_view src, std::size_t& at) {
    if (at > src.size() || src.size()-at < 4) throw std::runtime_error("GPU checkpoint truncated u32");
    std::uint32_t value = 0;
    for (int i=0;i!=4;++i) value |= std::uint32_t(static_cast<unsigned char>(src[at++])) << (8*i);
    return value;
}
std::uint64_t read_u64(std::string_view src, std::size_t& at) {
    if (at > src.size() || src.size()-at < 8) throw std::runtime_error("GPU checkpoint truncated u64");
    std::uint64_t value = 0;
    for (int i=0;i!=8;++i) value |= std::uint64_t(static_cast<unsigned char>(src[at++])) << (8*i);
    return value;
}
void validate(const GpuObservation& gpu) {
    if (gpu.capacity==0 || gpu.capacity > max_cells || gpu.active_cells==0 ||
        gpu.active_cells>gpu.capacity || gpu.activity.size()!=gpu.active_cells ||
        gpu.nutrient.size()!=gpu.active_cells)
        throw std::invalid_argument("GPU observation shape invalid");
    for (float v : gpu.activity) if (!std::isfinite(v)) throw std::invalid_argument("GPU activity nonfinite");
    for (float v : gpu.nutrient) if (!std::isfinite(v)) throw std::invalid_argument("GPU nutrient nonfinite");
}
void put_floats(std::string& dst, const std::vector<float>& data) {
    for (float f : data) write_u32(dst, std::bit_cast<std::uint32_t>(f));
}
std::vector<float> get_floats(std::string_view bytes, std::size_t& at, std::uint32_t count) {
    std::vector<float> result;
    result.reserve(count);
    for (std::uint32_t i=0;i<count;++i) result.push_back(std::bit_cast<float>(read_u32(bytes,at)));
    return result;
}
}
GpuObservation GpuObservationCodec::capture(const GpuBridge& gpu,
          std::span<const float> activity, std::uint32_t capacity,
          std::uint64_t simulation_step) {
    if (activity.size() > max_cells || capacity < activity.size() ||
        activity.empty() || capacity==0 || capacity>max_cells)
        throw std::invalid_argument("GPU capture dimensions invalid");
    GpuObservation out{simulation_step,capacity,static_cast<std::uint32_t>(activity.size()),
                       {activity.begin(), activity.end()},gpu.nutrient()};
    validate(out);
    return out;
}
std::string GpuObservationCodec::encode(const MyceliaWorldCpu& world,
                                        const DynamicAssociativeDatabase& memory,
                                        const GpuObservation& gpu) {
    validate(gpu);
    const auto checkpoint = WorldCheckpointCodec::encode(world,memory);
    if (checkpoint.size()>max_world) throw std::invalid_argument("world checkpoint too large");
    std::string output(magic);
    write_u64(output,checkpoint.size());
    write_u64(output,gpu.simulation_step);
    write_u32(output,gpu.capacity);
    write_u32(output,gpu.active_cells);
    output += checkpoint;
    put_floats(output,gpu.activity);
    put_floats(output,gpu.nutrient);
    output += DynamicAssociativeDatabase::sha256(output);
    return output;
}
ObservedWorldCheckpoint GpuObservationCodec::decode(std::string_view bytes) {
    if (bytes.size()<magic.size()+8+8+4+4+digest_length || !bytes.starts_with(magic))
        throw std::runtime_error("GPU checkpoint header invalid");
    std::size_t at=magic.size();
    auto len=read_u64(bytes,at);
    auto step=read_u64(bytes,at);
    auto capacity=read_u32(bytes,at);
    auto active=read_u32(bytes,at);
    if (len>max_world || active==0 || active>capacity || capacity>max_cells ||
        len>bytes.size()-at || active > (bytes.size()-at-len)/8 ||
        bytes.size()-at-len != std::size_t(active)*8+digest_length)
        throw std::runtime_error("GPU checkpoint lengths invalid");
    auto prefix=bytes.substr(0,bytes.size()-digest_length);
    if (DynamicAssociativeDatabase::sha256(prefix)!=bytes.substr(bytes.size()-digest_length))
        throw std::runtime_error("GPU checkpoint digest invalid");
    auto world = WorldCheckpointCodec::decode(bytes.substr(at,static_cast<std::size_t>(len)));
    at+=static_cast<std::size_t>(len);
    auto activity=get_floats(bytes,at,active);
    auto nutrient=get_floats(bytes,at,active);
    GpuObservation gpu{step,capacity,active,std::move(activity),std::move(nutrient)};
    validate(gpu);
    return {std::move(world),std::move(gpu)};
}
}
