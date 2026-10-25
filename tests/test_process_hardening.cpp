#include "process_hardening.hpp"
#include <iostream>
int main(){
#ifdef _WIN32
    auto r=krmemory::apply_process_hardening();if(!r.dll_search||!r.extension_points_disabled||!r.remote_images_blocked||!r.low_integrity_images_blocked||!r.prefer_system32){std::cerr<<"FAIL: required Windows process mitigations are not active\n";return 1;}std::cout<<"PASS: hardened DLL search, extension points and image loading\n";
#endif
    return 0;
}
