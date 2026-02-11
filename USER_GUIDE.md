# MUXI C++ SDK User Guide

## Installation

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    muxi
    GIT_REPOSITORY https://github.com/muxi-ai/muxi-cpp.git
    GIT_TAG v0.20260211.0
)
FetchContent_MakeAvailable(muxi)
target_link_libraries(your_target PRIVATE muxi)
```

### Dependencies

- libcurl (`apt install libcurl4-openssl-dev` or `brew install curl`)
- OpenSSL (`apt install libssl-dev` or `brew install openssl`)
- nlohmann/json (`apt install nlohmann-json3-dev` or `brew install nlohmann-json`)

## Clients

- **ServerClient** (management, HMAC): deploy/list/update formations, server health/status, logs
- **FormationClient** (runtime, client/admin keys): chat/audio, agents, secrets, MCP, memory, scheduler, sessions, triggers, etc.

## Quick Start

### ServerClient

```cpp
#include <muxi/muxi.hpp>

muxi::ServerConfig config;
config.url = std::getenv("MUXI_SERVER_URL");
config.key_id = std::getenv("MUXI_KEY_ID");
config.secret_key = std::getenv("MUXI_SECRET_KEY");

muxi::ServerClient server(config);
auto status = server.status();
std::cout << status.dump() << std::endl;
```

### FormationClient

```cpp
#include <muxi/muxi.hpp>

muxi::FormationConfig config;
config.formation_id = "my-bot";
config.server_url = std::getenv("MUXI_SERVER_URL");
config.client_key = std::getenv("MUXI_CLIENT_KEY");
config.admin_key = std::getenv("MUXI_ADMIN_KEY");

muxi::FormationClient client(config);

nlohmann::json payload = {{"message", "Hello"}};
auto response = client.chat(payload, "user123");
std::cout << response.dump() << std::endl;
```

## Auth & Headers

- **ServerClient**: HMAC signature (`MUXI-HMAC key=<id>, timestamp=<sec>, signature=<b64>`)
- **FormationClient**: `X-MUXI-CLIENT-KEY` required; `X-MUXI-ADMIN-KEY` for admin endpoints
- **Idempotency**: `X-Muxi-Idempotency-Key` auto-generated on every request
- **SDK Headers**: `X-Muxi-SDK: cpp/{version}`, `X-Muxi-Client: {os}/{arch}`

## Timeouts & Retries

- Default timeout: 30s (configurable)
- Retries on 429/5xx with exponential backoff
- Respects `Retry-After` header for rate limits

## Error Handling

```cpp
#include <muxi/errors.hpp>

try {
    server.get_formation("nonexistent");
} catch (const muxi::AuthenticationException& e) { /* 401 */ }
  catch (const muxi::AuthorizationException& e) { /* 403 */ }
  catch (const muxi::NotFoundException& e) { /* 404 */ }
  catch (const muxi::ValidationException& e) { /* 422 */ }
  catch (const muxi::RateLimitException& e) { /* 429 - check e.retry_after() */ }
  catch (const muxi::ServerException& e) { /* 5xx */ }
  catch (const muxi::ConnectionException& e) { /* network error */ }
  catch (const muxi::MuxiException& e) { /* base exception */ }
```

## Streaming

```cpp
// Chat streaming with callback
client.chat_stream(payload, "user123", [](const muxi::SseEvent& event) {
    std::cout << event.data;
});

// Deploy with progress
server.deploy_formation_stream(formation_id, payload, [](const muxi::SseEvent& event) {
    std::cout << event.event << ": " << event.data << std::endl;
});
```

## Webhook Verification

```cpp
#include <muxi/webhook.hpp>

// In your webhook handler
std::string payload = get_request_body();
std::string signature = get_header("X-Muxi-Signature");
std::string secret = std::getenv("WEBHOOK_SECRET");

if (!muxi::Webhook::verify_signature(payload, signature, secret)) {
    return http_response(401, "Invalid signature");
}

auto event = muxi::Webhook::parse(payload);

if (event.status == "completed") {
    for (const auto& item : event.content) {
        if (item.type == "text") {
            std::cout << item.text << std::endl;
        }
    }
} else if (event.status == "failed") {
    std::cout << "Error: " << event.error.message << std::endl;
} else if (event.status == "awaiting_clarification") {
    std::cout << "Question: " << event.clarification.question << std::endl;
}
```

## Building & Testing

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
make
ctest --output-on-failure
```

## Contributing

- Follow C++17 standards
- Use clang-format for formatting
- Preserve idempotency header injection
