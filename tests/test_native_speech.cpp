#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sapi.h>
#include <sphelper.h>
#include "native_media_analysis.hpp"
#include <atomic>
#include <filesystem>
#include <iostream>
#include <stdexcept>

template<class T>struct Ptr{T* p{};~Ptr(){if(p)p->Release();}T** out(){return&p;}T* operator->()const{return p;}};
void check(HRESULT result,const char* message){if(FAILED(result))throw std::runtime_error(message);}

int main(int argc,char** argv){try{
    check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"COM initialization failed");auto path=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/L"kr_mycelia_sapi_speech.wav";
    Ptr<ISpObjectToken> voice_token;Ptr<IEnumSpObjectTokens> voices;check(SpEnumTokens(SPCAT_VOICES,L"Language=407",nullptr,voices.out()),"German voice enumeration failed");if(voices->Item(0,voice_token.out())!=S_OK)throw std::runtime_error("no German SAPI voice installed");
    Ptr<ISpVoice> voice;check(CoCreateInstance(CLSID_SpVoice,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(voice.out())),"SAPI voice unavailable");check(voice->SetVoice(voice_token.p),"cannot select German voice");
    WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=1;format.nSamplesPerSec=16000;format.nAvgBytesPerSec=32000;format.nBlockAlign=2;format.wBitsPerSample=16;Ptr<ISpStream> stream;check(CoCreateInstance(CLSID_SpStream,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(stream.out())),"cannot create SAPI output stream");check(stream->BindToFile(path.c_str(),SPFM_CREATE_ALWAYS,&SPDFID_WaveFormatEx,&format,0),"cannot bind SAPI wave output");check(voice->SetOutput(stream.p,TRUE),"cannot route SAPI voice");check(voice->Speak(L"Dies ist ein Test der Spracherkennung im Myzel Gedächtnis.",SPF_DEFAULT,nullptr),"speech synthesis failed");voice->SetOutput(nullptr,TRUE);stream->Close();
    if(argc>1){CoUninitialize();std::cout<<"PASS: native SAPI speech fixture created: "<<path.string()<<'\n';return 0;}
    auto result=krmemory::decode_audio_native(path);std::string transcript;for(const auto& segment:result.segments){if(!segment.transcript.empty()){if(!transcript.empty())transcript+=' ';transcript+=segment.transcript;}}
    if(!result.decoded||result.transcription_engine.empty())throw std::runtime_error("native audio pipeline did not run");if(transcript.empty())throw std::runtime_error("installed SAPI recognizer produced no transcript: "+result.transcription_status);if(transcript.find("Test")==std::string::npos&&transcript.find("test")==std::string::npos)throw std::runtime_error("unexpected SAPI transcript: "+transcript);
    std::atomic_bool cancel{true};auto cancelled=krmemory::decode_audio_native(path,&cancel);if(!cancelled.cancelled)throw std::runtime_error("native audio cancellation was not propagated");std::filesystem::remove(path);
    CoUninitialize();std::cout<<"PASS: real native SAPI offline file transcription: "<<transcript<<'\n';return 0;
}catch(const std::exception& error){CoUninitialize();std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
