#include "memory_service_rpc.hpp"
#include "process_hardening.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv){try{krmemory::apply_process_hardening();
    if(argc!=2)throw std::runtime_error("usage: KR_Mycelia_Import.exe <file>");
    auto file=std::filesystem::absolute(argv[1]);if(!std::filesystem::is_regular_file(file))throw std::runtime_error("file not found");
    const auto path_utf8=file.u8string();const std::string path(path_utf8.begin(),path_utf8.end());
    std::string response;if(!krmemory::service_rpc::call("importer","memory.import","{\"path\":"+krmemory::service_rpc::quote(path)+"}",response,30000))throw std::runtime_error("local memory service unavailable or import failed");
    std::cout<<"Imported through local memory service: "<<file.string()<<"\n"<<response<<"\n";return 0;
}catch(const std::exception& e){std::cerr<<"KR Mycelia Import: "<<e.what()<<'\n';return 1;}}
