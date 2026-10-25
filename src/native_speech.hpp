#pragma once
#include <atomic>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace krmemory {

struct SpeechPiece {
    double start_seconds{};
    double end_seconds{};
    std::string text;
    float confidence{};
};

struct SpeechTranscript {
    bool engine_available{};
    bool success{};
    bool cancelled{};
    std::string engine;
    std::string language;
    std::string detail;
    std::vector<SpeechPiece> pieces;
};

SpeechTranscript transcribe_pcm16_native(std::span<const std::int16_t> mono_pcm,
                                         std::uint32_t sample_rate,
                                         const std::atomic_bool* cancel = nullptr,
                                         std::uint32_t timeout_seconds = 300);

}
