#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace muxi {

using json = nlohmann::json;

struct ContentItem {
    std::string type;
    std::string text;
    std::string url;
};

struct ErrorInfo {
    std::string code;
    std::string message;
};

struct ClarificationInfo {
    std::string question;
    std::vector<std::string> options;
};

struct WebhookEvent {
    std::string request_id;
    std::string session_id;
    std::string user_id;
    std::string status;
    std::vector<ContentItem> content;
    std::optional<ErrorInfo> error;
    std::optional<ClarificationInfo> clarification;
    std::string timestamp;
};

class Webhook {
public:
    static bool verify_signature(const std::string& payload, const std::string& header, const std::string& secret, int tolerance = 300);
    static WebhookEvent parse(const std::string& payload);
};

} // namespace muxi
