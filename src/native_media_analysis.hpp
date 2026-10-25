#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace krmemory {

struct AudioSegment {
    double start_seconds{};
    double end_seconds{};
    double rms{};
    std::string transcript;
};

struct AudioAnalysis {
    bool decoded{};
    bool cancelled{};
    std::uint32_t sample_rate{16000};
    std::vector<std::int16_t> mono_pcm;
    std::vector<AudioSegment> segments;
    std::string transcription_engine;
    std::string transcription_status;
};

AudioAnalysis decode_audio_native(const std::filesystem::path& path,
                                  const std::atomic_bool* cancel = nullptr,
                                  std::uint32_t maximum_minutes = 180);

struct VideoFrameAnalysis {
    double timestamp_seconds{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint64_t perceptual_hash{};
    double scene_score{};
    bool scene_change{};
    std::string recognized_text;
};

struct VideoAnalysis {
    bool decoded{};
    bool cancelled{};
    bool has_audio{};
    double duration_seconds{};
    std::vector<VideoFrameAnalysis> frames;
    std::string detail;
};

VideoAnalysis analyze_video_native(const std::filesystem::path& path,
                                   const std::atomic_bool* cancel = nullptr,
                                   double sample_interval_seconds = 2.0,
                                   std::uint32_t maximum_frames = 120);

}
