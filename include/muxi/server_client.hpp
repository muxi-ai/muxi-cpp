#pragma once

#include <string>
#include <functional>
#include <nlohmann/json.hpp>

namespace muxi {

using json = nlohmann::json;

struct ServerConfig {
    std::string url;
    std::string key_id;
    std::string secret_key;
    int timeout = 30;
    int max_retries = 0;
};

struct SseEvent {
    std::string event;
    std::string data;
};

class ServerClient {
public:
    explicit ServerClient(const ServerConfig& config);
    ~ServerClient();
    
    json health();
    json status();
    json list_formations();
    json get_formation(const std::string& formation_id);
    json stop_formation(const std::string& formation_id);
    json start_formation(const std::string& formation_id);
    json restart_formation(const std::string& formation_id);
    json rollback_formation(const std::string& formation_id);
    json delete_formation(const std::string& formation_id);
    json cancel_update(const std::string& formation_id);
    json deploy_formation(const std::string& formation_id, const json& payload);
    json update_formation(const std::string& formation_id, const json& payload);
    json get_formation_logs(const std::string& formation_id, int limit = -1);
    json get_server_logs(int limit = -1);
    
    void deploy_formation_stream(const std::string& formation_id, const json& payload, std::function<void(const SseEvent&)> handler);
    void stream_formation_logs(const std::string& formation_id, std::function<void(const SseEvent&)> handler);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace muxi
