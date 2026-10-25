#include "native_speech.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sapi.h>
#include <sphelper.h>
#include <objidl.h>

namespace krmemory { namespace {
template<class T> struct ComPtr {T* value{};ComPtr()=default;ComPtr(const ComPtr&)=delete;ComPtr& operator=(const ComPtr&)=delete;ComPtr(ComPtr&& other)noexcept:value(other.value){other.value=nullptr;}ComPtr& operator=(ComPtr&& other)noexcept{if(this!=&other){if(value)value->Release();value=other.value;other.value=nullptr;}return *this;}~ComPtr(){if(value)value->Release();}T** out(){return &value;}T* operator->()const{return value;}};
std::string utf8(std::wstring_view value){if(value.empty())return{};int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);if(n<=0)return{};std::string out(std::size_t(n),'\0');WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),out.data(),n,nullptr,nullptr);return out;}
std::wstring language_hex(LANGID language){std::wostringstream out;out<<std::hex<<std::setw(4)<<std::setfill(L'0')<<unsigned(language);return out.str();}
std::string locale_name(std::wstring_view language){wchar_t* end{};auto id=LANGID(std::wcstoul(std::wstring(language).c_str(),&end,16));wchar_t locale[LOCALE_NAME_MAX_LENGTH]{};return id&&LCIDToLocaleName(MAKELCID(id,SORT_DEFAULT),locale,LOCALE_NAME_MAX_LENGTH,0)?utf8(locale):utf8(language);}
void clear_event(SPEVENT& event){if(!event.lParam)return;if(event.elParamType==SPET_LPARAM_IS_OBJECT||event.elParamType==SPET_LPARAM_IS_TOKEN)reinterpret_cast<IUnknown*>(event.lParam)->Release();else if(event.elParamType==SPET_LPARAM_IS_POINTER||event.elParamType==SPET_LPARAM_IS_STRING)CoTaskMemFree(reinterpret_cast<void*>(event.lParam));event.lParam=0;}

ComPtr<ISpObjectToken> recognizer_token(std::string& language,std::string& description){
    ComPtr<IEnumSpObjectTokens> tokens;if(FAILED(SpEnumTokens(SPCAT_RECOGNIZERS,nullptr,nullptr,tokens.out())))return{};ULONG count{};tokens->GetCount(&count);ISpObjectToken* fallback{};auto desired=language_hex(GetUserDefaultUILanguage());
    for(ULONG index{};index<count;++index){ISpObjectToken* token{};if(tokens->Item(index,&token)!=S_OK)continue;wchar_t* lang{};token->GetStringValue(L"Language",&lang);std::wstring current=lang?lang:L"";if(lang)CoTaskMemFree(lang);if(!fallback){fallback=token;fallback->AddRef();}if(_wcsicmp(current.c_str(),desired.c_str())==0){if(fallback)fallback->Release();fallback=token;fallback->AddRef();token->Release();break;}token->Release();}
    ComPtr<ISpObjectToken> selected;selected.value=fallback;if(!selected.value)return selected;wchar_t* lang{};if(SUCCEEDED(selected->GetStringValue(L"Language",&lang))&&lang){language=locale_name(lang);CoTaskMemFree(lang);}wchar_t* label{};if(SUCCEEDED(SpGetDescription(selected.value,&label))&&label){description=utf8(label);CoTaskMemFree(label);}return selected;
}
}

SpeechTranscript transcribe_pcm16_native(std::span<const std::int16_t> pcm,std::uint32_t sample_rate,const std::atomic_bool* cancel,std::uint32_t timeout_seconds){
    SpeechTranscript output;if(pcm.empty()){output.detail="No PCM samples were supplied.";return output;}if(sample_rate<8000||sample_rate>48000||timeout_seconds==0||timeout_seconds>3600){output.detail="Invalid native speech input limits.";return output;}
    HRESULT apartment=CoInitializeEx(nullptr,COINIT_MULTITHREADED);const bool uninitialize=SUCCEEDED(apartment);if(FAILED(apartment)&&apartment!=RPC_E_CHANGED_MODE){output.detail="COM initialization failed.";return output;}
    try{
        std::string description;auto token=recognizer_token(output.language,description);if(!token.value){output.detail="No Windows desktop SAPI speech recognizer is installed.";if(uninitialize)CoUninitialize();return output;}output.engine_available=true;output.engine=description.empty()?"Windows SAPI in-process recognizer":description;
        ComPtr<IStream> memory;if(FAILED(CreateStreamOnHGlobal(nullptr,TRUE,memory.out())))throw std::runtime_error("cannot allocate speech stream");ULONG written{};if(FAILED(memory->Write(pcm.data(),ULONG(pcm.size_bytes()),&written))||written!=pcm.size_bytes())throw std::runtime_error("cannot write speech PCM stream");LARGE_INTEGER zero{};memory->Seek(zero,STREAM_SEEK_SET,nullptr);
        WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=1;format.nSamplesPerSec=sample_rate;format.wBitsPerSample=16;format.nBlockAlign=2;format.nAvgBytesPerSec=sample_rate*2;format.cbSize=0;
        ComPtr<ISpStream> speech_stream;if(FAILED(CoCreateInstance(CLSID_SpStream,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(speech_stream.out())))||FAILED(speech_stream->SetBaseStream(memory.value,SPDFID_WaveFormatEx,&format)))throw std::runtime_error("cannot create SAPI PCM stream");
        ComPtr<ISpRecognizer> recognizer;if(FAILED(CoCreateInstance(CLSID_SpInprocRecognizer,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(recognizer.out()))))throw std::runtime_error("cannot create SAPI recognizer");if(FAILED(recognizer->SetRecognizer(token.value))||FAILED(recognizer->SetInput(speech_stream.value,TRUE)))throw std::runtime_error("cannot configure SAPI recognizer");
        ComPtr<ISpRecoContext> context;if(FAILED(recognizer->CreateRecoContext(context.out())))throw std::runtime_error("cannot create recognition context");if(FAILED(context->SetNotifyWin32Event()))throw std::runtime_error("cannot create speech notification event");const ULONGLONG interest=SPFEI(SPEI_RECOGNITION)|SPFEI(SPEI_FALSE_RECOGNITION)|SPFEI(SPEI_END_SR_STREAM);context->SetInterest(interest,interest);
        ComPtr<ISpRecoGrammar> grammar;if(FAILED(context->CreateGrammar(1,grammar.out()))||FAILED(grammar->LoadDictation(nullptr,SPLO_STATIC))||FAILED(grammar->SetDictationState(SPRS_ACTIVE)))throw std::runtime_error("cannot activate SAPI dictation");recognizer->SetRecoState(SPRST_ACTIVE_ALWAYS);
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(timeout_seconds);bool ended{};
        while(!ended&&std::chrono::steady_clock::now()<deadline){if(cancel&&cancel->load()){output.cancelled=true;break;}context->WaitForNotifyEvent(100);SPEVENT event{};ULONG fetched{};while(context->GetEvents(1,&event,&fetched)==S_OK&&fetched){if(event.eEventId==SPEI_END_SR_STREAM)ended=true;else if(event.eEventId==SPEI_RECOGNITION&&event.elParamType==SPET_LPARAM_IS_OBJECT){auto* result=reinterpret_cast<ISpRecoResult*>(event.lParam);wchar_t* recognized{};if(SUCCEEDED(result->GetText(0,SP_GETWHOLEPHRASE,TRUE,&recognized,nullptr))&&recognized){SpeechPiece piece;piece.text=utf8(recognized);CoTaskMemFree(recognized);SPPHRASE* phrase{};if(SUCCEEDED(result->GetPhrase(&phrase))&&phrase){piece.start_seconds=double(phrase->ullAudioStreamPosition)/10000000.0;piece.end_seconds=piece.start_seconds+double(phrase->ulAudioSizeTime)/10000000.0;if(phrase->Rule.ulCountOfElements&&phrase->pElements)piece.confidence=phrase->pElements[0].SREngineConfidence;CoTaskMemFree(phrase);}if(!piece.text.empty())output.pieces.push_back(std::move(piece));}}clear_event(event);}}
        recognizer->SetRecoState(SPRST_INACTIVE);grammar->SetDictationState(SPRS_INACTIVE);if(output.cancelled)output.detail="Windows SAPI transcription cancelled.";else if(!ended)output.detail="Windows SAPI transcription timed out.";else if(output.pieces.empty())output.detail="Windows SAPI completed but recognized no speech.";else{output.success=true;output.detail="Windows SAPI offline file transcription completed with "+std::to_string(output.pieces.size())+" phrase(s).";}
    }catch(const std::exception& error){output.detail=error.what();}
    if(uninitialize)CoUninitialize();return output;
}
}
#else
namespace krmemory {SpeechTranscript transcribe_pcm16_native(std::span<const std::int16_t>,std::uint32_t,const std::atomic_bool*,std::uint32_t){SpeechTranscript result;result.detail="Windows SAPI is available on Windows only.";return result;}}
#endif
