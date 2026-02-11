#pragma once

#include <string>
#include <utility>

namespace muxi {

class Auth {
public:
    static std::pair<std::string, std::string> generate_hmac_signature(
        const std::string& secret_key,
        const std::string& method,
        const std::string& path
    );
    
    static std::string build_auth_header(
        const std::string& key_id,
        const std::string& secret_key,
        const std::string& method,
        const std::string& path
    );
};

} // namespace muxi
