#include "native_media_analysis.hpp"
#include "native_ocr.hpp"
#include "native_speech.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <span>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Media.SpeechRecognition.h>

namespace krmemory {
namespace {

template<class T> struct ComPtr {
    T* value{};
    ~ComPtr(){if(value)value->Release();}
    T** out(){return &value;}
    T* operator->()const{return value;}
};
struct ComApartment {HRESULT hr{CoInitializeEx(nullptr,COINIT_MULTITHREADED)};~ComApartment(){if(SUCCEEDED(hr))CoUninitialize();}};
struct MfSession {HRESULT hr{MFStartup(MF_VERSION,MFSTARTUP_FULL)};~MfSession(){if(SUCCEEDED(hr))MFShutdown();}};
void require(HRESULT hr,const char* message){if(FAILED(hr))throw std::runtime_error(message+std::string(" (HRESULT ")+std::to_string(static_cast<unsigned>(hr))+")");}
constexpr GUID kMajorType{0x48eba18e,0xf8c9,0x4687,{0xbf,0x11,0x0a,0x74,0xc9,0xf9,0x6a,0x8f}};
constexpr GUID kSubtype{0xf7e34c9a,0x42e8,0x4714,{0xb7,0x4b,0xcb,0x29,0xd7,0x2c,0x35,0xe5}};
constexpr GUID kAudio{0x73647561,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}};
constexpr GUID kPcm{0x00000001,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}};
constexpr GUID kChannels{0x37e48bf5,0x645e,0x4c5b,{0x89,0xde,0xad,0xa9,0xe2,0x9b,0x69,0x6a}};
constexpr GUID kSampleRate{0x5faeeae7,0x0290,0x4c31,{0x9e,0x8a,0xc5,0x34,0xf6,0x8d,0x9d,0xba}};
constexpr GUID kBits{0xf2deb57f,0x40fa,0x4764,{0xaa,0x33,0xed,0x4f,0x2d,0x1f,0xf6,0x69}};
constexpr GUID kBlock{0x322de230,0x9eeb,0x43bd,{0xab,0x7a,0xff,0x41,0x22,0x51,0x54,0x1d}};
constexpr GUID kByteRate{0x1aab75c8,0xcfef,0x451c,{0xab,0x95,0xac,0x03,0x4b,0x8e,0x17,0x31}};
constexpr GUID kVideo{0x73646976,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}};
constexpr GUID kRgb32{0x00000016,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}};
constexpr GUID kFrameSize{0x1652c33d,0xd6b2,0x4012,{0xb8,0x34,0x72,0x03,0x08,0x49,0xa3,0x7d}};

double rms(std::span<const std::int16_t> samples){if(samples.empty())return 0;long double sum{};for(auto sample:samples){auto normalized=static_cast<long double>(sample)/32768.0L;sum+=normalized*normalized;}return std::sqrt(static_cast<double>(sum/samples.size()));}

std::string speech_resource_status(){
    try{
        auto languages=winrt::Windows::Media::SpeechRecognition::SpeechRecognizer::SupportedTopicLanguages();std::string tags;
        for(const auto& language:languages){if(!tags.empty())tags+=",";tags+=winrt::to_string(language.LanguageTag());if(tags.size()>256){tags+="…";break;}}
        using PackageName=LONG(WINAPI*)(UINT32*,PWSTR);auto package_name=reinterpret_cast<PackageName>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"GetCurrentPackageFullName"));UINT32 package_length{};const bool packaged=package_name&&package_name(&package_length,nullptr)==ERROR_INSUFFICIENT_BUFFER;
        if(tags.empty())return "No Windows offline speech language resource was reported. The Win32 process is "+std::string(packaged?"packaged":"unpackaged")+"; no supported arbitrary-file PCM speech recognizer is available.";
        return "Windows speech language resource(s) detected: "+tags+". The process is "+std::string(packaged?"packaged":"unpackaged")+". Windows SpeechRecognizer accepts capture input but exposes no supported injection path for the decoded file PCM; no microphone or cloud fallback was used.";
    }catch(const winrt::hresult_error& error){return "Windows speech-resource probe failed (HRESULT "+std::to_string(static_cast<unsigned>(error.code().value))+ "); no transcript was simulated.";}
}

}

AudioAnalysis decode_audio_native(const std::filesystem::path& path,const std::atomic_bool* cancel,std::uint32_t maximum_minutes){
    if(!std::filesystem::is_regular_file(path))throw std::invalid_argument("audio path is not a regular file");
    if(!maximum_minutes||maximum_minutes>1440)throw std::invalid_argument("invalid audio duration limit");
    ComApartment apartment;MfSession mf;if(FAILED(mf.hr))throw std::runtime_error("Media Foundation is unavailable");
    ComPtr<IMFSourceReader> reader;require(MFCreateSourceReaderFromURL(path.c_str(),nullptr,reader.out()),"cannot open audio source");
    require(reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS,FALSE),"cannot clear source streams");
    require(reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM,TRUE),"audio stream is unavailable");
    ComPtr<IMFMediaType> type;require(MFCreateMediaType(type.out()),"cannot create PCM media type");
    require(type->SetGUID(kMajorType,kAudio),"cannot set audio major type");
    require(type->SetGUID(kSubtype,kPcm),"cannot set PCM subtype");
    require(type->SetUINT32(kChannels,1),"cannot set mono audio");
    require(type->SetUINT32(kSampleRate,16000),"cannot set sample rate");
    require(type->SetUINT32(kBits,16),"cannot set sample size");
    require(type->SetUINT32(kBlock,2),"cannot set block alignment");
    require(type->SetUINT32(kByteRate,32000),"cannot set audio rate");
    require(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,nullptr,type.value),"native decoder cannot produce 16 kHz mono PCM");
    AudioAnalysis result;const std::size_t sample_limit=std::size_t(maximum_minutes)*60*16000;
    while(result.mono_pcm.size()<sample_limit){
        if(cancel&&cancel->load()){result.cancelled=true;break;}
        DWORD stream{},flags{};LONGLONG timestamp{};ComPtr<IMFSample> sample;
        require(reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM,0,&stream,&flags,&timestamp,sample.out()),"audio decoding failed");
        if(flags&MF_SOURCE_READERF_ENDOFSTREAM)break;
        if(!sample.value)continue;
        ComPtr<IMFMediaBuffer> buffer;require(sample->ConvertToContiguousBuffer(buffer.out()),"cannot access decoded PCM");
        BYTE* bytes{};DWORD length{};require(buffer->Lock(&bytes,nullptr,&length),"cannot lock decoded PCM");
        const auto samples=std::min<std::size_t>(length/2,sample_limit-result.mono_pcm.size());
        const auto old=result.mono_pcm.size();result.mono_pcm.resize(old+samples);std::memcpy(result.mono_pcm.data()+old,bytes,samples*2);buffer->Unlock();
    }
    result.decoded=!result.mono_pcm.empty();
    constexpr std::size_t segment_samples=10*16000;
    for(std::size_t offset{};offset<result.mono_pcm.size();offset+=segment_samples){auto count=std::min(segment_samples,result.mono_pcm.size()-offset);AudioSegment segment;segment.start_seconds=double(offset)/16000.0;segment.end_seconds=double(offset+count)/16000.0;segment.rms=rms(std::span<const std::int16_t>(result.mono_pcm.data()+offset,count));result.segments.push_back(segment);}
    auto transcript=transcribe_pcm16_native(result.mono_pcm,result.sample_rate,cancel,std::min<std::uint32_t>(3600,maximum_minutes*60+30));
    result.transcription_engine=transcript.engine_available?transcript.engine:"Windows native speech capability probe";result.transcription_status=transcript.detail;
    for(const auto& piece:transcript.pieces){if(result.segments.empty())break;auto index=std::min<std::size_t>(result.segments.size()-1,std::size_t(std::max(0.0,piece.start_seconds)/10.0));auto& segment=result.segments[index];if(!segment.transcript.empty())segment.transcript+=' ';segment.transcript+=piece.text;}
    if(!transcript.engine_available)result.transcription_status+=" "+speech_resource_status();
    result.transcription_status+=" PCM decoding, 16 kHz resampling, mono conversion, segmentation and timestamps completed.";
    return result;
}

VideoAnalysis analyze_video_native(const std::filesystem::path& path,const std::atomic_bool* cancel,double interval,std::uint32_t maximum_frames){
    if(!std::filesystem::is_regular_file(path))throw std::invalid_argument("video path is not a regular file");
    if(interval<0.25||interval>3600||!maximum_frames||maximum_frames>1000)throw std::invalid_argument("invalid video analysis limits");
    ComApartment apartment;MfSession mf;if(FAILED(mf.hr))throw std::runtime_error("Media Foundation is unavailable");
    ComPtr<IMFSourceReader> reader;require(MFCreateSourceReaderFromURL(path.c_str(),nullptr,reader.out()),"cannot open video source");
    ComPtr<IMFMediaType> type;require(MFCreateMediaType(type.out()),"cannot create video media type");
    require(type->SetGUID(kMajorType,kVideo),"cannot set video major type");require(type->SetGUID(kSubtype,kRgb32),"cannot set RGB32 subtype");
    require(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,nullptr,type.value),"native decoder cannot produce RGB32 frames");
    ComPtr<IMFMediaType> current;require(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,current.out()),"cannot query decoded video type");
    UINT32 width{},height{};require(MFGetAttributeSize(current.value,kFrameSize,&width,&height),"video dimensions are unavailable");
    VideoAnalysis result;
    PROPVARIANT duration;PropVariantInit(&duration);if(SUCCEEDED(reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE,MF_PD_DURATION,&duration))){if(duration.vt==VT_UI8)result.duration_seconds=double(duration.uhVal.QuadPart)/10000000.0;else if(duration.vt==VT_I8&&duration.hVal.QuadPart>0)result.duration_seconds=double(duration.hVal.QuadPart)/10000000.0;}PropVariantClear(&duration);
    ComPtr<IMFMediaType> audio;result.has_audio=SUCCEEDED(reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,0,audio.out()));
    const double end=result.duration_seconds>0?result.duration_seconds:interval*maximum_frames;std::uint64_t previous{};bool has_previous{};
    for(std::uint32_t index{};index<maximum_frames&&double(index)*interval<=end;++index){
        if(cancel&&cancel->load()){result.cancelled=true;break;}
        const auto target=static_cast<LONGLONG>(double(index)*interval*10000000.0);PROPVARIANT position;PropVariantInit(&position);position.vt=VT_I8;position.hVal.QuadPart=target;require(reader->SetCurrentPosition(GUID_NULL,position),"cannot seek video");
        DWORD stream{},flags{};LONGLONG timestamp{};ComPtr<IMFSample> sample;for(;;){ComPtr<IMFSample> candidate;require(reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,&stream,&flags,&timestamp,candidate.out()),"video frame decoding failed");if(flags&MF_SOURCE_READERF_ENDOFSTREAM)break;if(!candidate.value)continue;if(timestamp+500000>=target){sample.value=candidate.value;candidate.value=nullptr;break;}}if(flags&MF_SOURCE_READERF_ENDOFSTREAM&&!sample.value)break;if(!sample.value)continue;
        ComPtr<IMFMediaBuffer> buffer;require(sample->ConvertToContiguousBuffer(buffer.out()),"cannot access video frame");BYTE* bytes{};DWORD length{};require(buffer->Lock(&bytes,nullptr,&length),"cannot lock video frame");const std::size_t stride=std::size_t(width)*4;if(length<stride*height){buffer->Unlock();throw std::runtime_error("decoded video frame is truncated");}
        std::uint64_t hash{};std::uint64_t sum{};std::array<unsigned,64> levels{};for(unsigned y{};y<8;++y)for(unsigned x{};x<8;++x){auto px=std::min(width-1,(x*2+1)*width/16),py=std::min(height-1,(y*2+1)*height/16);auto p=bytes+std::size_t(py)*stride+px*4;auto level=(unsigned(p[2])*77+unsigned(p[1])*150+unsigned(p[0])*29)>>8;levels[y*8+x]=level;sum+=level;}auto average=sum/64;for(unsigned bit{};bit<64;++bit)if(levels[bit]>=average)hash|=std::uint64_t(1)<<bit;
        unsigned changes=has_previous?std::popcount(hash^previous):64;double score=double(changes)/64.0;bool scene=!has_previous||score>=0.22;bool retain=scene||index==0||index%15==0;
        if(retain){VideoFrameAnalysis frame;frame.timestamp_seconds=double(timestamp)/10000000.0;frame.width=width;frame.height=height;frame.perceptual_hash=hash;frame.scene_score=has_previous?score:1.0;frame.scene_change=scene;auto ocr=recognize_bgra_text(std::span<const std::uint8_t>(bytes,std::size_t(length)),width,height,static_cast<std::uint32_t>(stride));if(ocr.success)frame.recognized_text=std::move(ocr.text);result.frames.push_back(std::move(frame));}
        previous=hash;has_previous=true;buffer->Unlock();
    }
    result.decoded=has_previous;result.detail="Native Media Foundation frame decoding, perceptual deduplication, scene detection and Windows OCR completed";return result;
}

}
#else
namespace krmemory {
AudioAnalysis decode_audio_native(const std::filesystem::path&,const std::atomic_bool*,std::uint32_t){throw std::runtime_error("native audio analysis is available on Windows only");}
VideoAnalysis analyze_video_native(const std::filesystem::path&,const std::atomic_bool*,double,std::uint32_t){throw std::runtime_error("native video analysis is available on Windows only");}
}
#endif
