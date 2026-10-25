#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <memory>
#include <optional>
namespace krmemory {
inline constexpr unsigned local_stream_timeout_ms=300000;
inline constexpr std::size_t local_stream_maximum_bytes=4*1024*1024;
inline constexpr unsigned local_stream_maximum_tokens=8192;
struct LocalModelStatus { bool available{}; std::string detail; };
struct LocalCompletion { bool success{}; std::string text, detail; };
struct LocalModelList { bool success{}; std::vector<std::string> models; std::string detail; };
enum class LocalStreamEnd { completed, cancelled, timeout, network_error, model_error, response_too_large };
struct LocalStreamEvent { std::string delta,detail; std::optional<LocalStreamEnd> end; };
class LocalStreamRequest {
public:
    using Callback=std::function<void(const LocalStreamEvent&)>;
    static std::shared_ptr<LocalStreamRequest> start(std::string prompt,std::string model,Callback callback,
                                                     unsigned timeout_ms=local_stream_timeout_ms,
                                                     std::size_t maximum_bytes=local_stream_maximum_bytes);
    ~LocalStreamRequest();
    void cancel();
    bool active() const;
private:
    struct Impl;explicit LocalStreamRequest(std::shared_ptr<Impl>);std::shared_ptr<Impl> impl_;
};
// Probes only the configured local OpenAI-compatible endpoint; it never falls
// back to cloud services.
LocalModelStatus probe_lm_studio(std::string host="127.0.0.1", unsigned short port=1234);
LocalModelList list_lm_studio_models(std::string host="127.0.0.1", unsigned short port=1234,
                                     unsigned timeout_ms=5000);
std::string preferred_local_model();
bool save_preferred_local_model(std::string_view model);
// Sends a completion exclusively to a loopback OpenAI-compatible endpoint.
// The caller is responsible for providing a prompt that labels retrieved data.
LocalCompletion complete_lm_studio(std::string_view prompt, std::string_view model="",
                                  std::string host="127.0.0.1", unsigned short port=1234,
                                  unsigned timeout_ms=60000);
}
