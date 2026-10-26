#include "collector_runtime.hpp"
#include "folder_collector.hpp"
#include <chrono>
#include <thread>
#include <unordered_set>
namespace krmemory {
namespace {
bool beneath(std::string_view path,std::string_view root){return path.size()>root.size()&&path.compare(0,root.size(),root)==0&&path[root.size()]=='/';}
template<class Change> bool short_transaction(MemoryStore& store,const std::atomic_bool& stopping,Change&& change){
    for(int attempt{};attempt<10&&!stopping;++attempt){try{store.transaction(std::forward<Change>(change),std::chrono::seconds(2));return true;}catch(...){std::this_thread::sleep_for(std::chrono::milliseconds(200));}}
    return false;
}
void reconcile_incrementally(MemoryStore& store,const std::filesystem::path& root,const std::vector<std::filesystem::path>& roots,const std::atomic_bool& stopping){
    constexpr std::size_t maximum_files=20000;std::unordered_set<std::string> present;bool truncated{};std::error_code error;
    std::filesystem::recursive_directory_iterator iterator(root,std::filesystem::directory_options::skip_permission_denied,error),end;
    while(iterator!=end&&!stopping){
        if(error){error.clear();iterator.increment(error);continue;}
        if(iterator->is_regular_file(error)&&!error){auto path=iterator->path();if(collector_format_supported(path)){if(present.size()>=maximum_files){truncated=true;break;}present.insert(normalized_source_key(path));short_transaction(store,stopping,[&](auto& db){collect_changed_file(db,path,roots,&stopping);});std::this_thread::sleep_for(std::chrono::milliseconds(25));}}else error.clear();
        iterator.increment(error);
    }
    if(stopping||truncated)return;
    std::vector<std::filesystem::path> stale;try{auto db=store.read();const auto root_key=normalized_source_key(root);for(const auto& record:db.query_sql_like("imported_memory")){auto source=record.data.find("source_path");if(source==record.data.end())continue;auto path=std::filesystem::path(mycelia::sql::display(source->second));auto key=normalized_source_key(path);if(collector_format_supported(path)&&beneath(key,root_key)&&!present.contains(key))stale.push_back(std::move(path));}}catch(...){return;}
    for(const auto& path:stale){if(stopping)return;short_transaction(store,stopping,[&](auto& db){remove_collected_source(db,path);});std::this_thread::sleep_for(std::chrono::milliseconds(25));}
}
}
CollectorRuntime::CollectorRuntime(MemoryStore& store,std::vector<std::filesystem::path> roots):store_(store),roots_(std::move(roots)){}
CollectorRuntime::~CollectorRuntime(){stop();}
void CollectorRuntime::start(){
    stop();stopping_=false;
    for(const auto& root:roots_){
        auto watcher=std::make_unique<DirectoryWatcher>();
        watcher->start(root,[this](const auto& file){try{
            if(stopping_)return;
            if(!std::filesystem::exists(file)){store_.transaction([&](auto& db){remove_collected_source(db,file);});return;}
            if(!std::filesystem::is_regular_file(file)||!collector_format_supported(file))return;
            short_transaction(store_,stopping_,[&](auto& db){collect_changed_file(db,file,roots_,&stopping_);});
        }catch(...){/* Collector failures are isolated; later changes retry the file. */}});
        watchers_.push_back(std::move(watcher));
    }
    // The watcher becomes responsive immediately. The potentially larger initial
    // reconciliation runs independently and can be cancelled when capture stops.
    reconcile_worker_=std::thread([this]{
        for(int delay{};delay<5&&!stopping_;++delay)std::this_thread::sleep_for(std::chrono::milliseconds(100));
        for(const auto& root:roots_){if(stopping_)break;try{reconcile_incrementally(store_,root,roots_,stopping_);}catch(...){/* Continue with the other explicitly approved roots. */}}
    });
}
void CollectorRuntime::stop(){
    stopping_=true;
    for(auto& watcher:watchers_)watcher->stop();
    watchers_.clear();
    if(reconcile_worker_.joinable())reconcile_worker_.join();
}
}
