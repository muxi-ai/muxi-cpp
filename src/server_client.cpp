#include "muxi/server_client.hpp"
#include "muxi/auth.hpp"
#include "muxi/errors.hpp"
#include "muxi/version.hpp"
#include "muxi/version_check.hpp"
#include <curl/curl.h>
#include <sstream>
#include <random>
#include <unordered_map>

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
    userp->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

static size_t header_callback(char* buffer, size_t size, size_t nitems, std::unordered_map<std::string, std::string>* headers) {
    size_t total = size * nitems;
    std::string line(buffer, total);
    size_t colon = line.find(':');
    if (colon != std::string::npos) {
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(0, 1);
        while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) value.pop_back();
        (*headers)[key] = value;
    }
    return total;
}

class ServerClient::Impl {
public:
    Impl(const ServerConfig& config) 
        : base_url_(config.url), key_id_(config.key_id), 
          secret_key_(config.secret_key), app_(config.app_), timeout_(config.timeout) {
        while (!base_url_.empty() && base_url_.back() == '/') base_url_.pop_back();
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    
    ~Impl() { curl_global_cleanup(); }
    
    json request(const std::string& method, const std::string& path, const json& body = nullptr, bool auth = true) {
        CURL* curl = curl_easy_init();
        if (!curl) throw ConnectionException("Failed to initialize CURL");
        
        std::string url = base_url_ + path;
        std::string response_body;
        std::unordered_map<std::string, std::string> response_headers;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_headers);
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("X-Muxi-SDK: cpp/" + VERSION).c_str());
        headers = curl_slist_append(headers, ("X-Muxi-Client: cpp/" + VERSION).c_str());
        headers = curl_slist_append(headers, ("X-Muxi-Idempotency-Key: " + generate_uuid()).c_str());
        headers = curl_slist_append(headers, "Accept: application/json");
        if (!app_.empty()) headers = curl_slist_append(headers, ("X-Muxi-App: " + app_).c_str());
        
        if (auth) {
            headers = curl_slist_append(headers, ("Authorization: " + Auth::build_auth_header(key_id_, secret_key_, method, path)).c_str());
        }
        
        std::string body_str;
        if (!body.is_null()) {
            body_str = body.dump();
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
            headers = curl_slist_append(headers, "Content-Type: application/json");
        }
        
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        if (method == "POST") curl_easy_setopt(curl, CURLOPT_POST, 1L);
        else if (method == "PUT") curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        else if (method == "DELETE") curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        
        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        // Check for SDK updates (non-blocking, once per process)
        internal::check_for_updates(response_headers);
        
        if (res != CURLE_OK) throw ConnectionException(curl_easy_strerror(res));
        
        if (http_code >= 400) {
            std::string code, message = "Unknown error";
            try {
                auto err = json::parse(response_body);
                if (err.contains("code")) code = err["code"];
                else if (err.contains("error")) code = err["error"];
                if (err.contains("message")) message = err["message"];
            } catch (...) {}
            throw map_error(static_cast<int>(http_code), code, message);
        }
        
        return response_body.empty() ? json::object() : json::parse(response_body);
    }
    
    void stream_sse(const std::string& method, const std::string& path, 
                    const json& body, std::function<void(const SseEvent&)> handler) {
        CURL* curl = curl_easy_init();
        if (!curl) return;
        
        std::string url = base_url_ + path;
        std::string response_body;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("X-Muxi-SDK: cpp/" + VERSION).c_str());
        headers = curl_slist_append(headers, "Accept: text/event-stream");
        headers = curl_slist_append(headers, ("Authorization: " + Auth::build_auth_header(key_id_, secret_key_, method, path)).c_str());
        
        std::string body_str;
        if (!body.is_null()) {
            body_str = body.dump();
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
            headers = curl_slist_append(headers, "Content-Type: application/json");
        }
        
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        if (method == "POST") curl_easy_setopt(curl, CURLOPT_POST, 1L);
        
        curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        // Parse SSE
        std::istringstream stream(response_body);
        std::string line, current_event;
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
                std::string d = line.substr(5);
                while (!d.empty() && d[0] == ' ') d.erase(0, 1);
                data_parts.push_back(d);
            }
        }
    }
    
private:
    std::string base_url_;
    std::string key_id_;
    std::string secret_key_;
    std::string app_;
    int timeout_;
};

ServerClient::ServerClient(const ServerConfig& config) : impl_(std::make_unique<Impl>(config)) {}
ServerClient::~ServerClient() = default;

json ServerClient::health() { return impl_->request("GET", "/health", nullptr, false); }
json ServerClient::status() { return impl_->request("GET", "/rpc/server/status"); }
json ServerClient::list_formations() { return impl_->request("GET", "/rpc/formations"); }
json ServerClient::get_formation(const std::string& id) { return impl_->request("GET", "/rpc/formations/" + id); }
json ServerClient::stop_formation(const std::string& id) { return impl_->request("POST", "/rpc/formations/" + id + "/stop", json::object()); }
json ServerClient::start_formation(const std::string& id) { return impl_->request("POST", "/rpc/formations/" + id + "/start", json::object()); }
json ServerClient::restart_formation(const std::string& id) { return impl_->request("POST", "/rpc/formations/" + id + "/restart", json::object()); }
json ServerClient::rollback_formation(const std::string& id) { return impl_->request("POST", "/rpc/formations/" + id + "/rollback", json::object()); }
json ServerClient::delete_formation(const std::string& id) { return impl_->request("DELETE", "/rpc/formations/" + id); }
json ServerClient::cancel_update(const std::string& id) { return impl_->request("POST", "/rpc/formations/" + id + "/cancel-update", json::object()); }
json ServerClient::deploy_formation(const std::string& id, const json& p) { return impl_->request("POST", "/rpc/formations/" + id + "/deploy", p); }
json ServerClient::update_formation(const std::string& id, const json& p) { return impl_->request("POST", "/rpc/formations/" + id + "/update", p); }
json ServerClient::get_formation_logs(const std::string& id, int limit) {
    std::string path = "/rpc/formations/" + id + "/logs";
    if (limit > 0) path += "?limit=" + std::to_string(limit);
    return impl_->request("GET", path);
}
json ServerClient::get_server_logs(int limit) {
    std::string path = "/rpc/server/logs";
    if (limit > 0) path += "?limit=" + std::to_string(limit);
    return impl_->request("GET", path);
}
void ServerClient::deploy_formation_stream(const std::string& id, const json& p, std::function<void(const SseEvent&)> h) {
    impl_->stream_sse("POST", "/rpc/formations/" + id + "/deploy/stream", p, h);
}
void ServerClient::stream_formation_logs(const std::string& id, std::function<void(const SseEvent&)> h) {
    impl_->stream_sse("GET", "/rpc/formations/" + id + "/logs/stream", nullptr, h);
}

} // namespace muxi
