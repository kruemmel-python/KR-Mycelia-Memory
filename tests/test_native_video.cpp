#include "native_media_analysis.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <filesystem>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
template<class T> struct Com {T* p{};~Com(){if(p)p->Release();}T** out(){return &p;}T* operator->()const{return p;}};
void ok(HRESULT hr,const char* message){if(FAILED(hr))throw std::runtime_error(message+std::string(" HRESULT=")+std::to_string(static_cast<unsigned>(hr)));}
constexpr GUID majorKey{0x48eba18e,0xf8c9,0x4687,{0xbf,0x11,0x0a,0x74,0xc9,0xf9,0x6a,0x8f}};
constexpr GUID subtypeKey{0xf7e34c9a,0x42e8,0x4714,{0xb7,0x4b,0xcb,0x29,0xd7,0x2c,0x35,0xe5}};
constexpr GUID videoType{0x73646976,0,0x10,{0x80,0,0,0xaa,0,0x38,0x9b,0x71}};
constexpr GUID rgb32{0x16,0,0x10,{0x80,0,0,0xaa,0,0x38,0x9b,0x71}};
constexpr GUID wmv3{0x33564d57,0,0x10,{0x80,0,0,0xaa,0,0x38,0x9b,0x71}};
constexpr GUID frameSize{0x1652c33d,0xd6b2,0x4012,{0xb8,0x34,0x72,3,8,0x49,0xa3,0x7d}};
constexpr GUID frameRate{0xc459a2e8,0x3d2c,0x4e44,{0xb1,0x32,0xfe,0xe5,0x15,0x6c,0x7b,0xb0}};
constexpr GUID pixelAspect{0xc6376a1e,0x8d0a,0x4027,{0xbe,0x45,0x6d,0x9a,0x0a,0xd3,0x9b,0xb6}};
constexpr GUID interlace{0xe2724bb8,0xe676,0x4806,{0xb4,0xb2,0xa8,0xd6,0xef,0xb4,0x4c,0xcd}};
constexpr GUID bitrate{0x20332624,0xfb0d,0x4d9e,{0xbd,0x0d,0xcb,0xf6,0x78,0x6c,0x10,0x2e}};

std::vector<unsigned char> frame(std::wstring text,bool alternate){constexpr int width=640,height=360;BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;void* bits{};HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&bits,nullptr,0);ReleaseDC(nullptr,screen);auto old=SelectObject(dc,bitmap);RECT area{0,0,width,height};HBRUSH background=CreateSolidBrush(RGB(250,250,250));FillRect(dc,&area,background);DeleteObject(background);if(alternate){RECT left{0,0,width/2,height};HBRUSH dark=CreateSolidBrush(RGB(20,35,60));FillRect(dc,&left,dark);DeleteObject(dark);}HFONT font=CreateFontW(58,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");auto oldFont=SelectObject(dc,font);SetTextColor(dc,alternate?RGB(220,40,40):RGB(5,20,45));SetBkMode(dc,TRANSPARENT);DrawTextW(dc,text.data(),int(text.size()),&area,DT_CENTER|DT_VCENTER|DT_SINGLELINE);GdiFlush();std::vector<unsigned char> result(std::size_t(width)*height*4);std::memcpy(result.data(),bits,result.size());SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return result;}

void create_video(const std::filesystem::path& path){constexpr UINT32 width=640,height=360,fps=10;ok(MFStartup(MF_VERSION,MFSTARTUP_FULL),"MFStartup");Com<IMFSinkWriter> writer;ok(MFCreateSinkWriterFromURL(path.c_str(),nullptr,nullptr,writer.out()),"create sink writer");Com<IMFMediaType> output;ok(MFCreateMediaType(output.out()),"output type");ok(output->SetGUID(majorKey,videoType),"output major");ok(output->SetGUID(subtypeKey,wmv3),"output subtype");ok(output->SetUINT32(bitrate,900000),"bitrate");ok(output->SetUINT64(frameSize,(std::uint64_t(width)<<32)|height),"size");ok(output->SetUINT64(frameRate,(std::uint64_t(fps)<<32)|1),"rate");ok(output->SetUINT64(pixelAspect,(std::uint64_t(1)<<32)|1),"aspect");ok(output->SetUINT32(interlace,2),"interlace");DWORD stream{};ok(writer->AddStream(output.p,&stream),"add stream");Com<IMFMediaType> input;ok(MFCreateMediaType(input.out()),"input type");ok(input->SetGUID(majorKey,videoType),"input major");ok(input->SetGUID(subtypeKey,rgb32),"input subtype");ok(input->SetUINT64(frameSize,(std::uint64_t(width)<<32)|height),"input size");ok(input->SetUINT64(frameRate,(std::uint64_t(fps)<<32)|1),"input rate");ok(input->SetUINT64(pixelAspect,(std::uint64_t(1)<<32)|1),"input aspect");ok(input->SetUINT32(interlace,2),"input interlace");ok(writer->SetInputMediaType(stream,input.p,nullptr),"set input");ok(writer->BeginWriting(),"begin writing");auto first=frame(L"MYCELIA VIDEO OCR 314",false),second=frame(L"MYCELIA SECOND SCENE 927",true);for(UINT32 index{};index<30;++index){auto& pixels=index<15?first:second;Com<IMFMediaBuffer> buffer;ok(MFCreateMemoryBuffer(DWORD(pixels.size()),buffer.out()),"frame buffer");BYTE* target{};DWORD capacity{};ok(buffer->Lock(&target,&capacity,nullptr),"lock frame");std::memcpy(target,pixels.data(),pixels.size());buffer->Unlock();ok(buffer->SetCurrentLength(DWORD(pixels.size())),"frame length");Com<IMFSample> sample;ok(MFCreateSample(sample.out()),"sample");ok(sample->AddBuffer(buffer.p),"sample buffer");ok(sample->SetSampleTime(LONGLONG(index)*10000000/fps),"sample time");ok(sample->SetSampleDuration(10000000/fps),"sample duration");ok(writer->WriteSample(stream,sample.p),"write sample");}ok(writer->Finalize(),"finalize video");MFShutdown();}
}

int main(){try{CoInitializeEx(nullptr,COINIT_MULTITHREADED);auto path=std::filesystem::temp_directory_path()/"kr_mycelia_native_video.wmv";create_video(path);auto result=krmemory::analyze_video_native(path,nullptr,0.5,20);std::filesystem::remove(path);CoUninitialize();if(!result.decoded||result.frames.empty())throw std::runtime_error("native video frames missing");bool scene{},ocr{};for(auto& frame:result.frames){scene|=frame.scene_change&&frame.timestamp_seconds>0.5;ocr|=frame.recognized_text.find("MYCELIA")!=std::string::npos;}if(!scene)throw std::runtime_error("scene change was not detected");if(!ocr)throw std::runtime_error("video frame OCR did not recognize generated text");std::cout<<"PASS: native video decode, timestamps, keyframe sampling, scene detection, deduplication and OCR\n";return 0;}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
