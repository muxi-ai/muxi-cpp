#include "muxi/server_client.hpp"
#include "muxi/auth.hpp"
#include "muxi/errors.hpp"
#include "muxi/version.hpp"
#include <curl/curl.h>
#include <sstream>
#include <random>

namespace muxi {

static std::string generate_uuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";
    
    std::string uuid(36, '-');
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) continue;
        uuid[i] = hex[dis(gen)];
    }
    uuid[14] = '4';
    uuid[19] = hex[(dis(gen) & 0x3) | 0x8];
    return uuid;
}

static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total = size * nmemb;
    userp->append(static_cast<char*>(contents), total);
    return total;
}

class Transport {
public:
    Transport(const std::string& base_url, int timeout) 
        : base_url_(base_url), timeout_(timeout) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    
    ~Transport() {
        curl_global_cleanup();
    }
    
    json request(const std::string& method, const std::string& path, 
                 const json& body, const std::map<std::string, std::string>& headers) {
        CURL* curl = curl_easy_init();
        if (!curl) throw ConnectionException("Failed to initialize CURL");
        
        std::string url = base_url_ + path;
        std::string response_body;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        
        struct curl_slist* header_list = nullptr;
        for (const auto& [key, value] : headers) {
            std::string header = key + ": " + value;
            header_list = curl_slist_append(header_list, header.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
        
        std::string body_str;
        if (!body.is_null()) {
            body_str = body.dump();
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
        }
        
        if (method == "POST") curl_easy_setopt(curl, CURLOPT_POST, 1L);
        else if (method == "PUT") curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        else if (method == "DELETE") curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        
        CURLcode res = curl_easy_perform(curl);
        
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        curl_slist_free_all(header_list);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            throw ConnectionException(curl_easy_strerror(res));
        }
        
        if (http_code >= 400) {
            std::string code, message = "Unknown error";
            try {
                auto err = json::parse(response_body);
                if (err.contains("code")) code = err["code"].get<std::string>();
                else if (err.contains("error")) code = err["error"].get<std::string>();
                if (err.contains("message")) message = err["message"].get<std::string>();
            } catch (...) {}
            throw map_error(static_cast<int>(http_code), code, message);
        }
        
        return response_body.empty() ? json::object() : json::parse(response_body);
    }
    
    void stream_sse(const std::string& method, const std::string& path,
                    const json& body, const std::map<std::string, std::string>& headers,
                    std::function<void(const SseEvent&)> handler) {
        CURL* curl = curl_easy_init();
        if (!curl) throw ConnectionException("Failed to initialize CURL");
        
        std::string url = base_url_ + path;
        std::string response_body;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);  // No timeout for streaming
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        
        struct curl_slist* header_list = nullptr;
        for (const auto& [key, value] : headers) {
            std::string header = key + ": " + value;
            header_list = curl_slist_append(header_list, header.c_str());
        }
        header_list = curl_slist_append(header_list, "Accept: text/event-stream");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
        
        std::string body_str;
        if (!body.is_null()) {
            body_str = body.dump();
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
        }
        
        if (method == "POST") curl_easy_setopt(curl, CURLOPT_POST, 1L);
        
        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(header_list);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) return;
        
        // Parse SSE events
        std::istringstream stream(response_body);
        std::string line;
        std::string current_event;
        std::vector<std::string> data_parts;
        
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            
            if (line.empty()) {
                if (!data_parts.empty()) {
                    std::string data;
                    for (size_t i = 0; i < data_parts.size(); i++) {
                        if (i > 0) data += "\n";
                        data += data_parts[i];
                    }
                    handler(SseEvent{current_event.empty() ? "message" : current_event, data});
                    data_parts.clear();
                }
                current_event.clear();
                continue;
            }
            
            if (line[0] == ':') continue;
            
            if (line.rfind("event:", 0) == 0) {
                current_event = line.substr(6);
                while (!current_event.empty() && current_event[0] == ' ') current_event.erase(0, 1);
            } else if (line.rfind("data:", 0) == 0) {
                std::string data = line.substr(5);
                while (!data.empty() && data[0] == ' ') data.erase(0, 1);
                data_parts.push_back(data);
            }
        }
    }

private:
    std::string base_url_;
    int timeout_;
};

// Helper to create common headers
static std::map<std::string, std::string> make_headers(bool has_body) {
    std::map<std::string, std::string> headers;
    headers["X-Muxi-SDK"] = "cpp/" + VERSION;
    headers["X-Muxi-Client"] = "cpp/" + VERSION;
    headers["X-Muxi-Idempotency-Key"] = generate_uuid();
    headers["Accept"] = "application/json";
    if (has_body) headers["Content-Type"] = "application/json";
    return headers;
}

} // namespace muxi
