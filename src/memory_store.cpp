#include "memory_store.hpp"
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace krmemory {
#ifdef _WIN32
namespace {
class StoreLock {
public:
    explicit StoreLock(std::chrono::milliseconds timeout) {
        mutex_=CreateMutexW(nullptr,FALSE,L"Local\\KRMyceliaMemoryStore-v1");
        if(!mutex_) throw std::runtime_error("Cannot create memory transaction mutex");
        const auto wait=WaitForSingleObject(mutex_,static_cast<DWORD>(timeout.count()));
        if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED){CloseHandle(mutex_);mutex_={};throw std::runtime_error("Memory store is busy");}
        locked_=true;
    }
    StoreLock(const StoreLock&)=delete;
    StoreLock& operator=(const StoreLock&)=delete;
    ~StoreLock(){if(mutex_){if(locked_)ReleaseMutex(mutex_);CloseHandle(mutex_);}}
private: HANDLE mutex_{};bool locked_{};
};
}
#endif
MemoryStore::MemoryStore(std::filesystem::path snapshot):snapshot_(std::move(snapshot)) {
    if(!snapshot_.is_absolute()) throw std::invalid_argument("Memory snapshot path must be absolute");
}
mycelia::DynamicAssociativeDatabase MemoryStore::load_unlocked() const {
    if(std::filesystem::exists(snapshot_)){
        try{return mycelia::DynamicAssociativeDatabase::load_secure_snapshot(snapshot_);}
        catch(...){
            auto one=snapshot_;one+=L".bak1";auto two=snapshot_;two+=L".bak2";
            if(std::filesystem::exists(one))try{return mycelia::DynamicAssociativeDatabase::load_secure_snapshot(one);}catch(...){}
            if(std::filesystem::exists(two))return mycelia::DynamicAssociativeDatabase::load_secure_snapshot(two);
            throw;
        }
    }
    return mycelia::DynamicAssociativeDatabase(50000);
}
mycelia::DynamicAssociativeDatabase MemoryStore::read() const {
#ifdef _WIN32
    // Atomic replacement makes a torn file unlikely, but taking the same
    // lock as writers also provides a coherent read-after-write boundary.
    StoreLock lock(std::chrono::seconds(30));
    return load_unlocked();
#else
    throw std::runtime_error("Windows DPAPI memory store is unavailable on this platform");
#endif
}
void MemoryStore::rotate_backups_unlocked() const {
#ifdef _WIN32
    if(!std::filesystem::exists(snapshot_)) return;
    auto one=snapshot_;one+=L".bak1";auto two=snapshot_;two+=L".bak2";
    std::error_code ec;std::filesystem::remove(two,ec);
    if(std::filesystem::exists(one)) std::filesystem::rename(one,two,ec);
    ec.clear();std::filesystem::copy_file(snapshot_,one,std::filesystem::copy_options::overwrite_existing,ec);
    if(ec) throw std::runtime_error("Cannot rotate encrypted memory backup");
#endif
}
void MemoryStore::transaction(const std::function<void(mycelia::DynamicAssociativeDatabase&)>& change,
                              std::chrono::milliseconds timeout) const {
#ifdef _WIN32
    StoreLock lock(timeout);
    auto db=load_unlocked();change(db);rotate_backups_unlocked();db.save_secure_snapshot(snapshot_);
#else
    (void)change;(void)timeout;throw std::runtime_error("Windows DPAPI memory store is unavailable on this platform");
#endif
}
}
