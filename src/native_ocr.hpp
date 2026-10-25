#pragma once
#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>
#include <span>
namespace krmemory {
struct OcrLine { std::string text; std::uint32_t page{1}; double x{},y{},width{},height{}; };
struct OcrResult { bool available{},success{}; std::string text,language,detail; std::vector<OcrLine> lines; };
OcrResult recognize_image_text(const std::filesystem::path& path,std::string language_tag={});
OcrResult recognize_bgra_text(std::span<const std::uint8_t> pixels,std::uint32_t width,
                              std::uint32_t height,std::uint32_t stride,
                              std::string language_tag={});
OcrResult recognize_pdf_text(const std::filesystem::path& path,std::string language_tag={},std::uint32_t maximum_pages=100);
}
