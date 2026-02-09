#pragma once

#include <string>
#include <functional>
#include <optional>
#include <nlohmann/json.hpp>
#include "server_client.hpp"

namespace muxi {

using json = nlohmann::json;

struct FormationConfig {
    std::string server_url;
    std::string formation_id;
    std::string base_url;
    std::string client_key;
    std::string admin_key;
    int timeout = 30;
    std::string mode = "live";  // "live" (default) or "draft" for local dev
    std::string app_;  // Internal: for Console telemetry
};

class FormationClient {
public:
    explicit FormationClient(const FormationConfig& config);
    ~FormationClient();
    
    // Health / Status
    json health();
    json get_status();
    json get_config();
    json get_formation_info();
    
    // Agents / MCP
    json get_agents();
    json get_agent(const std::string& agent_id);
    json get_mcp_servers();
    json get_mcp_server(const std::string& server_id);
    json get_mcp_tools();
    
    // Secrets
    json get_secrets();
    json get_secret(const std::string& key);
    void set_secret(const std::string& key, const std::string& value);
    void delete_secret(const std::string& key);
    
    // Chat
    json chat(const json& payload, const std::string& user_id = "");
    void chat_stream(const json& payload, const std::string& user_id, std::function<void(const SseEvent&)> handler);
    json audio_chat(const json& payload, const std::string& user_id = "");
    void audio_chat_stream(const json& payload, const std::string& user_id, std::function<void(const SseEvent&)> handler);
    
    // Sessions
    json get_sessions(const std::string& user_id, int limit = -1);
    json get_session(const std::string& session_id, const std::string& user_id);
    json get_session_messages(const std::string& session_id, const std::string& user_id);
    
    // Memory
    json get_memory_config();
    json get_memories(const std::string& user_id, int limit = -1);
    json add_memory(const std::string& user_id, const std::string& type, const std::string& detail);
    void delete_memory(const std::string& user_id, const std::string& memory_id);
    json get_buffer_stats();
    
    // Scheduler
    json get_scheduler_config();
    json get_scheduler_jobs(const std::string& user_id);
    json get_scheduler_job(const std::string& job_id);
    json create_scheduler_job(const std::string& type, const std::string& schedule, const std::string& message, const std::string& user_id);
    void delete_scheduler_job(const std::string& job_id);
    
    // Config endpoints
    json get_async_config();
    json get_a2a_config();
    json get_logging_config();
    json get_overlord_config();
    json get_llm_settings();
    
    // Triggers / Audit
    json get_triggers();
    json get_trigger(const std::string& name);
    json fire_trigger(const std::string& name, const json& data, bool is_async = false, const std::string& user_id = "");
    json get_audit_log();
    
    // Streaming
    void stream_events(const std::string& user_id, std::function<void(const SseEvent&)> handler);
    void stream_logs(const json& filters, std::function<void(const SseEvent&)> handler);
    
    // Resolve user
    json resolve_user(const std::string& identifier, bool create_user = false);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace muxi
