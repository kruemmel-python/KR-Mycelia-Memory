#pragma once
#include <atomic>
#include <filesystem>
#include <memory>
#include <thread>
#include <vector>
#include "directory_watcher.hpp"
#include "memory_store.hpp"
namespace krmemory {class CollectorRuntime {public:CollectorRuntime(MemoryStore&,std::vector<std::filesystem::path>);~CollectorRuntime();CollectorRuntime(const CollectorRuntime&)=delete;CollectorRuntime& operator=(const CollectorRuntime&)=delete;void start();void stop();private:MemoryStore& store_;std::vector<std::filesystem::path> roots_;std::vector<std::unique_ptr<DirectoryWatcher>> watchers_;std::atomic_bool stopping_{false};std::thread reconcile_worker_;};}
