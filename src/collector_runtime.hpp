#pragma once
#include <filesystem>
#include <memory>
#include <vector>
#include "directory_watcher.hpp"
#include "memory_store.hpp"
namespace krmemory {class CollectorRuntime {public:CollectorRuntime(MemoryStore&,std::vector<std::filesystem::path>);void start();void stop();private:MemoryStore& store_;std::vector<std::filesystem::path> roots_;std::vector<std::unique_ptr<DirectoryWatcher>> watchers_;};}
