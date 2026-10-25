#include "directory_watcher.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <thread>
#include <atomic>
#include <vector>
#include <stdexcept>
namespace krmemory {struct DirectoryWatcher::State{std::thread worker;std::atomic<bool> running{};HANDLE handle{INVALID_HANDLE_VALUE};};DirectoryWatcher::DirectoryWatcher():state_(std::make_unique<State>()){}DirectoryWatcher::~DirectoryWatcher(){stop();}void DirectoryWatcher::start(const std::filesystem::path& root,Callback cb){stop();state_->handle=CreateFileW(root.c_str(),FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);if(state_->handle==INVALID_HANDLE_VALUE)throw std::runtime_error("cannot watch directory");state_->running=true;state_->worker=std::thread([this,root,cb]{std::vector<unsigned char>b(64*1024);while(state_->running){DWORD n{};if(!ReadDirectoryChangesW(state_->handle,b.data(),(DWORD)b.size(),TRUE,FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_LAST_WRITE,&n,nullptr,nullptr))break;for(auto* p=reinterpret_cast<FILE_NOTIFY_INFORMATION*>(b.data());state_->running;){cb(root/std::wstring(p->FileName,p->FileNameLength/sizeof(wchar_t)));if(!p->NextEntryOffset)break;p=reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<unsigned char*>(p)+p->NextEntryOffset);}}});}void DirectoryWatcher::stop(){if(!state_->running)return;state_->running=false;CancelIoEx(state_->handle,nullptr);if(state_->worker.joinable())state_->worker.join();CloseHandle(state_->handle);state_->handle=INVALID_HANDLE_VALUE;}}
#else
namespace krmemory {struct DirectoryWatcher::State{};DirectoryWatcher::DirectoryWatcher():state_(std::make_unique<State>()){}DirectoryWatcher::~DirectoryWatcher()=default;void DirectoryWatcher::start(const std::filesystem::path&,Callback){throw std::runtime_error("Windows directory watcher unavailable");}void DirectoryWatcher::stop(){}}
#endif
