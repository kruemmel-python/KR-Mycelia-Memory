#include "local_ai.hpp"
#include "json_value.hpp"
#include "sse_parser.hpp"
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#endif

namespace krmemory {
namespace {
bool loopback(std::string_view host){return host=="127.0.0.1"||host=="localhost"||host=="::1";}
std::string escape(std::string_view in){std::string out;for(unsigned char c:in){switch(c){case '"':out+="\\\"";break;case '\\':out+="\\\\";break;case '\n':out+="\\n";break;case '\r':out+="\\r";break;case '\t':out+="\\t";break;default:if(c>=32)out+=char(c);}}return out;}
#ifdef _WIN32
std::filesystem::path model_setting_path(){wchar_t* local{};if(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local)!=S_OK)return{};std::filesystem::path path=std::filesystem::path(local)/L"KRMyceliaNativeMemory"/L"lmstudio.model";CoTaskMemFree(local);return path;}
struct HttpResult { bool sent{}; DWORD status{}; std::string body; };
HttpResult request(std::string_view method,std::string_view path,std::string_view payload,std::string_view host,unsigned short port,unsigned timeout_ms=5000){
    HttpResult result;auto session=WinHttpOpen(L"KRMyceliaMemory/1.0",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!session)return result;
    std::wstring wide_host(host.begin(),host.end()),wide_method(method.begin(),method.end()),wide_path(path.begin(),path.end());auto connect=WinHttpConnect(session,wide_host.c_str(),port,0);if(!connect){WinHttpCloseHandle(session);return result;}
    auto handle=WinHttpOpenRequest(connect,wide_method.c_str(),wide_path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,0);if(handle){WinHttpSetTimeouts(handle,timeout_ms,timeout_ms,timeout_ms,timeout_ms);const wchar_t* headers=L"Content-Type: application/json\r\n";result.sent=WinHttpSendRequest(handle,headers,-1L,payload.empty()?WINHTTP_NO_REQUEST_DATA:(LPVOID)payload.data(),static_cast<DWORD>(payload.size()),static_cast<DWORD>(payload.size()),0)&&WinHttpReceiveResponse(handle,nullptr);DWORD size=sizeof(result.status);if(result.sent)WinHttpQueryHeaders(handle,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&result.status,&size,WINHTTP_NO_HEADER_INDEX);while(result.sent&&result.body.size()<4*1024*1024){DWORD available{};if(!WinHttpQueryDataAvailable(handle,&available)||!available)break;available=static_cast<DWORD>(std::min<std::size_t>(available,4*1024*1024-result.body.size()));auto old=result.body.size();result.body.resize(old+available);DWORD read{};if(!WinHttpReadData(handle,result.body.data()+old,available,&read)){result.body.clear();break;}result.body.resize(old+read);}}if(handle)WinHttpCloseHandle(handle);WinHttpCloseHandle(connect);WinHttpCloseHandle(session);return result;
}
#endif
}
std::string preferred_local_model(){
#ifdef _WIN32
    auto path=model_setting_path();std::ifstream input(path,std::ios::binary);std::string model;if(input)std::getline(input,model);if(model.size()>512||std::any_of(model.begin(),model.end(),[](unsigned char c){return c<32;}))return{};return model;
#else
    return{};
#endif
}
bool save_preferred_local_model(std::string_view model){
#ifdef _WIN32
    if(model.empty()||model.size()>512||std::any_of(model.begin(),model.end(),[](unsigned char c){return c<32;}))return false;auto path=model_setting_path();if(path.empty())return false;std::error_code error;std::filesystem::create_directories(path.parent_path(),error);if(error)return false;auto temp=path;temp+=L".tmp";{std::ofstream output(temp,std::ios::binary|std::ios::trunc);output.write(model.data(),std::streamsize(model.size()));output<<'\n';if(!output)return false;}if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){std::filesystem::remove(temp,error);return false;}return true;
#else
    (void)model;return false;
#endif
}
LocalModelStatus probe_lm_studio(std::string host,unsigned short port){
#ifdef _WIN32
    if(!loopback(host))return{false,"Only loopback local AI endpoints are allowed"};auto result=request("GET","/v1/models","",host,port);return{result.sent&&result.status==200,result.sent&&result.status==200?"LM Studio model API available":"LM Studio model API returned an error"};
#else
    (void)host;(void)port;return{false,"Local AI probing is available on Windows only"};
#endif
}
LocalModelList list_lm_studio_models(std::string host,unsigned short port,unsigned timeout_ms){
#ifdef _WIN32
    if(!loopback(host))return{false,{},"Only loopback local AI endpoints are allowed"};auto result=request("GET","/v1/models","",host,port,timeout_ms);if(!result.sent||result.status!=200)return{false,{},"LM Studio is not reachable"};
    try{auto root=json::parse(result.body);auto data=root.get("data");if(!data||!data->array())throw std::runtime_error("missing data");LocalModelList out{true,{},"Models discovered"};for(const auto& item:*data->array()){auto id=item.get("id");if(id&&id->string()&&!id->string()->empty())out.models.push_back(*id->string());}return out;}catch(...){return{false,{},"LM Studio returned an invalid model list"};}
#else
    (void)host;(void)port;(void)timeout_ms;return{false,{},"Local AI probing is available on Windows only"};
#endif
}
LocalCompletion complete_lm_studio(std::string_view prompt,std::string_view model,std::string host,unsigned short port,unsigned timeout_ms){
    if(prompt.empty()||prompt.size()>32768)return{false,{},"Prompt must be 1..32768 bytes"};
#ifdef _WIN32
    if(!loopback(host))return{false,{},"Only loopback local AI endpoints are allowed"};if(timeout_ms<1000||timeout_ms>300000)return{false,{},"Timeout must be 1000..300000 ms"};std::string body="{\"messages\":[{\"role\":\"user\",\"content\":\""+escape(prompt)+"\"}],\"temperature\":0.2";if(!model.empty())body+=",\"model\":\""+escape(model)+"\"";body+="}";auto result=request("POST","/v1/chat/completions",body,host,port,timeout_ms);if(!result.sent||result.status<200||result.status>=300)return{false,{},"Local AI completion endpoint returned an error"};try{auto root=json::parse(result.body);auto choices=root.get("choices");if(!choices||!choices->array()||choices->array()->empty())throw std::runtime_error("missing choices");auto message=choices->array()->front().get("message");auto text=message?message->get("content"):nullptr;if(!text||!text->string())throw std::runtime_error("missing completion text");return{true,*text->string(),"Local completion received"};}catch(...){return{false,{},"Local AI returned an invalid completion response"};}
#else
    (void)model;(void)host;(void)port;return{false,{},"Local AI completion is available on Windows only"};
#endif
}

struct LocalStreamRequest::Impl : std::enable_shared_from_this<LocalStreamRequest::Impl> {
    std::string prompt,model;Callback callback;unsigned timeout{};std::size_t maximum{};
    std::atomic_bool running{true},cancelled{};std::mutex handle_mutex;
#ifdef _WIN32
    HINTERNET request_handle{};
#endif
    void emit(LocalStreamEvent event){try{callback(event);}catch(...){} }
    void finish(LocalStreamEnd end,std::string detail){if(!running.exchange(false))return;emit({{},std::move(detail),end});}
    void cancel(){cancelled=true;
#ifdef _WIN32
        HINTERNET handle{};{std::lock_guard lock(handle_mutex);handle=request_handle;request_handle=nullptr;}if(handle)WinHttpCloseHandle(handle);
#endif
    }
    void run(){
#ifdef _WIN32
        auto session=WinHttpOpen(L"KRMyceliaMemory/2.0",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!session){finish(LocalStreamEnd::network_error,"WinHTTP session could not be opened");return;}
        auto connection=WinHttpConnect(session,L"127.0.0.1",1234,0);if(!connection){WinHttpCloseHandle(session);finish(LocalStreamEnd::network_error,"LM Studio connection could not be opened");return;}
        auto request=WinHttpOpenRequest(connection,L"POST",L"/v1/chat/completions",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,0);if(!request){WinHttpCloseHandle(connection);WinHttpCloseHandle(session);finish(LocalStreamEnd::network_error,"LM Studio request could not be opened");return;}
        {std::lock_guard lock(handle_mutex);if(cancelled){WinHttpCloseHandle(request);request=nullptr;}else request_handle=request;}
        if(!request){WinHttpCloseHandle(connection);WinHttpCloseHandle(session);finish(LocalStreamEnd::cancelled,"Cancelled by user");return;}
        WinHttpSetTimeouts(request,timeout,timeout,timeout,timeout);std::string body="{\"messages\":[{\"role\":\"user\",\"content\":\""+escape(prompt)+"\"}],\"temperature\":0.2,\"stream\":true,\"max_tokens\":"+std::to_string(local_stream_maximum_tokens);if(!model.empty())body+=",\"model\":\""+escape(model)+"\"";body+="}";const wchar_t* headers=L"Content-Type: application/json\r\nAccept: text/event-stream\r\n";
        bool sent=WinHttpSendRequest(request,headers,-1L,body.data(),DWORD(body.size()),DWORD(body.size()),0)&&WinHttpReceiveResponse(request,nullptr);DWORD error=sent?ERROR_SUCCESS:GetLastError(),status{},status_size=sizeof(status);if(sent)WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&status_size,WINHTTP_NO_HEADER_INDEX);
        if(!sent||status<200||status>=300){bool was_cancelled=cancelled;{std::lock_guard lock(handle_mutex);if(request_handle==request){request_handle=nullptr;WinHttpCloseHandle(request);}}WinHttpCloseHandle(connection);WinHttpCloseHandle(session);finish(was_cancelled?LocalStreamEnd::cancelled:(error==ERROR_WINHTTP_TIMEOUT?LocalStreamEnd::timeout:status?LocalStreamEnd::model_error:LocalStreamEnd::network_error),was_cancelled?"Cancelled by user":error==ERROR_WINHTTP_TIMEOUT?"LM Studio request timed out":"LM Studio streaming request failed");return;}
        SseParser parser;std::size_t total{};bool parse_error{};while(!cancelled){DWORD available{};if(!WinHttpQueryDataAvailable(request,&available)){error=GetLastError();break;}if(!available){auto result=parser.finish([&](std::string_view delta){emit({std::string(delta),{},std::nullopt});});if(result==SseParser::FeedResult::error)parse_error=true;else if(result==SseParser::FeedResult::done)error=ERROR_SUCCESS;break;}if(total+available>maximum){error=ERROR_INSUFFICIENT_BUFFER;break;}std::string chunk(available,'\0');DWORD read{};if(!WinHttpReadData(request,chunk.data(),available,&read)){error=GetLastError();break;}chunk.resize(read);total+=read;auto result=parser.feed(chunk,[&](std::string_view delta){emit({std::string(delta),{},std::nullopt});});if(result==SseParser::FeedResult::error){parse_error=true;break;}if(result==SseParser::FeedResult::done){error=ERROR_SUCCESS;break;}}
        {std::lock_guard lock(handle_mutex);if(request_handle==request){request_handle=nullptr;WinHttpCloseHandle(request);}}WinHttpCloseHandle(connection);WinHttpCloseHandle(session);
        if(cancelled)finish(LocalStreamEnd::cancelled,"Cancelled by user");else if(error==ERROR_WINHTTP_TIMEOUT)finish(LocalStreamEnd::timeout,"LM Studio request timed out");else if(error==ERROR_INSUFFICIENT_BUFFER)finish(LocalStreamEnd::response_too_large,"Streaming response exceeded the configured limit");else if(parse_error)finish(LocalStreamEnd::model_error,"Invalid server-sent event: "+parser.error());else if(error!=ERROR_SUCCESS)finish(LocalStreamEnd::network_error,"LM Studio network read failed");else finish(LocalStreamEnd::completed,"Streaming completion received");
#else
        finish(LocalStreamEnd::network_error,"Streaming is available on Windows only");
#endif
    }
};
LocalStreamRequest::LocalStreamRequest(std::shared_ptr<Impl> impl):impl_(std::move(impl)){}
LocalStreamRequest::~LocalStreamRequest(){if(impl_)impl_->cancel();}
std::shared_ptr<LocalStreamRequest> LocalStreamRequest::start(std::string prompt,std::string model,Callback callback,unsigned timeout_ms,std::size_t maximum_bytes){if(prompt.empty()||prompt.size()>32768||timeout_ms<1000||timeout_ms>300000||maximum_bytes<1024||maximum_bytes>4*1024*1024||!callback)return{};auto impl=std::make_shared<Impl>();impl->prompt=std::move(prompt);impl->model=std::move(model);impl->callback=std::move(callback);impl->timeout=timeout_ms;impl->maximum=maximum_bytes;auto request=std::shared_ptr<LocalStreamRequest>(new LocalStreamRequest(impl));std::thread([impl]{impl->run();}).detach();return request;}
void LocalStreamRequest::cancel(){if(impl_)impl_->cancel();}
bool LocalStreamRequest::active()const{return impl_&&impl_->running;}
}
