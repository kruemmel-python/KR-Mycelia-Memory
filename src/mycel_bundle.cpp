#include "mycel_bundle.hpp"
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#endif
namespace mycelia {
namespace {
constexpr std::string_view magic="KRMYBND1";
constexpr std::string_view secure_magic="KRBNDENC";
constexpr std::size_t limit=128ull*1024*1024;
void append64(std::string& s,std::uint64_t v){for(int i=0;i<8;++i)s.push_back(static_cast<char>(v>>(i*8)));}
std::uint64_t read64(std::string_view s,std::size_t& p){if(p>s.size()||s.size()-p<8)throw std::runtime_error("bundle length truncated");std::uint64_t n=0;for(int i=0;i<8;++i)n|=std::uint64_t(static_cast<unsigned char>(s[p++]))<<(i*8);return n;}
std::uint32_t read32(std::string_view s,std::size_t p){if(p>s.size()||s.size()-p<4)throw std::runtime_error("MYCL header truncated");return std::uint32_t(static_cast<unsigned char>(s[p]))|(std::uint32_t(static_cast<unsigned char>(s[p+1]))<<8)|(std::uint32_t(static_cast<unsigned char>(s[p+2]))<<16)|(std::uint32_t(static_cast<unsigned char>(s[p+3]))<<24);}
void validate_mycl(std::string_view s){
 if(s.size()<28 || s.size()>limit)throw std::runtime_error("MYCL size invalid");
 const auto m=read32(s,0),v=read32(s,4);const auto cap=std::uint64_t(read32(s,8));
 const auto channels=std::uint64_t(read32(s,12)),neigh=std::uint64_t(read32(s,16));
 const auto active=std::uint64_t(read32(s,20)),free_head=std::uint64_t(read32(s,24));
 if(m!=0x4D59434CUL||v!=1||!cap||cap>1000000||!channels||channels>4096||!neigh||neigh>4096||active>cap||free_head>cap)throw std::runtime_error("invalid MYCL metadata");
 const auto edges=cap*neigh,pher=edges*channels;
 const std::uint64_t expected=28+2*cap+4*(cap+cap+cap*channels+2*channels+edges+2*edges+pher+2*cap+7);
 if(expected!=s.size())throw std::runtime_error("MYCL dimensions or length inconsistent");
}
#ifdef _WIN32
struct Blob{DATA_BLOB data{};~Blob(){if(data.pbData){SecureZeroMemory(data.pbData,data.cbData);LocalFree(data.pbData);}}};
void erase(std::string& s){if(!s.empty())SecureZeroMemory(s.data(),s.size());}
std::runtime_error winerror(const char* op){return std::runtime_error(std::string(op)+" Win32="+std::to_string(GetLastError()));}
void write_new(const std::filesystem::path& path,std::string_view bytes){
 if(!path.is_absolute()||std::filesystem::exists(path))throw std::invalid_argument("new absolute bundle path required");
 HANDLE h=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(h==INVALID_HANDLE_VALUE)throw winerror("CreateFileW bundle");
 try{DWORD n=0;if(bytes.size()>std::numeric_limits<DWORD>::max()||!WriteFile(h,bytes.data(),static_cast<DWORD>(bytes.size()),&n,nullptr)||n!=bytes.size())throw winerror("WriteFile bundle");if(!FlushFileBuffers(h))throw winerror("FlushFileBuffers bundle");}
 catch(...){CloseHandle(h);DeleteFileW(path.c_str());throw;}
 if(!CloseHandle(h))throw winerror("CloseHandle bundle");
}
#endif
}
std::string MycelBundleCodec::encode(const MyceliaWorldCpu& world,const DynamicAssociativeDatabase& memory,std::string_view mycl){
 validate_mycl(mycl);
 auto checkpoint=WorldCheckpointCodec::encode(world,memory);
 if(checkpoint.size()>limit||mycl.size()>limit)throw std::runtime_error("bundle exceeds component limit");
 std::string out(magic);append64(out,checkpoint.size());append64(out,mycl.size());out+=checkpoint;out.append(mycl);out+=DynamicAssociativeDatabase::sha256(out);return out;
}
MycelBundle MycelBundleCodec::decode(std::string_view s){
 if(s.size()<magic.size()+16+64||s.size()>2*limit+256||!s.starts_with(magic))throw std::runtime_error("bundle header invalid");
 std::size_t p=magic.size();auto a=read64(s,p),b=read64(s,p);
 if(a>limit||b>limit||a>s.size()-p||b>s.size()-p-static_cast<std::size_t>(a)||s.size()-p-a-b!=64)throw std::runtime_error("bundle section lengths invalid");
 if(DynamicAssociativeDatabase::sha256(s.substr(0,s.size()-64))!=s.substr(s.size()-64))throw std::runtime_error("bundle digest mismatch");
 const auto mycl=s.substr(p+static_cast<std::size_t>(a),static_cast<std::size_t>(b));validate_mycl(mycl);
 auto checkpoint=WorldCheckpointCodec::decode(s.substr(p,static_cast<std::size_t>(a)));
 return {std::move(checkpoint),std::string(mycl)};
}
void MycelBundleCodec::save_secure(const std::filesystem::path& path,const MyceliaWorldCpu& world,const DynamicAssociativeDatabase& memory,std::string_view mycl){
#ifdef _WIN32
 std::string plain=encode(world,memory,mycl);
 DATA_BLOB input{static_cast<DWORD>(plain.size()),reinterpret_cast<BYTE*>(plain.data())};Blob output;
 const BOOL ok=CryptProtectData(&input,L"KR Mycelia bundled state",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output.data);
 erase(plain);if(!ok)throw winerror("CryptProtectData bundle");
 std::string file(secure_magic);file.append(reinterpret_cast<const char*>(output.data.pbData),output.data.cbData);
 try{write_new(path,file);}catch(...){erase(file);throw;}erase(file);
#else
 (void)path;(void)world;(void)memory;(void)mycl;throw std::runtime_error("Windows DPAPI required; no plaintext bundle fallback");
#endif
}
MycelBundle MycelBundleCodec::load_secure(const std::filesystem::path& path){
#ifdef _WIN32
 std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("bundle not readable");
 const auto end=f.tellg();if(end<static_cast<std::streamoff>(secure_magic.size()+16)||end>static_cast<std::streamoff>(2*limit+8192))throw std::runtime_error("encrypted bundle length invalid");
 std::string file(static_cast<std::size_t>(end),'\0');f.seekg(0);if(!f.read(file.data(),static_cast<std::streamsize>(file.size())))throw std::runtime_error("encrypted bundle read failed");
 if(!std::string_view(file).starts_with(secure_magic))throw std::runtime_error("bundle is not DPAPI encrypted");
 DATA_BLOB input{static_cast<DWORD>(file.size()-secure_magic.size()),reinterpret_cast<BYTE*>(file.data()+secure_magic.size())};Blob output;
 const BOOL ok=CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output.data);erase(file);
 if(!ok)throw winerror("CryptUnprotectData bundle");
 return decode(std::string_view(reinterpret_cast<const char*>(output.data.pbData),output.data.cbData));
#else
 (void)path;throw std::runtime_error("Windows DPAPI required; no plaintext bundle fallback");
#endif
}
}
