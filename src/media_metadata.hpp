#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <optional>
namespace krmemory {struct MediaMetadata { std::string format; std::uint64_t bytes{}; std::uint32_t width{},height{}; std::optional<double> duration_seconds; bool has_audio{},has_video{}; };MediaMetadata inspect_media(const std::filesystem::path&);}
