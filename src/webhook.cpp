#include "muxi/webhook.hpp"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace muxi {

bool Webhook::verify_signature(const std::string& payload, const std::string& header, const std::string& secret, int tolerance) {
    if (secret.empty()) throw std::invalid_argument("Webhook secret is required");
    if (header.empty()) return false;
    
    std::string timestamp, signature;
    std::istringstream iss(header);
    std::string part;
    while (std::getline(iss, part, ',')) {
        auto pos = part.find('=');
        if (pos != std::string::npos) {
            std::string key = part.substr(0, pos);
            std::string value = part.substr(pos + 1);
            while (!key.empty() && key[0] == ' ') key.erase(0, 1);
            while (!value.empty() && value[0] == ' ') value.erase(0, 1);
            if (key == "t") timestamp = value;
            else if (key == "v1") signature = value;
        }
    }
    
    if (timestamp.empty() || signature.empty()) return false;
    
    long long ts;
    try { ts = std::stoll(timestamp); } catch (...) { return false; }
    
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto now_secs = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
    if (std::abs(now_secs - ts) > tolerance) return false;
    
    std::string message = timestamp + "." + payload;
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    
    HMAC(EVP_sha256(),
         secret.c_str(), static_cast<int>(secret.length()),
         reinterpret_cast<const unsigned char*>(message.c_str()), message.length(),
         hash, &hash_len);
    
    std::ostringstream expected;
    for (unsigned int i = 0; i < hash_len; i++) {
        expected << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
    }
    
    return expected.str() == signature;
}

WebhookEvent Webhook::parse(const std::string& payload) {
    auto j = json::parse(payload);
    WebhookEvent event;
    
    if (j.contains("requestId")) event.request_id = j["requestId"];
    if (j.contains("sessionId")) event.session_id = j["sessionId"];
    if (j.contains("userId")) event.user_id = j["userId"];
    if (j.contains("status")) event.status = j["status"];
    if (j.contains("timestamp")) event.timestamp = j["timestamp"];
    
    if (j.contains("content") && j["content"].is_array()) {
        for (const auto& item : j["content"]) {
            ContentItem ci;
            if (item.contains("type")) ci.type = item["type"];
            if (item.contains("text")) ci.text = item["text"];
            if (item.contains("url")) ci.url = item["url"];
            event.content.push_back(ci);
        }
    }
    
    if (j.contains("error") && j["error"].is_object()) {
        ErrorInfo err;
        if (j["error"].contains("code")) err.code = j["error"]["code"];
        if (j["error"].contains("message")) err.message = j["error"]["message"];
        event.error = err;
    }
    
    if (j.contains("clarification") && j["clarification"].is_object()) {
        ClarificationInfo clar;
        if (j["clarification"].contains("question")) clar.question = j["clarification"]["question"];
        if (j["clarification"].contains("options") && j["clarification"]["options"].is_array()) {
            for (const auto& opt : j["clarification"]["options"]) {
                clar.options.push_back(opt.get<std::string>());
            }
        }
        event.clarification = clar;
    }
    
    return event;
}

} // namespace muxi
