#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>
#include "native_ocr.hpp"
#include <filesystem>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void release(IUnknown* value){if(value)value->Release();}
bool encode(IWICImagingFactory* factory,const GUID& container,const std::filesystem::path& path,const std::vector<unsigned char>& bgra,UINT width,UINT height){
    IWICBitmap* bitmap{};IWICStream* stream{};IWICBitmapEncoder* encoder{};IWICBitmapFrameEncode* frame{};IPropertyBag2* properties{};
    HRESULT hr=factory->CreateBitmapFromMemory(width,height,GUID_WICPixelFormat32bppBGRA,width*4,UINT(bgra.size()),const_cast<BYTE*>(bgra.data()),&bitmap);
    if(SUCCEEDED(hr))hr=factory->CreateStream(&stream);if(SUCCEEDED(hr))hr=stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE);if(SUCCEEDED(hr))hr=factory->CreateEncoder(container,nullptr,&encoder);if(SUCCEEDED(hr))hr=encoder->Initialize(stream,WICBitmapEncoderNoCache);if(SUCCEEDED(hr))hr=encoder->CreateNewFrame(&frame,&properties);if(SUCCEEDED(hr))hr=frame->Initialize(properties);if(SUCCEEDED(hr))hr=frame->SetSize(width,height);WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;if(SUCCEEDED(hr))hr=frame->SetPixelFormat(&format);if(SUCCEEDED(hr))hr=frame->WriteSource(bitmap,nullptr);if(SUCCEEDED(hr))hr=frame->Commit();if(SUCCEEDED(hr))hr=encoder->Commit();
    release(properties);release(frame);release(encoder);release(stream);release(bitmap);return SUCCEEDED(hr);
}
}

int main(){try{
    if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)))throw std::runtime_error("COM initialization failed");
    constexpr int width=1400,height=300;BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* pixels{};auto screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);auto bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);ReleaseDC(nullptr,screen);if(!dc||!bitmap||!pixels)throw std::runtime_error("DIB creation failed");
    auto old=SelectObject(dc,bitmap);RECT rect{0,0,width,height};FillRect(dc,&rect,(HBRUSH)GetStockObject(WHITE_BRUSH));SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(0,0,0));auto font=CreateFontW(96,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");auto old_font=SelectObject(dc,font);DrawTextW(dc,L"MYCELIA MEMORY OCR TEST 4827",-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE);GdiFlush();std::vector<unsigned char> bgra(width*height*4);std::memcpy(bgra.data(),pixels,bgra.size());
    SelectObject(dc,old_font);SelectObject(dc,old);DeleteObject(font);DeleteObject(bitmap);DeleteDC(dc);
    IWICImagingFactory* factory{};if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))throw std::runtime_error("WIC factory unavailable");
    struct Format{const wchar_t* extension;const GUID* container;bool required;};const Format formats[]={{L".bmp",&GUID_ContainerFormatBmp,true},{L".png",&GUID_ContainerFormatPng,true},{L".jpg",&GUID_ContainerFormatJpeg,true},{L".tiff",&GUID_ContainerFormatTiff,true},{L".webp",&GUID_ContainerFormatWebp,false}};
    unsigned tested{};for(const auto& format:formats){auto path=std::filesystem::temp_directory_path()/(std::wstring(L"kr-mycelia-native-ocr")+format.extension);if(!encode(factory,*format.container,path,bgra,width,height)){if(format.required)throw std::runtime_error("required WIC encoder unavailable");continue;}auto result=krmemory::recognize_image_text(path);std::filesystem::remove(path);if(!result.available)throw std::runtime_error("Windows OCR resources unavailable: "+result.detail);if(!result.success||result.text.find("MYCELIA")==std::string::npos||result.text.find("4827")==std::string::npos)throw std::runtime_error("unexpected OCR text for image format: "+result.text);if(result.lines.empty())throw std::runtime_error("OCR line geometry missing");++tested;}
    release(factory);CoUninitialize();if(tested<4)throw std::runtime_error("not all required image formats were tested");std::cout<<"PASS: native Windows OCR recognized real text in "<<tested<<" WIC image formats\n";return 0;
}catch(const std::exception& error){CoUninitialize();std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
