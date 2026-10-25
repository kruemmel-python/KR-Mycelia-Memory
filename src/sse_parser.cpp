#include "sse_parser.hpp"
#include "json_value.hpp"
#include <stdexcept>

namespace krmemory {
SseParser::FeedResult SseParser::feed(std::string_view bytes,const DeltaCallback& on_delta){
    if(done_)return FeedResult::done;
    if(pending_.size()+bytes.size()>1024*1024){error_="SSE buffer limit exceeded";return FeedResult::error;}
    pending_.append(bytes);
    return drain(false,on_delta);
}
SseParser::FeedResult SseParser::finish(const DeltaCallback& on_delta){return drain(true,on_delta);}
SseParser::FeedResult SseParser::drain(bool final,const DeltaCallback& on_delta){
    try{
        while(true){
            auto lf=pending_.find("\n\n"),crlf=pending_.find("\r\n\r\n");
            std::size_t end=std::string::npos,separator{};
            if(lf!=std::string::npos){end=lf;separator=2;}
            if(crlf!=std::string::npos&&(end==std::string::npos||crlf<end)){end=crlf;separator=4;}
            if(end==std::string::npos){if(!final)return FeedResult::more;if(pending_.empty())return done_?FeedResult::done:FeedResult::more;end=pending_.size();separator=0;}
            auto event=pending_.substr(0,end);pending_.erase(0,end+separator);
            std::string data;
            for(std::size_t at{};at<event.size();){auto line_end=event.find('\n',at);auto line=event.substr(at,line_end==std::string::npos?event.size()-at:line_end-at);if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.rfind("data:",0)==0){auto value=line.substr(5);if(!value.empty()&&value.front()==' ')value.erase(0,1);if(!data.empty())data+='\n';data+=value;}if(line_end==std::string::npos)break;at=line_end+1;}
            if(data.empty()){if(final&&pending_.empty())return done_?FeedResult::done:FeedResult::more;continue;}
            if(data=="[DONE]"){done_=true;return FeedResult::done;}
            auto root=json::parse(data);auto choices=root.get("choices");if(!choices||!choices->array()||choices->array()->empty())continue;auto delta=choices->array()->front().get("delta");auto content=delta?delta->get("content"):nullptr;if(content&&content->string()&&!content->string()->empty())on_delta(*content->string());
            if(final&&pending_.empty())return done_?FeedResult::done:FeedResult::more;
        }
    }catch(const std::exception& e){error_=e.what();return FeedResult::error;}
}
}
