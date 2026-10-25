#pragma once
#include <functional>
#include <string>
#include <string_view>

namespace krmemory {
class SseParser {
public:
    enum class FeedResult { more, done, error };
    using DeltaCallback = std::function<void(std::string_view)>;
    FeedResult feed(std::string_view bytes,const DeltaCallback& on_delta);
    FeedResult finish(const DeltaCallback& on_delta);
    std::string error() const { return error_; }
private:
    FeedResult drain(bool final,const DeltaCallback& on_delta);
    std::string pending_,error_;
    bool done_{};
};
}
