#include "mycelia.hpp"
#include <fstream>
#include <stdexcept>
#include <limits>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dpapi.h>
#endif

namespace mycelia {
#ifdef _WIN32
namespace {
constexpr size_t MAX_CIPHER = 128ull*1024*1024 + 4096;
constexpr std::string_view ENVELOPE_MAGIC = "KRMSEC02";
constexpr size_t DIGEST_SIZE = 64; // SHA-256 hex characters
// Unkeyed digest detects accidental changes to the DPAPI blob, including its
// metadata. It does NOT authenticate the envelope against a malicious writer.
// DPAPI is responsible for protecting the encrypted payload.
std::string wrap_ciphertext(std::string_view cipher) {
    return std::string(ENVELOPE_MAGIC) + DynamicAssociativeDatabase::sha256(cipher)
         + std::string(cipher);
}
std::string_view unwrap_ciphertext(std::string_view file) {
    const size_t header = ENVELOPE_MAGIC.size() + DIGEST_SIZE;
    if (file.size() < header + 16 || file.substr(0, ENVELOPE_MAGIC.size()) != ENVELOPE_MAGIC)
        throw std::runtime_error("invalid secure snapshot envelope");
    const auto stored = file.substr(ENVELOPE_MAGIC.size(), DIGEST_SIZE);
    const auto blob = file.substr(header);
    const auto actual = DynamicAssociativeDatabase::sha256(blob);
    unsigned mismatch = 0;
    for (size_t i = 0; i < DIGEST_SIZE; ++i)
        mismatch |= static_cast<unsigned>(static_cast<unsigned char>(stored[i]) ^ static_cast<unsigned char>(actual[i]));
    if (mismatch != 0) throw std::runtime_error("encrypted snapshot corruption detected");
    return blob;
}
struct ProtectedBlob {
    DATA_BLOB data{};
    ~ProtectedBlob(){if(data.pbData){SecureZeroMemory(data.pbData,data.cbData);LocalFree(data.pbData);}}
    ProtectedBlob()=default;
    ProtectedBlob(const ProtectedBlob&)=delete;
    ProtectedBlob& operator=(const ProtectedBlob&)=delete;
};
std::runtime_error winerror(const char* where){return std::runtime_error(std::string(where)+" (Win32="+std::to_string(GetLastError())+")");}
void secure_write(const std::filesystem::path& path, const std::string& ciphertext){
    if(ciphertext.size()>MAX_CIPHER + ENVELOPE_MAGIC.size() + DIGEST_SIZE)throw std::runtime_error("ciphertext too large");
    if(path.has_parent_path())std::filesystem::create_directories(path.parent_path());
    auto temp=path;temp+=L".tmp";
    HANDLE h=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)throw winerror("CreateFileW");
    try {
        DWORD done=0;
        if(!WriteFile(h,ciphertext.data(),static_cast<DWORD>(ciphertext.size()),&done,nullptr)||done!=ciphertext.size())throw winerror("WriteFile");
        if(!FlushFileBuffers(h))throw winerror("FlushFileBuffers");
    }catch(...){CloseHandle(h);DeleteFileW(temp.c_str());throw;}
    if(!CloseHandle(h)){DeleteFileW(temp.c_str());throw winerror("CloseHandle");}
    // Atomic replacement on same volume. Does not guarantee directory metadata durability after power loss.
    if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        DeleteFileW(temp.c_str());throw winerror("MoveFileExW");
    }
}
std::string secure_read(const std::filesystem::path& path){
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f)throw std::runtime_error("encrypted snapshot not readable");
    auto n=f.tellg();
    if(n<16||n>static_cast<std::streamoff>(MAX_CIPHER + ENVELOPE_MAGIC.size() + DIGEST_SIZE))throw std::runtime_error("encrypted snapshot size invalid");
    std::string v(static_cast<size_t>(n),'\0');f.seekg(0);f.read(v.data(),n);
    if(!f)throw std::runtime_error("encrypted snapshot truncated");
    return v;
}
}
#endif
void DynamicAssociativeDatabase::save_secure_snapshot(const std::filesystem::path& path) const {
#ifdef _WIN32
    std::string plain=snapshot_bytes();
    if(plain.size()>std::numeric_limits<DWORD>::max())throw std::runtime_error("plaintext too large");
    DATA_BLOB input{static_cast<DWORD>(plain.size()),reinterpret_cast<BYTE*>(plain.data())};
    ProtectedBlob output;
    const bool ok=CryptProtectData(&input,L"KR Mycelia native snapshot",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output.data)!=0;
    SecureZeroMemory(plain.data(),plain.size());
    if(!ok)throw winerror("CryptProtectData");
    std::string cipher(reinterpret_cast<const char*>(output.data.pbData),output.data.cbData);
    secure_write(path,wrap_ciphertext(cipher));
    SecureZeroMemory(cipher.data(),cipher.size());
#else
    (void)path;
    throw std::runtime_error("Windows DPAPI secure snapshots require Windows; no plaintext fallback");
#endif
}
DynamicAssociativeDatabase DynamicAssociativeDatabase::load_secure_snapshot(const std::filesystem::path& path){
#ifdef _WIN32
    auto file=secure_read(path);
    const auto encoded=unwrap_ciphertext(file);
    std::string cipher(encoded);
    SecureZeroMemory(file.data(),file.size());
    DATA_BLOB input{static_cast<DWORD>(cipher.size()),reinterpret_cast<BYTE*>(cipher.data())};
    ProtectedBlob output;
    const bool ok=CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output.data)!=0;
    SecureZeroMemory(cipher.data(),cipher.size());
    if(!ok)throw winerror("CryptUnprotectData");
    return from_snapshot_bytes(std::string_view(reinterpret_cast<const char*>(output.data.pbData),output.data.cbData));
#else
    (void)path;
    throw std::runtime_error("Windows DPAPI secure snapshots require Windows; no plaintext fallback");
#endif
}
}
