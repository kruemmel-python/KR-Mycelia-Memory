#include "world_checkpoint.hpp"
#include <algorithm>
#include <array>
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
constexpr std::string_view magic = "KRCHECK1";
constexpr std::string_view secure_magic = "KRCDPAPI";
constexpr std::size_t max_part = 128ull * 1024 * 1024;
constexpr std::size_t max_total = 2 * max_part + 4096;
void write64(std::string& out, std::uint64_t value) {
    for (int i = 0; i != 8; ++i) out.push_back(static_cast<char>(value >> (8 * i)));
}
std::uint64_t read64(std::string_view in, std::size_t& offset) {
    if (offset > in.size() || in.size() - offset < 8) throw std::runtime_error("checkpoint truncated");
    std::uint64_t result = 0;
    for (int i = 0; i != 8; ++i) result |= std::uint64_t(static_cast<unsigned char>(in[offset++])) << (8 * i);
    return result;
}
#ifdef _WIN32
struct LocalBlob {
    DATA_BLOB blob{};
    LocalBlob() = default;
    LocalBlob(const LocalBlob&) = delete;
    LocalBlob& operator=(const LocalBlob&) = delete;
    ~LocalBlob() { if (blob.pbData) { SecureZeroMemory(blob.pbData, blob.cbData); LocalFree(blob.pbData); } }
};
void erase(std::string& buffer) { if (!buffer.empty()) SecureZeroMemory(buffer.data(), buffer.size()); }
std::runtime_error last_error(const char* operation) {
    return std::runtime_error(std::string(operation) + " Win32=" + std::to_string(GetLastError()));
}
void write_atomic(const std::filesystem::path& path, std::string_view bytes) {
    if (bytes.size() > max_total || bytes.size() > std::numeric_limits<DWORD>::max())
        throw std::runtime_error("checkpoint disk size exceeded");
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    auto temp = path; temp += L".tmp";
    HANDLE h = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) throw last_error("CreateFileW checkpoint temp");
    try {
        DWORD count = 0;
        if (!WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr) || count != bytes.size())
            throw last_error("WriteFile checkpoint");
        if (!FlushFileBuffers(h)) throw last_error("FlushFileBuffers checkpoint");
    } catch (...) { CloseHandle(h); DeleteFileW(temp.c_str()); throw; }
    if (!CloseHandle(h)) { DeleteFileW(temp.c_str()); throw last_error("CloseHandle checkpoint"); }
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temp.c_str()); throw last_error("MoveFileExW checkpoint");
    }
}
std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("checkpoint unreadable");
    auto length = input.tellg();
    if (length < 16 || length > static_cast<std::streamoff>(max_total))
        throw std::runtime_error("checkpoint disk size invalid");
    std::string bytes(static_cast<std::size_t>(length), '\0');
    input.seekg(0);
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!input) throw std::runtime_error("checkpoint read truncated");
    return bytes;
}
#endif
}
std::string WorldCheckpointCodec::encode(const MyceliaWorldCpu& world,
                                         const DynamicAssociativeDatabase& memory) {
    const std::string world_bytes = world.serialize();
    const std::string memory_bytes = memory.snapshot_bytes();
    if (world_bytes.size() > max_part || memory_bytes.size() > max_part)
        throw std::runtime_error("checkpoint component too large");
    std::string result(magic);
    write64(result, world_bytes.size());
    write64(result, memory_bytes.size());
    result += world_bytes;
    result += memory_bytes;
    result += DynamicAssociativeDatabase::sha256(result);
    return result;
}
WorldCheckpoint WorldCheckpointCodec::decode(std::string_view input) {
    if (input.size() < magic.size() + 16 + 64 || input.size() > max_total || !input.starts_with(magic))
        throw std::runtime_error("checkpoint header or length invalid");
    std::size_t offset = magic.size();
    const std::uint64_t wn = read64(input, offset);
    const std::uint64_t mn = read64(input, offset);
    if (wn > max_part || mn > max_part || wn > input.size() - offset ||
        mn > input.size() - offset - static_cast<std::size_t>(wn) ||
        input.size() - offset - static_cast<std::size_t>(wn) - static_cast<std::size_t>(mn) != 64)
        throw std::runtime_error("checkpoint lengths invalid");
    const auto payload = input.substr(0, input.size() - 64);
    if (DynamicAssociativeDatabase::sha256(payload) != input.substr(input.size() - 64))
        throw std::runtime_error("checkpoint integrity failure");
    // Decode both before returning either; no partial restore is exposed.
    auto world = MyceliaWorldCpu::deserialize(input.substr(offset, static_cast<std::size_t>(wn)));
    auto memory = DynamicAssociativeDatabase::from_snapshot_bytes(input.substr(offset + static_cast<std::size_t>(wn), static_cast<std::size_t>(mn)));
    return {std::move(world), std::move(memory)};
}
void WorldCheckpointCodec::save_secure(const std::filesystem::path& path,
                                      const MyceliaWorldCpu& world,
                                      const DynamicAssociativeDatabase& memory) {
#ifdef _WIN32
    std::string plain = encode(world, memory);
    if (plain.size() > std::numeric_limits<DWORD>::max()) throw std::runtime_error("DPAPI input too large");
    DATA_BLOB input{static_cast<DWORD>(plain.size()), reinterpret_cast<BYTE*>(plain.data())};
    LocalBlob encrypted;
    const BOOL ok = CryptProtectData(&input, L"KR Mycelia world-memory checkpoint", nullptr,
                                      nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &encrypted.blob);
    erase(plain);
    if (!ok) throw last_error("CryptProtectData checkpoint");
    std::string file(secure_magic);
    file.append(reinterpret_cast<const char*>(encrypted.blob.pbData), encrypted.blob.cbData);
    write_atomic(path, file);
    erase(file);
#else
    (void)path; (void)world; (void)memory;
    throw std::runtime_error("secure checkpoint needs Windows DPAPI; plaintext fallback forbidden");
#endif
}
WorldCheckpoint WorldCheckpointCodec::load_secure(const std::filesystem::path& path) {
#ifdef _WIN32
    std::string file = read_file(path);
    if (!std::string_view(file).starts_with(secure_magic)) throw std::runtime_error("not an encrypted checkpoint");
    const auto blob_size = file.size() - secure_magic.size();
    if (blob_size == 0 || blob_size > std::numeric_limits<DWORD>::max())
        throw std::runtime_error("invalid DPAPI payload length");
    DATA_BLOB input{static_cast<DWORD>(blob_size), reinterpret_cast<BYTE*>(file.data() + secure_magic.size())};
    LocalBlob decrypted;
    const BOOL ok = CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                                        CRYPTPROTECT_UI_FORBIDDEN, &decrypted.blob);
    erase(file);
    if (!ok) throw last_error("CryptUnprotectData checkpoint");
    return decode(std::string_view(reinterpret_cast<const char*>(decrypted.blob.pbData), decrypted.blob.cbData));
#else
    (void)path;
    throw std::runtime_error("secure checkpoint needs Windows DPAPI; plaintext fallback forbidden");
#endif
}
}
