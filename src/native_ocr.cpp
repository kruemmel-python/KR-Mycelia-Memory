#include "native_ocr.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Pdf.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

namespace krmemory {namespace {
template<class T>struct Com {T* p{};~Com(){if(p)p->Release();}T** out(){return &p;}T* operator->()const{return p;}};
std::string utf8(std::wstring_view text){if(text.empty())return{};int n=WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),nullptr,0,nullptr,nullptr);std::string out(size_t(n),'\0');WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),out.data(),n,nullptr,nullptr);return out;}
struct Decoded {UINT width{},height{};std::vector<std::uint8_t> bgra;};
struct Apartment {
    Apartment(){winrt::init_apartment(winrt::apartment_type::multi_threaded);}
    ~Apartment(){winrt::uninit_apartment();CoFreeUnusedLibrariesEx(0,0);}
};
Decoded decode_wic(const std::filesystem::path& path,UINT maximum){
    Com<IWICImagingFactory> factory;winrt::check_hresult(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.out())));
    Com<IWICBitmapDecoder> decoder;winrt::check_hresult(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,decoder.out()));
    Com<IWICBitmapFrameDecode> frame;winrt::check_hresult(decoder->GetFrame(0,frame.out()));UINT width{},height{};winrt::check_hresult(frame->GetSize(&width,&height));if(!width||!height||width>32768||height>32768)throw std::runtime_error("invalid image dimensions");
    IWICBitmapSource* source=frame.p;Com<IWICBitmapScaler> scaler;if(width>maximum||height>maximum){double factor=std::min(double(maximum)/width,double(maximum)/height);UINT scaled_width=std::max(1u,UINT(width*factor)),scaled_height=std::max(1u,UINT(height*factor));winrt::check_hresult(factory->CreateBitmapScaler(scaler.out()));winrt::check_hresult(scaler->Initialize(frame.p,scaled_width,scaled_height,WICBitmapInterpolationModeFant));source=scaler.p;width=scaled_width;height=scaled_height;}
    Com<IWICFormatConverter> converter;winrt::check_hresult(factory->CreateFormatConverter(converter.out()));winrt::check_hresult(converter->Initialize(source,GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));if(std::size_t(width)>std::numeric_limits<std::size_t>::max()/height/4)throw std::runtime_error("image too large");Decoded decoded{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4)};winrt::check_hresult(converter->CopyPixels(nullptr,width*4,UINT(decoded.bgra.size()),decoded.bgra.data()));return decoded;
}
winrt::Windows::Media::Ocr::OcrEngine create_engine(std::string_view language_tag){using namespace winrt::Windows::Media::Ocr;using namespace winrt::Windows::Globalization;if(language_tag.empty())return OcrEngine::TryCreateFromUserProfileLanguages();auto language=Language(winrt::to_hstring(language_tag));return OcrEngine::IsLanguageSupported(language)?OcrEngine::TryCreateFromLanguage(language):nullptr;}
void append_result(OcrResult& out,const winrt::Windows::Media::Ocr::OcrResult& result,std::uint32_t page){auto text=utf8(result.Text().c_str());if(!text.empty()){if(!out.text.empty())out.text+="\n\n";out.text+="[Page "+std::to_string(page)+"]\n"+text;}auto lines=result.Lines();for(std::uint32_t index{};index<lines.Size();++index){auto line=lines.GetAt(index);OcrLine item;item.page=page;item.text=utf8(line.Text().c_str());auto words=line.Words();if(words.Size()){auto first=words.GetAt(0).BoundingRect();auto last=words.GetAt(words.Size()-1).BoundingRect();item.x=first.X;item.y=first.Y;item.width=last.X+last.Width-first.X;item.height=std::max(first.Height,last.Height);}out.lines.push_back(std::move(item));}}
}
OcrResult recognize_image_text(const std::filesystem::path& path,std::string language_tag){OcrResult out;try{Apartment apartment;using namespace winrt::Windows::Media::Ocr;using namespace winrt::Windows::Graphics::Imaging;using namespace winrt::Windows::Security::Cryptography;auto engine=create_engine(language_tag);out.available=engine!=nullptr;if(!engine){out.detail="Windows OCR has no installed supported language";return out;}auto decoded=decode_wic(path,OcrEngine::MaxImageDimension());auto buffer=CryptographicBuffer::CreateFromByteArray(decoded.bgra);auto bitmap=SoftwareBitmap::CreateCopyFromBuffer(buffer,BitmapPixelFormat::Bgra8,decoded.width,decoded.height,BitmapAlphaMode::Ignore);auto result=engine.RecognizeAsync(bitmap).get();out.language=utf8(engine.RecognizerLanguage().LanguageTag().c_str());append_result(out,result,1);out.success=true;out.detail=out.text.empty()?"Windows OCR completed without recognized text":"Windows OCR completed";return out;}catch(const winrt::hresult_error& error){out.detail="Windows OCR error: "+utf8(error.message().c_str());return out;}catch(const std::exception& error){out.detail=error.what();return out;}}
OcrResult recognize_bgra_text(std::span<const std::uint8_t> pixels,std::uint32_t width,std::uint32_t height,std::uint32_t stride,std::string language_tag){OcrResult out;try{if(!width||!height||stride<width*4||pixels.size()<std::size_t(stride)*height)throw std::invalid_argument("invalid BGRA image buffer");Apartment apartment;using namespace winrt::Windows::Media::Ocr;using namespace winrt::Windows::Graphics::Imaging;using namespace winrt::Windows::Security::Cryptography;auto engine=create_engine(language_tag);out.available=engine!=nullptr;if(!engine){out.detail="Windows OCR has no installed supported language";return out;}std::vector<std::uint8_t> packed(std::size_t(width)*height*4);for(std::uint32_t y{};y<height;++y)std::copy_n(pixels.data()+std::size_t(y)*stride,std::size_t(width)*4,packed.data()+std::size_t(y)*width*4);auto buffer=CryptographicBuffer::CreateFromByteArray(packed);auto bitmap=SoftwareBitmap::CreateCopyFromBuffer(buffer,BitmapPixelFormat::Bgra8,width,height,BitmapAlphaMode::Ignore);append_result(out,engine.RecognizeAsync(bitmap).get(),1);out.language=utf8(engine.RecognizerLanguage().LanguageTag().c_str());out.success=true;out.detail=out.text.empty()?"Windows OCR completed without recognized text":"Windows OCR completed";return out;}catch(const winrt::hresult_error& error){out.detail="Windows OCR error: "+utf8(error.message().c_str());return out;}catch(const std::exception& error){out.detail=error.what();return out;}}
OcrResult recognize_pdf_text(const std::filesystem::path& path, std::string language_tag,
                             std::uint32_t maximum_pages) {
    OcrResult out;
    try {
        if (!maximum_pages || maximum_pages > 1000)
            throw std::invalid_argument("invalid PDF page limit");
        Apartment apartment;
        using namespace winrt::Windows::Data::Pdf;
        using namespace winrt::Windows::Graphics::Imaging;
        using namespace winrt::Windows::Storage;
        using namespace winrt::Windows::Storage::Streams;
        auto engine = create_engine(language_tag);
        out.available = engine != nullptr;
        if (!engine) {
            out.detail = "Windows OCR has no installed supported language";
            return out;
        }
        auto file = StorageFile::GetFileFromPathAsync(path.wstring()).get();
        auto document = PdfDocument::LoadFromFileAsync(file).get();
        const auto count = std::min(document.PageCount(), maximum_pages);
        for (std::uint32_t index{}; index < count; ++index) {
            auto page = document.GetPage(index);
            InMemoryRandomAccessStream stream;
            PdfPageRenderOptions options;
            const auto size = page.Size();
            const double factor = size.Width > 2200 ? 2200.0 / size.Width : 1.0;
            options.DestinationWidth(std::max(1u, std::uint32_t(size.Width * factor)));
            options.DestinationHeight(std::max(1u, std::uint32_t(size.Height * factor)));
            page.RenderToStreamAsync(stream, options).get();
            stream.Seek(0);
            {
                auto decoder = BitmapDecoder::CreateAsync(stream).get();
                auto bitmap = decoder.GetSoftwareBitmapAsync(BitmapPixelFormat::Bgra8,
                                                              BitmapAlphaMode::Ignore).get();
                auto recognition = engine.RecognizeAsync(bitmap).get();
                append_result(out, recognition, index + 1);
                recognition = nullptr;
                bitmap.Close();
                bitmap = nullptr;
                decoder = nullptr;
            }
            stream.Close();
            stream = nullptr;
            page.Close();
            page = nullptr;
        }
        out.language = utf8(engine.RecognizerLanguage().LanguageTag().c_str());
        out.success = true;
        out.detail = "Windows PDF rendering and OCR completed for " + std::to_string(count) + " page(s)";
        return out;
    } catch (const winrt::hresult_error& error) {
        out.detail = "Windows PDF/OCR error: " + utf8(error.message().c_str());
        return out;
    } catch (const std::exception& error) {
        out.detail = error.what();
        return out;
    }
}
}
#else
namespace krmemory {OcrResult recognize_image_text(const std::filesystem::path&,std::string){return{false,false,{}, {},"Windows OCR is available on Windows only",{}};}OcrResult recognize_bgra_text(std::span<const std::uint8_t>,std::uint32_t,std::uint32_t,std::uint32_t,std::string){return{false,false,{}, {},"Windows OCR is available on Windows only",{}};}OcrResult recognize_pdf_text(const std::filesystem::path&,std::string,std::uint32_t){return{false,false,{}, {},"Windows PDF/OCR is available on Windows only",{}};}}
#endif
