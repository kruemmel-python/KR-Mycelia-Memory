#include "native_media_analysis.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void u16(std::ofstream& out,std::uint16_t value){out.put(char(value));out.put(char(value>>8));}
void u32(std::ofstream& out,std::uint32_t value){u16(out,std::uint16_t(value));u16(out,std::uint16_t(value>>16));}
void write_wave(const std::filesystem::path& path){constexpr std::uint32_t rate=8000,count=16000,data=count*2;std::ofstream out(path,std::ios::binary);out.write("RIFF",4);u32(out,36+data);out.write("WAVEfmt ",8);u32(out,16);u16(out,1);u16(out,1);u32(out,rate);u32(out,rate*2);u16(out,2);u16(out,16);out.write("data",4);u32(out,data);for(std::uint32_t i{};i<count;++i){auto value=std::int16_t(std::sin(2*3.141592653589793*440*i/rate)*12000);u16(out,std::uint16_t(value));}}
}
int main(){try{auto path=std::filesystem::temp_directory_path()/"kr_mycelia_native_audio.wav";write_wave(path);auto result=krmemory::decode_audio_native(path);std::filesystem::remove(path);if(!result.decoded||result.sample_rate!=16000)throw std::runtime_error("native audio decoding failed");if(result.mono_pcm.size()<31000||result.mono_pcm.size()>33000)throw std::runtime_error("native resampling did not produce two seconds of 16 kHz PCM");if(result.segments.empty()||result.segments.front().end_seconds<1.9||result.segments.front().rms<0.1)throw std::runtime_error("audio segmentation or level analysis failed");if(result.transcription_status.empty())throw std::runtime_error("transcription capability status missing");std::cout<<"PASS: native Media Foundation PCM decode, 16 kHz resampling, mono conversion and timed segmentation\n";return 0;}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
