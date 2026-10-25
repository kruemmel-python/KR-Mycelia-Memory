#include "memory_store.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
static void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){
#ifdef _WIN32
    try {
        auto path=std::filesystem::temp_directory_path()/"kr_mycelia_memory_store_test.dpapi";
        std::error_code ec;std::filesystem::remove(path,ec);std::filesystem::remove(path.string()+".bak1",ec);std::filesystem::remove(path.string()+".bak2",ec);
        krmemory::MemoryStore store(path); constexpr int writers=1000,workers=16;
        std::vector<std::thread> jobs;for(int worker=0;worker<workers;++worker)jobs.emplace_back([&,worker]{for(int i=worker;i<writers;i+=workers)store.transaction([i](auto& db){mycelia::sql::Row row;row["title"]="writer-"+std::to_string(i);row["application"]="test";row["timestamp"]=(std::int64_t)i;db.store_sql_record("concurrent",row);});});
        for(auto& job:jobs)job.join();auto db=store.read();check(db.query_sql_like("concurrent").size()==writers,"lost concurrent transaction");
        check(std::filesystem::exists(path.string()+".bak1"),"encrypted backup missing");
        {std::ofstream corrupt(path,std::ios::binary|std::ios::trunc);corrupt<<"incomplete snapshot";}
        auto recovered=store.read();check(!recovered.query_sql_like("concurrent").empty(),"backup recovery failed");
        std::filesystem::remove(path,ec);std::filesystem::remove(path.string()+".bak1",ec);std::filesystem::remove(path.string()+".bak2",ec);
        std::cout<<"PASS: 1000 serialized transactions and encrypted backup recovery\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
#else
    std::cout<<"SKIP: Windows DPAPI memory-store test\n";return 0;
#endif
}
