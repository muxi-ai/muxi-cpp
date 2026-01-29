#include "muxi/auth.hpp"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace muxi {

static std::string base64_encode(const unsigned char* data, size_t len) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve((len + 2) / 3 * 4);
    
    for (size_t i = 0; i < len; i += 3) {
        unsigned int n = static_cast<unsigned int>(data[i]) << 16;
        if (i + 1 < len) n |= static_cast<unsigned int>(data[i + 1]) << 8;
        if (i + 2 < len) n |= static_cast<unsigned int>(data[i + 2]);
        
        result += chars[(n >> 18) & 0x3F];
        result += chars[(n >> 12) & 0x3F];
        result += (i + 1 < len) ? chars[(n >> 6) & 0x3F] : '=';
        result += (i + 2 < len) ? chars[n & 0x3F] : '=';
    }
    return result;
}

std::pair<std::string, std::string> Auth::generate_hmac_signature(
    const std::string& method,
    const std::string& path,
    const std::string& key_id,
    const std::string& secret_key
) {
    std::string clean_path = path;
    auto pos = clean_path.find('?');
    if (pos != std::string::npos) clean_path = clean_path.substr(0, pos);
    
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
    std::string timestamp = std::to_string(seconds);
    
    std::string upper_method = method;
    for (auto& c : upper_method) c = std::toupper(c);
    
    std::string message = upper_method + "\n" + clean_path + "\n" + timestamp;
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    
    HMAC(EVP_sha256(),
         secret_key.c_str(), static_cast<int>(secret_key.length()),
         reinterpret_cast<const unsigned char*>(message.c_str()), message.length(),
         hash, &hash_len);
    
    std::string signature = base64_encode(hash, hash_len);
    
    return {signature, timestamp};
}

std::string Auth::build_auth_header(
    const std::string& key_id,
    const std::string& signature,
    const std::string& timestamp
) {
    return "MUXI-HMAC-SHA256 Credential=" + key_id + ",Timestamp=" + timestamp + ",Signature=" + signature;
}

} // namespace muxi
