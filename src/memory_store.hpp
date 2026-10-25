#pragma once
#include <chrono>
#include <filesystem>
#include <functional>
#include "mycelia.hpp"

namespace krmemory {
// Serializes all writers in one Windows user session.  Each transaction reloads
// the current encrypted snapshot while holding the lock, so independently
// launched native clients cannot overwrite each other's committed update.
class MemoryStore {
public:
    explicit MemoryStore(std::filesystem::path snapshot);
    mycelia::DynamicAssociativeDatabase read() const;
    void transaction(const std::function<void(mycelia::DynamicAssociativeDatabase&)>& change,
                     std::chrono::milliseconds timeout=std::chrono::seconds(30)) const;
    const std::filesystem::path& path() const noexcept { return snapshot_; }
private:
    mycelia::DynamicAssociativeDatabase load_unlocked() const;
    void rotate_backups_unlocked() const;
    std::filesystem::path snapshot_;
};
}
