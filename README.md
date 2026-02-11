# MUXI C++ SDK

Official C++ SDK for [MUXI](https://muxi.ai) — infrastructure for AI agents.

**Highlights**
- Modern C++17 with `libcurl` and `nlohmann/json`
- Built-in retries, idempotency, and typed errors
- Streaming helpers for chat/audio and deploy/log tails

> Need deeper usage notes? See the [User Guide](https://github.com/muxi-ai/muxi-cpp/blob/main/USER_GUIDE.md) for streaming, retries, and auth details.

## Requirements

- C++17 compiler (GCC 8+, Clang 7+, MSVC 2019+)
- CMake 3.14+
- libcurl
- OpenSSL
- nlohmann/json

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

### Manual Build

```bash
git clone https://github.com/muxi-ai/muxi-cpp.git
cd muxi-cpp
mkdir build && cd build
cmake ..
make
sudo make install
```

## Quick Start

### Server Management (Control Plane)

```cpp
#include <muxi/muxi.hpp>
#include <iostream>

int main() {
    muxi::ServerConfig config;
    config.url = std::getenv("MUXI_SERVER_URL");
    config.key_id = std::getenv("MUXI_KEY_ID");
    config.secret_key = std::getenv("MUXI_SECRET_KEY");

    muxi::ServerClient server(config);

    // List formations
    auto formations = server.list_formations();
    std::cout << formations.dump(2) << std::endl;

    // Get server status
    auto status = server.status();
    std::cout << "Status: " << status.dump() << std::endl;

    return 0;
}
```

### Formation Usage (Runtime API)

```cpp
#include <muxi/muxi.hpp>
#include <iostream>

int main() {
    // Connect via server proxy
    muxi::FormationConfig config;
    config.formation_id = "my-bot";
    config.server_url = std::getenv("MUXI_SERVER_URL");
    config.client_key = std::getenv("MUXI_CLIENT_KEY");
    config.admin_key = std::getenv("MUXI_ADMIN_KEY");

    muxi::FormationClient client(config);

    // Or connect directly to formation
    muxi::FormationConfig direct_config;
    direct_config.url = "http://localhost:8001";
    direct_config.client_key = std::getenv("MUXI_CLIENT_KEY");
    direct_config.admin_key = std::getenv("MUXI_ADMIN_KEY");

    muxi::FormationClient direct_client(direct_config);

    // Chat (non-streaming)
    nlohmann::json payload = {{"message", "Hello!"}};
    auto response = client.chat(payload, "user123");
    std::cout << response.dump() << std::endl;

    // Health check
    auto health = client.health();
    std::cout << "Status: " << health.dump() << std::endl;

    return 0;
}
```

## Auth & Headers

- **Server**: HMAC with `key_id`/`secret_key` on `/rpc/*` endpoints
- **Formation**: `X-MUXI-CLIENT-KEY` or `X-MUXI-ADMIN-KEY` headers
- **Idempotency**: `X-Muxi-Idempotency-Key` auto-generated on every request
- **SDK**: `X-Muxi-SDK`, `X-Muxi-Client` headers set automatically

## Error Handling

```cpp
#include <muxi/muxi.hpp>

try {
    auto result = server.get_formation("nonexistent");
} catch (const muxi::NotFoundException& e) {
    std::cerr << "Not found: " << e.what() << std::endl;
} catch (const muxi::AuthenticationException& e) {
    std::cerr << "Auth failed: " << e.what() << std::endl;
} catch (const muxi::RateLimitException& e) {
    std::cerr << "Rate limited. Retry after: " << e.retry_after() << "s" << std::endl;
} catch (const muxi::MuxiException& e) {
    std::cerr << "Error: " << e.what() << " (" << e.status_code() << ")" << std::endl;
}
```

## Configuration

```cpp
muxi::ServerConfig config;
config.url = "https://muxi.example.com:7890";
config.key_id = "your-key-id";
config.secret_key = "your-secret-key";
config.timeout = 30;       // Request timeout in seconds
config.max_retries = 3;    // Retry on 429/5xx errors
config.debug = true;       // Enable debug logging

muxi::ServerClient server(config);
```

## License

Licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) for details.

## Links

- [MUXI SDKs](https://github.com/muxi-ai/sdks)
- [MUXI Documentation](https://docs.muxi.ai)
- [GitHub](https://github.com/muxi-ai/muxi-cpp)
