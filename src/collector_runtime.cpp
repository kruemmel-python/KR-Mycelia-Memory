#include "collector_runtime.hpp"
#include "folder_collector.hpp"
#include <chrono>
#include <thread>
namespace krmemory {
CollectorRuntime::CollectorRuntime(MemoryStore& store,std::vector<std::filesystem::path> roots):store_(store),roots_(std::move(roots)){}
CollectorRuntime::~CollectorRuntime(){stop();}
void CollectorRuntime::start(){
    stop();stopping_=false;
    for(const auto& root:roots_){
        auto watcher=std::make_unique<DirectoryWatcher>();
        watcher->start(root,[this](const auto& file){try{
            if(stopping_)return;
            if(!std::filesystem::exists(file)){store_.transaction([&](auto& db){remove_collected_source(db,file);});return;}
            if(!std::filesystem::is_regular_file(file)||!import_format_supported(file))return;
            for(int attempt{};attempt<3&&!stopping_;++attempt){try{store_.transaction([&](auto& db){collect_changed_file(db,file,roots_);});return;}catch(...){if(attempt==2)throw;std::this_thread::sleep_for(std::chrono::milliseconds(150));}}
        }catch(...){/* Collector failures are isolated; later changes retry the file. */}});
        watchers_.push_back(std::move(watcher));
    }
    // The watcher becomes responsive immediately. The potentially larger initial
    // reconciliation runs independently and can be cancelled when capture stops.
    reconcile_worker_=std::thread([this]{
        for(int delay{};delay<5&&!stopping_;++delay)std::this_thread::sleep_for(std::chrono::milliseconds(100));
        for(const auto& root:roots_){if(stopping_)break;try{store_.transaction([&](auto& db){reconcile_collected_root(db,root,roots_,20000,&stopping_);});}catch(...){/* Continue with the other explicitly approved roots. */}}
    });
}
void CollectorRuntime::stop(){
    stopping_=true;
    for(auto& watcher:watchers_)watcher->stop();
    watchers_.clear();
    if(reconcile_worker_.joinable())reconcile_worker_.join();
}
}
