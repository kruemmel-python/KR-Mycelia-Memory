#include "gpu_observation.hpp"
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dpapi.h>
#endif

namespace mycelia {
namespace {
#ifdef _WIN32
constexpr std::string_view magic = "KRGPUENC1";
constexpr std::size_t max_file_size = 256ull * 1024 * 1024 + 65536;
constexpr std::size_t digest_size = 64;

[[nodiscard]] std::runtime_error win_error(const char* operation) {
    return std::runtime_error(std::string(operation) + " Win32=" + std::to_string(GetLastError()));
}
struct DpapiBlob {
    DATA_BLOB blob{};
    DpapiBlob() = default;
    DpapiBlob(const DpapiBlob&) = delete;
    DpapiBlob& operator=(const DpapiBlob&) = delete;
    ~DpapiBlob() {
        if (blob.pbData != nullptr) {
            SecureZeroMemory(blob.pbData, blob.cbData);
            LocalFree(blob.pbData);
        }
    }
};
void cleanse(std::string& bytes) noexcept {
    if (!bytes.empty()) SecureZeroMemory(bytes.data(), bytes.size());
}
struct CleanString {
    std::string bytes;
    ~CleanString() { cleanse(bytes); }
};
void write_atomic(const std::filesystem::path& path, std::string_view bytes) {
    if (bytes.size() > max_file_size || bytes.size() > std::numeric_limits<DWORD>::max())
        throw std::runtime_error("encrypted GPU checkpoint too large");
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    auto temporary = path;
    temporary += L".tmp";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw win_error("CreateFileW");
    try {
        DWORD written = 0;
        if (!WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) ||
            written != bytes.size()) throw win_error("WriteFile");
        if (!FlushFileBuffers(file)) throw win_error("FlushFileBuffers");
    } catch (...) {
        CloseHandle(file);
        DeleteFileW(temporary.c_str());
        throw;
    }
    if (!CloseHandle(file)) {
        DeleteFileW(temporary.c_str());
        throw win_error("CloseHandle");
    }
    // Never overwrite an existing checkpoint. The caller must choose a new snapshot ID.
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        throw win_error("MoveFileExW");
    }
}
std::string read_file(const std::filesystem::path& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw win_error("CreateFileW read");
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 80 ||
        size.QuadPart > static_cast<LONGLONG>(max_file_size)) {
        CloseHandle(file);
        throw std::runtime_error("encrypted GPU checkpoint size invalid");
    }
    std::string bytes(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    const bool ok = ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) != 0;
    CloseHandle(file);
    if (!ok || read != bytes.size()) throw std::runtime_error("encrypted GPU checkpoint truncated");
    return bytes;
}
#endif
} // namespace

void GpuObservationCodec::save_secure(const std::filesystem::path& path,
                                     const MyceliaWorldCpu& world,
                                     const DynamicAssociativeDatabase& memory,
                                     const GpuObservation& gpu) {
#ifdef _WIN32
    CleanString plain{encode(world, memory, gpu)};
    if (plain.bytes.size() > std::numeric_limits<DWORD>::max())
        throw std::runtime_error("GPU checkpoint plaintext too large");
    DATA_BLOB input{static_cast<DWORD>(plain.bytes.size()),
                    reinterpret_cast<BYTE*>(plain.bytes.data())};
    DpapiBlob encrypted;
    if (!CryptProtectData(&input, L"KR Mycelia GPU observation", nullptr,
                          nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &encrypted.blob))
        throw win_error("CryptProtectData");
    std::string_view cipher(reinterpret_cast<const char*>(encrypted.blob.pbData), encrypted.blob.cbData);
    // Envelope digest detects random corruption, DPAPI protects ciphertext integrity.
    const std::string envelope = std::string(magic) + DynamicAssociativeDatabase::sha256(cipher)
                               + std::string(cipher);
    write_atomic(path, envelope);
#else
    (void)path; (void)world; (void)memory; (void)gpu;
    throw std::runtime_error("GPU observation secure save requires Windows DPAPI");
#endif
}

ObservedWorldCheckpoint GpuObservationCodec::load_secure(const std::filesystem::path& path) {
#ifdef _WIN32
    CleanString envelope{read_file(path)};
    if (!std::string_view(envelope.bytes).starts_with(magic) ||
        envelope.bytes.size() < magic.size()+digest_size+16)
        throw std::runtime_error("invalid encrypted GPU envelope");
    const std::string_view cipher(envelope.bytes.data()+magic.size()+digest_size,
                                  envelope.bytes.size()-magic.size()-digest_size);
    const std::string checksum = DynamicAssociativeDatabase::sha256(cipher);
    unsigned mismatch = 0;
    for (std::size_t i=0; i<digest_size; ++i)
        mismatch |= static_cast<unsigned>(static_cast<unsigned char>(checksum[i]) ^
                                          static_cast<unsigned char>(envelope.bytes[magic.size()+i]));
    if (mismatch) throw std::runtime_error("encrypted GPU envelope corrupted");
    DATA_BLOB input{static_cast<DWORD>(cipher.size()),
                    reinterpret_cast<BYTE*>(const_cast<char*>(cipher.data()))};
    DpapiBlob decrypted;
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &decrypted.blob))
        throw win_error("CryptUnprotectData");
    return decode(std::string_view(reinterpret_cast<const char*>(decrypted.blob.pbData),
                                   decrypted.blob.cbData));
#else
    (void)path;
    throw std::runtime_error("GPU observation secure load requires Windows DPAPI");
#endif
}
} // namespace mycelia
