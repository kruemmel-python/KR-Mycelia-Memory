#include "collector_runtime.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

int main(){
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/"kr_mycelia_runtime_collector";
    const auto snapshot=fs::temp_directory_path()/"kr_mycelia_runtime_collector.dpapi";
    auto cleanup=[&]{std::error_code error;fs::remove_all(root,error);fs::remove(snapshot,error);fs::remove(snapshot.wstring()+L".bak1",error);fs::remove(snapshot.wstring()+L".bak2",error);};
    try{
        cleanup();fs::create_directories(root/"Project");
        for(int index{};index<8;++index){std::ofstream out(root/"Project"/("source"+std::to_string(index)+".md"));out<<"# Runtime source "<<index<<"\n"<<std::string(16384,char('a'+index));}
        {std::ofstream out(root/"Project"/"malformed.png",std::ios::binary);out<<"not an image";}
        krmemory::MemoryStore store(snapshot);krmemory::CollectorRuntime runtime(store,{root});runtime.start();std::this_thread::sleep_for(std::chrono::milliseconds(650));
        auto began=std::chrono::steady_clock::now();auto during_scan=store.read();auto elapsed=std::chrono::steady_clock::now()-began;if(elapsed>std::chrono::seconds(5))throw std::runtime_error("collector monopolized memory store");
        runtime.stop();auto final=store.read();for(const auto& record:final.query_sql_like("imported_memory")){auto source=record.data.find("source_path");if(source!=record.data.end()&&mycelia::sql::display(source->second).find("malformed.png")!=std::string::npos)throw std::runtime_error("recursive collector launched binary media import");}
        runtime.start();std::this_thread::sleep_for(std::chrono::milliseconds(100));runtime.stop();cleanup();std::cout<<"PASS: incremental collector remains responsive, skips media and stops safely\n";return 0;
    }catch(const std::exception& error){cleanup();std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
