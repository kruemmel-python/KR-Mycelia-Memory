#include "sse_parser.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    krmemory::SseParser parser;std::string output;std::vector<std::string> chunks={
        "data: {\"choices\":[{\"delta\":{\"content\":\"Gr",
        "ü", "ße\"}}]}\n\ndata: {\"choices\":[{\"delta\":{\"content\":\" ",
        "aus\"}}]}\r\n\r\ndata: {\"choices\":[{\"delta\":{\"content\":\" Mycelia\"}}]}\n\n",
        "data: [DO", "NE]\n\n"};
    krmemory::SseParser::FeedResult result{};for(const auto& chunk:chunks)result=parser.feed(chunk,[&](std::string_view delta){output+=delta;});
    require(result==krmemory::SseParser::FeedResult::done,"DONE event not detected");require(output=="Grüße aus Mycelia","fragmented UTF-8 or event ordering failed");
    krmemory::SseParser multiple;std::string second;auto combined="data: {\"choices\":[{\"delta\":{\"content\":\"A\"}}]}\n\ndata: {\"choices\":[{\"delta\":{\"content\":\"B\"}}]}\n\ndata: [DONE]\n\n";require(multiple.feed(combined,[&](std::string_view d){second+=d;})==krmemory::SseParser::FeedResult::done,"multiple events failed");require(second=="AB","multiple events reordered");
    std::cout<<"PASS: fragmented UTF-8 SSE streaming, multiple events and DONE\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
