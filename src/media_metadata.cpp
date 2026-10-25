#include "media_metadata.hpp"
#include <fstream>
#include <stdexcept>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wincodec.h>
#endif

namespace krmemory {
namespace {

std::uint32_t be(const unsigned char* p) {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
           (std::uint32_t(p[2]) << 8) | p[3];
}

#ifdef _WIN32
template<class T> struct ComPtr {
    T* value{};
    ~ComPtr() { if (value) value->Release(); }
    T** out() { return &value; }
    T* operator->() const { return value; }
};

struct ComApartment {
    HRESULT result{CoInitializeEx(nullptr, COINIT_MULTITHREADED)};
    ~ComApartment() { if (SUCCEEDED(result)) CoUninitialize(); }
};

struct MfSession {
    HRESULT result{MFStartup(MF_VERSION, MFSTARTUP_LITE)};
    ~MfSession() { if (SUCCEEDED(result)) MFShutdown(); }
};

void inspect_wic(const std::filesystem::path& path, MediaMetadata& metadata) {
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(factory.out())))) return;
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                  WICDecodeMetadataCacheOnDemand, decoder.out()))) return;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, frame.out()))) return;
    UINT width{}, height{};
    if (SUCCEEDED(frame->GetSize(&width, &height))) {
        metadata.width = width;
        metadata.height = height;
    }
}

void inspect_media_foundation(const std::filesystem::path& path, MediaMetadata& metadata) {
    static constexpr GUID frame_size_key{0x1652c33d, 0xd6b2, 0x4012,
                                         {0xb8, 0x34, 0x72, 0x03, 0x08, 0x49, 0xa3, 0x7d}};
    MfSession mf;
    if (FAILED(mf.result)) return;
    ComPtr<IMFSourceReader> reader;
    if (FAILED(MFCreateSourceReaderFromURL(path.c_str(), nullptr, reader.out()))) return;

    PROPVARIANT duration;
    PropVariantInit(&duration);
    if (SUCCEEDED(reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &duration))) {
        ULONGLONG value{};
        if (duration.vt == VT_UI8) value = duration.uhVal.QuadPart;
        else if (duration.vt == VT_I8 && duration.hVal.QuadPart > 0)
            value = static_cast<ULONGLONG>(duration.hVal.QuadPart);
        if (value)
            metadata.duration_seconds = static_cast<double>(value) / 10000000.0;
    }
    PropVariantClear(&duration);

    ComPtr<IMFMediaType> video;
    if (SUCCEEDED(reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, video.out()))) {
        UINT32 width{}, height{};
        if (SUCCEEDED(MFGetAttributeSize(video.value, frame_size_key, &width, &height))) {
            metadata.width = width;
            metadata.height = height;
        }
        metadata.has_video = true;
    }
    ComPtr<IMFMediaType> audio;
    if (SUCCEEDED(reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, audio.out())))
        metadata.has_audio = true;
}
#endif

}

MediaMetadata inspect_media(const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path))
        throw std::invalid_argument("media path is not a regular file");
    MediaMetadata metadata;
    metadata.bytes = std::filesystem::file_size(path);
    std::ifstream input(path, std::ios::binary);
    unsigned char header[32]{};
    input.read(reinterpret_cast<char*>(header), sizeof(header));
    const auto count = input.gcount();
    if (count >= 24 && std::string_view(reinterpret_cast<char*>(header), 8) == "\x89PNG\r\n\x1a\n") {
        metadata.format = "png";
        metadata.width = be(header + 16);
        metadata.height = be(header + 20);
    } else if (count >= 12 && std::string_view(reinterpret_cast<char*>(header), 4) == "RIFF" &&
               std::string_view(reinterpret_cast<char*>(header + 8), 4) == "WAVE") {
        metadata.format = "wav";
        metadata.has_audio = true;
    } else if (count >= 12 && std::string_view(reinterpret_cast<char*>(header + 4), 4) == "ftyp") {
        metadata.format = "iso_base_media";
    } else if (count >= 2 && header[0] == 0xff && header[1] == 0xd8) {
        metadata.format = "jpeg";
    } else {
        metadata.format = "unknown";
    }
#ifdef _WIN32
    ComApartment apartment;
    inspect_wic(path, metadata);
    inspect_media_foundation(path, metadata);
#endif
    return metadata;
}

}
