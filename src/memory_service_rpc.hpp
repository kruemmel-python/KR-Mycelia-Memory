#pragma once
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <wincrypt.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include "json_value.hpp"

namespace krmemory::service_rpc {
inline constexpr wchar_t pipe_name[] = L"\\\\.\\pipe\\KRMyceliaMemoryService-v1";
inline std::string quote(std::string_view text) { std::string out{"\""}; for(unsigned char c:text){if(c=='\"'||c=='\\'){out+='\\';out+=char(c);}else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if(c=='\t')out+="\\t";else if(c>=32)out+=char(c);}return out+'\"'; }
inline bool response_ok(std::string_view response,std::string_view expected_id){
    if(response.empty()||response.size()>65536)return false;
    try{
        const auto root=json::parse(response);if(!root.is_object())return false;
        const auto* version=root.get("version"),*id=root.get("id"),*ok=root.get("ok");
        if(!version||!std::holds_alternative<double>(version->data)||std::get<double>(version->data)!=1.0)return false;
        if(!id||!id->string()||*id->string()!=expected_id)return false;
        if(!ok||!std::holds_alternative<bool>(ok->data)||!std::get<bool>(ok->data))return false;
        return root.get("error")==nullptr;
    }catch(...){return false;}
}
inline std::optional<std::string> local_capability(){
    wchar_t* local{};if(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local)!=S_OK)return{};
    const auto path=std::filesystem::path(local)/L"KRMyceliaNativeMemory"/L"rpc.capability.dpapi";CoTaskMemFree(local);
    std::ifstream in(path,std::ios::binary);if(!in)return{};std::string encrypted((std::istreambuf_iterator<char>(in)),{});if(encrypted.empty()||encrypted.size()>4096)return{};
    DATA_BLOB source{DWORD(encrypted.size()),reinterpret_cast<BYTE*>(encrypted.data())},plain{};
    if(!CryptUnprotectData(&source,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&plain))return{};
    std::string secret(reinterpret_cast<char*>(plain.pbData),plain.cbData);LocalFree(plain.pbData);if(secret.size()!=64)return{};return secret;
}
inline bool transfer(std::string_view request,std::string_view id,std::string& response,DWORD timeout){
    if(request.size()>65536||!WaitNamedPipeW(pipe_name,timeout))return false;HANDLE pipe=CreateFileW(pipe_name,GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);if(pipe==INVALID_HANDLE_VALUE)return false;
    DWORD written{},read{};response.assign(65536,'\0');bool done=WriteFile(pipe,request.data(),DWORD(request.size()),&written,nullptr)!=FALSE&&written==request.size()&&ReadFile(pipe,response.data(),DWORD(response.size()),&read,nullptr)!=FALSE;CloseHandle(pipe);if(!done){response.clear();return false;}response.resize(read);return response_ok(response,id);
}
inline bool call(std::string_view id, std::string_view operation, std::string_view params, std::string& response, DWORD timeout=1000) {
    if(id.empty()||id.size()>128||operation.empty()||operation.size()>128||params.size()>58000)return false;auto secret=local_capability();if(!secret)return false;
    const std::string request="{\"version\":1,\"id\":"+quote(id)+",\"operation\":"+quote(operation)+",\"params\":"+std::string(params)+",\"auth\":{\"mode\":\"local\",\"token\":"+quote(*secret)+"}}";
    return transfer(request,id,response,timeout);
}
inline bool call_authenticated(std::string_view id,std::string_view operation,std::string_view params,std::string_view agent,std::string_view token,std::string_view scope,std::string& response,DWORD timeout=1000){
    if(id.empty()||id.size()>128||operation.empty()||operation.size()>128||params.size()>58000||agent.empty()||agent.size()>128||token.empty()||token.size()>256||scope.empty())return false;
    std::string request="{\"version\":1,\"id\":"+quote(id)+",\"operation\":"+quote(operation)+",\"params\":"+std::string(params)+",\"auth\":{\"mode\":\"mcp\",\"agent\":"+quote(agent)+",\"token\":"+quote(token)+",\"scope\":"+quote(scope)+"}}";
    return transfer(request,id,response,timeout);
}
inline bool healthy(DWORD timeout=250){std::string response;const std::string id="health",request="{\"version\":1,\"id\":\"health\",\"operation\":\"service.health\",\"params\":{}}";return transfer(request,id,response,timeout);}
}
#endif
