#include "mycl_compare.hpp"
#include <iostream>
#include <iomanip>
#include <exception>
int main(int argc,char** argv) {
    if(argc!=3) {
        std::cerr<<"Usage: mycelia_mycl_compare REFERENCE.mycl RESUMED.mycl\n";
        return 2;
    }
    try {
        const auto result=mycelia::compare_mycl(argv[1],argv[2]);
        std::cout<<"MYCL_COMPARE bytes="<<result.bytes
                 <<" different_bytes="<<result.differences
                 <<" reference_fnv64="<<std::hex<<result.left_fnv64
                 <<" resumed_fnv64="<<result.right_fnv64<<std::dec;
        if(!result.identical) std::cout<<" first_difference_offset="<<result.first_difference;
        std::cout<<'\n';
        std::cout<<(result.identical?"MYCL_RESTART_PARITY_PASS":"MYCL_RESTART_PARITY_DIFFERENT")<<'\n';
        return result.identical?0:3;
    } catch(const std::exception& e) {
        std::cerr<<"MYCL_COMPARE_ERROR: "<<e.what()<<'\n';
        return 1;
    }
}
