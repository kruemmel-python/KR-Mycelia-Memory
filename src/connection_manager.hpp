#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace krmemory {
enum class ClientKind { cursor, claude_desktop, codex };
struct ClientConnection {
    ClientKind kind{};
    std::string display_name;
    bool installed{};
    bool configured{};
    std::filesystem::path config_path;
    std::string detail;
};
std::vector<ClientConnection> detect_supported_clients();
ClientConnection connect_client(ClientKind kind,const std::filesystem::path& config_path,
                                const std::filesystem::path& mcp_executable,
                                std::string agent,std::string token);
ClientConnection disconnect_client(ClientKind kind,const std::filesystem::path& config_path,
                                   std::string_view agent);
}
