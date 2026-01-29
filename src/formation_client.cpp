#include "muxi/formation_client.hpp"
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
    userp->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

class FormationClient::Impl {
public:
    Impl(const FormationConfig& config) : client_key_(config.client_key), admin_key_(config.admin_key), timeout_(config.timeout) {
        if (!config.base_url.empty()) {
            base_url_ = config.base_url;
        } else if (!config.server_url.empty() && !config.formation_id.empty()) {
            base_url_ = config.server_url;
            while (!base_url_.empty() && base_url_.back() == '/') base_url_.pop_back();
            base_url_ += "/api/" + config.formation_id + "/v1";
        }
        while (!base_url_.empty() && base_url_.back() == '/') base_url_.pop_back();
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    ~Impl() { curl_global_cleanup(); }
    
    json request(const std::string& method, const std::string& path, const json& body, bool use_admin, const std::string& user_id) {
        CURL* curl = curl_easy_init();
        if (!curl) throw ConnectionException("Failed to initialize CURL");
        
        std::string url = base_url_ + path;
        std::string response_body;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("X-Muxi-SDK: cpp/" + VERSION).c_str());
        headers = curl_slist_append(headers, ("X-Muxi-Client: cpp/" + VERSION).c_str());
        headers = curl_slist_append(headers, ("X-Muxi-Idempotency-Key: " + generate_uuid()).c_str());
        headers = curl_slist_append(headers, "Accept: application/json");
        
        if (use_admin) headers = curl_slist_append(headers, ("X-MUXI-ADMIN-KEY: " + admin_key_).c_str());
        else headers = curl_slist_append(headers, ("X-MUXI-CLIENT-KEY: " + client_key_).c_str());
        if (!user_id.empty()) headers = curl_slist_append(headers, ("X-Muxi-User-ID: " + user_id).c_str());
        
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
        
        if (res != CURLE_OK) throw ConnectionException(curl_easy_strerror(res));
        if (http_code >= 400) {
            std::string code, message = "Unknown error";
            try {
                auto err = json::parse(response_body);
                if (err.contains("code")) code = err["code"];
                if (err.contains("message")) message = err["message"];
            } catch (...) {}
            throw map_error(static_cast<int>(http_code), code, message);
        }
        
        if (response_body.empty()) return json::object();
        json parsed = json::parse(response_body);
        return unwrap_envelope(parsed);
    }
    
    void stream_sse(const std::string& method, const std::string& path, const json& body, bool use_admin, const std::string& user_id, std::function<void(const SseEvent&)> handler) {
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
        if (use_admin) headers = curl_slist_append(headers, ("X-MUXI-ADMIN-KEY: " + admin_key_).c_str());
        else headers = curl_slist_append(headers, ("X-MUXI-CLIENT-KEY: " + client_key_).c_str());
        if (!user_id.empty()) headers = curl_slist_append(headers, ("X-Muxi-User-ID: " + user_id).c_str());
        
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
        
        std::istringstream stream(response_body);
        std::string line, current_event;
        std::vector<std::string> data_parts;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) {
                if (!data_parts.empty()) {
                    std::string data;
                    for (size_t i = 0; i < data_parts.size(); i++) { if (i > 0) data += "\n"; data += data_parts[i]; }
                    handler(SseEvent{current_event.empty() ? "message" : current_event, data});
                    data_parts.clear();
                }
                current_event.clear();
                continue;
            }
            if (line[0] == ':') continue;
            if (line.rfind("event:", 0) == 0) { current_event = line.substr(6); while (!current_event.empty() && current_event[0] == ' ') current_event.erase(0, 1); }
            else if (line.rfind("data:", 0) == 0) { std::string d = line.substr(5); while (!d.empty() && d[0] == ' ') d.erase(0, 1); data_parts.push_back(d); }
        }
    }
    
private:
    json unwrap_envelope(const json& obj) {
        if (obj.is_object() && obj.contains("data")) {
            auto data = obj["data"];
            if (data.is_object()) {
                json result = data;
                if (obj.contains("request") && obj["request"].is_object() && obj["request"].contains("id")) {
                    if (!result.contains("request_id")) result["request_id"] = obj["request"]["id"];
                }
                return result;
            }
            return data;
        }
        return obj;
    }
    
    std::string base_url_;
    std::string client_key_;
    std::string admin_key_;
    int timeout_;
};

FormationClient::FormationClient(const FormationConfig& config) : impl_(std::make_unique<Impl>(config)) {}
FormationClient::~FormationClient() = default;

json FormationClient::health() { return impl_->request("GET", "/health", nullptr, false, ""); }
json FormationClient::get_status() { return impl_->request("GET", "/status", nullptr, true, ""); }
json FormationClient::get_config() { return impl_->request("GET", "/config", nullptr, true, ""); }
json FormationClient::get_formation_info() { return impl_->request("GET", "/formation", nullptr, true, ""); }
json FormationClient::get_agents() { return impl_->request("GET", "/agents", nullptr, true, ""); }
json FormationClient::get_agent(const std::string& id) { return impl_->request("GET", "/agents/" + id, nullptr, true, ""); }
json FormationClient::get_mcp_servers() { return impl_->request("GET", "/mcp/servers", nullptr, true, ""); }
json FormationClient::get_mcp_server(const std::string& id) { return impl_->request("GET", "/mcp/servers/" + id, nullptr, true, ""); }
json FormationClient::get_mcp_tools() { return impl_->request("GET", "/mcp/tools", nullptr, true, ""); }
json FormationClient::get_secrets() { return impl_->request("GET", "/secrets", nullptr, true, ""); }
json FormationClient::get_secret(const std::string& key) { return impl_->request("GET", "/secrets/" + key, nullptr, true, ""); }
void FormationClient::set_secret(const std::string& key, const std::string& value) { impl_->request("PUT", "/secrets/" + key, {{"value", value}}, true, ""); }
void FormationClient::delete_secret(const std::string& key) { impl_->request("DELETE", "/secrets/" + key, nullptr, true, ""); }
json FormationClient::chat(const json& payload, const std::string& user_id) { return impl_->request("POST", "/chat", payload, false, user_id); }
void FormationClient::chat_stream(const json& payload, const std::string& user_id, std::function<void(const SseEvent&)> handler) {
    json p = payload; p["stream"] = true;
    impl_->stream_sse("POST", "/chat", p, false, user_id, handler);
}
json FormationClient::audio_chat(const json& payload, const std::string& user_id) { return impl_->request("POST", "/audiochat", payload, false, user_id); }
void FormationClient::audio_chat_stream(const json& payload, const std::string& user_id, std::function<void(const SseEvent&)> handler) {
    json p = payload; p["stream"] = true;
    impl_->stream_sse("POST", "/audiochat", p, false, user_id, handler);
}
json FormationClient::get_sessions(const std::string& user_id, int limit) {
    std::string path = "/sessions?user_id=" + user_id;
    if (limit > 0) path += "&limit=" + std::to_string(limit);
    return impl_->request("GET", path, nullptr, false, user_id);
}
json FormationClient::get_session(const std::string& session_id, const std::string& user_id) { return impl_->request("GET", "/sessions/" + session_id, nullptr, false, user_id); }
json FormationClient::get_session_messages(const std::string& session_id, const std::string& user_id) { return impl_->request("GET", "/sessions/" + session_id + "/messages", nullptr, false, user_id); }
json FormationClient::get_memory_config() { return impl_->request("GET", "/memory", nullptr, true, ""); }
json FormationClient::get_memories(const std::string& user_id, int limit) {
    std::string path = "/memories?user_id=" + user_id;
    if (limit > 0) path += "&limit=" + std::to_string(limit);
    return impl_->request("GET", path, nullptr, false, user_id);
}
json FormationClient::add_memory(const std::string& user_id, const std::string& type, const std::string& detail) {
    return impl_->request("POST", "/memories", {{"user_id", user_id}, {"type", type}, {"detail", detail}}, false, user_id);
}
void FormationClient::delete_memory(const std::string& user_id, const std::string& memory_id) {
    impl_->request("DELETE", "/memories/" + memory_id + "?user_id=" + user_id, nullptr, false, user_id);
}
json FormationClient::get_buffer_stats() { return impl_->request("GET", "/memory/stats", nullptr, true, ""); }
json FormationClient::get_scheduler_config() { return impl_->request("GET", "/scheduler", nullptr, true, ""); }
json FormationClient::get_scheduler_jobs(const std::string& user_id) { return impl_->request("GET", "/scheduler/jobs?user_id=" + user_id, nullptr, true, ""); }
json FormationClient::get_scheduler_job(const std::string& job_id) { return impl_->request("GET", "/scheduler/jobs/" + job_id, nullptr, true, ""); }
json FormationClient::create_scheduler_job(const std::string& type, const std::string& schedule, const std::string& message, const std::string& user_id) {
    return impl_->request("POST", "/scheduler/jobs", {{"type", type}, {"schedule", schedule}, {"message", message}, {"user_id", user_id}}, true, "");
}
void FormationClient::delete_scheduler_job(const std::string& job_id) { impl_->request("DELETE", "/scheduler/jobs/" + job_id, nullptr, true, ""); }
json FormationClient::get_async_config() { return impl_->request("GET", "/async", nullptr, true, ""); }
json FormationClient::get_a2a_config() { return impl_->request("GET", "/a2a", nullptr, true, ""); }
json FormationClient::get_logging_config() { return impl_->request("GET", "/logging", nullptr, true, ""); }
json FormationClient::get_overlord_config() { return impl_->request("GET", "/overlord", nullptr, true, ""); }
json FormationClient::get_llm_settings() { return impl_->request("GET", "/llm/settings", nullptr, true, ""); }
json FormationClient::get_triggers() { return impl_->request("GET", "/triggers", nullptr, false, ""); }
json FormationClient::get_trigger(const std::string& name) { return impl_->request("GET", "/triggers/" + name, nullptr, false, ""); }
json FormationClient::fire_trigger(const std::string& name, const json& data, bool is_async, const std::string& user_id) {
    std::string path = "/triggers/" + name + "?async=" + (is_async ? "true" : "false");
    return impl_->request("POST", path, data, false, user_id);
}
json FormationClient::get_audit_log() { return impl_->request("GET", "/audit", nullptr, true, ""); }
void FormationClient::stream_events(const std::string& user_id, std::function<void(const SseEvent&)> handler) {
    impl_->stream_sse("GET", "/events?user_id=" + user_id, nullptr, false, user_id, handler);
}
void FormationClient::stream_logs(const json& filters, std::function<void(const SseEvent&)> handler) {
    std::string path = "/logs";
    if (!filters.is_null() && filters.is_object()) {
        path += "?";
        for (auto& [k, v] : filters.items()) path += k + "=" + v.get<std::string>() + "&";
        path.pop_back();
    }
    impl_->stream_sse("GET", path, nullptr, true, "", handler);
}
json FormationClient::resolve_user(const std::string& identifier, bool create_user) {
    return impl_->request("POST", "/users/resolve", {{"identifier", identifier}, {"create_user", create_user}}, false, "");
}

} // namespace muxi
