#include "local_ai.hpp"
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
struct State{std::mutex mutex;std::condition_variable changed;std::string text;std::optional<krmemory::LocalStreamEnd> end;};
int main(){
    if(!krmemory::probe_lm_studio().available){std::cout<<"SKIP: no local LM Studio endpoint\n";return 0;}
    static_assert(krmemory::local_stream_timeout_ms==300000);
    static_assert(krmemory::local_stream_maximum_bytes==4*1024*1024);
    static_assert(krmemory::local_stream_maximum_tokens==8192);
    State first;auto request=krmemory::LocalStreamRequest::start("Antworte ausschließlich mit OK.","",[&](const auto& event){std::lock_guard lock(first.mutex);first.text+=event.delta;if(event.end)first.end=event.end;first.changed.notify_all();},30000,krmemory::local_stream_maximum_bytes);if(!request)return 1;
    {std::unique_lock lock(first.mutex);if(!first.changed.wait_for(lock,std::chrono::seconds(35),[&]{return first.end.has_value();}))return 2;if(*first.end!=krmemory::LocalStreamEnd::completed||first.text.empty())return 3;}
    State cancelled;auto second=krmemory::LocalStreamRequest::start("Schreibe eine sehr lange Abhandlung mit mindestens 5000 Wörtern.","",[&](const auto& event){std::lock_guard lock(cancelled.mutex);cancelled.text+=event.delta;if(event.end)cancelled.end=event.end;cancelled.changed.notify_all();},30000,512*1024);if(!second)return 4;std::this_thread::sleep_for(std::chrono::milliseconds(100));second->cancel();
    {std::unique_lock lock(cancelled.mutex);if(!cancelled.changed.wait_for(lock,std::chrono::seconds(5),[&]{return cancelled.end.has_value();}))return 5;if(*cancelled.end!=krmemory::LocalStreamEnd::cancelled)return 6;}
    std::cout<<"PASS: real LM Studio SSE stream completed and active HTTP request cancelled\n";return 0;
}
