# MUXI C++ SDK User Guide

## Installation

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    muxi
    GIT_REPOSITORY https://github.com/muxi-ai/muxi-cpp.git
    GIT_TAG main
)
FetchContent_MakeAvailable(muxi)

target_link_libraries(your_target PRIVATE muxi)
```

### Manual Build

```bash
git clone https://github.com/muxi-ai/muxi-cpp.git
cd muxi-cpp
mkdir build && cd build
cmake ..
cmake --build .
```

### Dependencies

- libcurl (with SSL support)
- OpenSSL
- nlohmann/json (fetched automatically via CMake)

## Quickstart

```cpp
#include <muxi/muxi.hpp>
#include <iostream>

int main() {
    // Server client (management, HMAC auth)
    muxi::ServerClient server(
        "https://server.example.com",
        "<key_id>",
        "<secret_key>"
    );
    std::cout << server.status().dump() << std::endl;

    // Formation client (runtime, key auth)
    muxi::FormationClient formation(
        "https://server.example.com",
        "<formation_id>",
        "<client_key>",
        "<admin_key>"
    );
    std::cout << formation.health().dump() << std::endl;

    return 0;
}
```

## Clients

- **ServerClient** (management, HMAC): deploy/list/update formations, server health/status, server logs.
- **FormationClient** (runtime, client/admin keys): chat/audio (streaming), agents, secrets, MCP, memory, scheduler, sessions/requests, identifiers, credentials, triggers/SOPs/audit, async/A2A/logging config, overlord/LLM settings, events/logs streaming.

## Streaming

```cpp
#include <muxi/muxi.hpp>

// Chat streaming
nlohmann::json request = {{"message", "Tell me a story"}};

formation.chat_stream(request, "user-123", [](const muxi::SseEvent& event) {
    if (event.event == "message") {
        std::cout << event.data << std::endl;
    }
});

// Event streaming
formation.stream_events("user-123", [](const muxi::SseEvent& event) {
    std::cout << event.data << std::endl;
});

// Log streaming (admin)
formation.stream_logs("info", [](const muxi::SseEvent& event) {
    std::cout << event.data << std::endl;
});
```

## Auth & Headers

- **ServerClient**: HMAC with `key_id`/`secret_key` on `/rpc` endpoints.
- **FormationClient**: `X-MUXI-CLIENT-KEY` or `X-MUXI-ADMIN-KEY` on `/api/{formation}/v1`. Override `base_url` for direct access (e.g., `http://localhost:9012/v1`).
- **Idempotency**: `X-Muxi-Idempotency-Key` auto-generated on every request.
- **SDK headers**: `X-Muxi-SDK`, `X-Muxi-Client` set automatically.

## Timeouts & Retries

- Default timeout: 30s (no timeout for streaming).
- Retries: `max_retries` with exponential backoff on 429/5xx/connection errors; respects `Retry-After`.

## Error Handling

```cpp
#include <muxi/errors.hpp>

try {
    auto response = formation.chat(request, "user-123");
} catch (const muxi::AuthenticationException& e) {
    std::cerr << "Auth failed: " << e.what() << std::endl;
} catch (const muxi::RateLimitException& e) {
    std::cerr << "Rate limited. Retry after: " << e.retry_after().value_or(0) << "s" << std::endl;
} catch (const muxi::NotFoundException& e) {
    std::cerr << "Not found: " << e.what() << std::endl;
} catch (const muxi::MuxiException& e) {
    std::cerr << e.error_code() << ": " << e.what() << " (" << e.status_code() << ")" << std::endl;
}
```

Error types: `AuthenticationException`, `AuthorizationException`, `NotFoundException`, `ValidationException`, `RateLimitException`, `ServerException`, `ConflictException`, `ConnectionException`.

## Notable Endpoints (FormationClient)

| Category | Methods |
|----------|---------|
| Chat/Audio | `chat`, `chat_stream`, `audio_chat`, `audio_chat_stream` |
| Memory | `get_memory_config`, `get_memories`, `add_memory`, `delete_memory`, `get_user_buffer`, `clear_user_buffer`, `clear_session_buffer`, `clear_all_buffers`, `get_buffer_stats` |
| Scheduler | `get_scheduler_config`, `get_scheduler_jobs`, `get_scheduler_job`, `create_scheduler_job`, `delete_scheduler_job` |
| Sessions | `get_sessions`, `get_session`, `get_session_messages`, `restore_session` |
| Requests | `get_requests`, `get_request_status`, `cancel_request` |
| Agents/MCP | `get_agents`, `get_agent`, `get_mcp_servers`, `get_mcp_server`, `get_mcp_tools` |
| Secrets | `get_secrets`, `get_secret`, `set_secret`, `delete_secret` |
| Credentials | `list_credential_services`, `list_credentials`, `get_credential`, `create_credential`, `delete_credential` |
| Identifiers | `get_user_identifiers_for_user`, `link_user_identifier`, `unlink_user_identifier` |
| Triggers/SOP | `get_triggers`, `get_trigger`, `fire_trigger`, `get_sops`, `get_sop` |
| Audit | `get_audit_log`, `clear_audit_log` |
| Config | `get_status`, `get_config`, `get_formation_info`, `get_async_config`, `get_a2a_config`, `get_logging_config`, `get_logging_destinations`, `get_overlord_config`, `get_overlord_persona`, `get_llm_settings` |
| Streaming | `stream_events`, `stream_logs`, `stream_request` |
| User | `resolve_user` |

## Webhook Verification

```cpp
#include <muxi/webhook.hpp>

// In your HTTP handler
void handle_webhook(const std::string& payload, const std::string& signature) {
    std::string secret = std::getenv("WEBHOOK_SECRET") ? std::getenv("WEBHOOK_SECRET") : "";

    if (!muxi::Webhook::verify_signature(payload, signature, secret)) {
        // Return 401
        return;
    }

    auto event = muxi::Webhook::parse(payload);

    if (event.status == "completed") {
        for (const auto& item : event.content) {
            if (item.type == "text") {
                std::cout << item.text << std::endl;
            }
        }
    } else if (event.status == "failed") {
        if (event.error) {
            std::cout << "Error: " << event.error->message << std::endl;
        }
    } else if (event.status == "awaiting_clarification") {
        if (event.clarification) {
            std::cout << "Question: " << event.clarification->question << std::endl;
        }
    }
}
```

## Testing Locally

```bash
cd cpp
mkdir build && cd build
cmake ..
cmake --build .
ctest
```
