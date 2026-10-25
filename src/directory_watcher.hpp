#pragma once
#include <filesystem>
#include <functional>
#include <memory>
namespace krmemory {class DirectoryWatcher {public:using Callback=std::function<void(const std::filesystem::path&)>;DirectoryWatcher();~DirectoryWatcher();void start(const std::filesystem::path&,Callback);void stop();private:struct State;std::unique_ptr<State> state_;};}
