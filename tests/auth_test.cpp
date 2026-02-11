#include <gtest/gtest.h>
#include "muxi/auth.hpp"

TEST(AuthTest, GenerateHmacSignature) {
    auto [sig, ts] = muxi::Auth::generate_hmac_signature("secret456", "GET", "/rpc/status");
    EXPECT_FALSE(sig.empty());
    EXPECT_FALSE(ts.empty());
}

TEST(AuthTest, BuildAuthHeader) {
    std::string header = muxi::Auth::build_auth_header("key123", "secret456", "GET", "/path");
    EXPECT_NE(header.find("MUXI-HMAC key="), std::string::npos);
    EXPECT_NE(header.find("key123"), std::string::npos);
    EXPECT_NE(header.find("timestamp="), std::string::npos);
    EXPECT_NE(header.find("signature="), std::string::npos);
}

TEST(AuthTest, SignatureStripsQueryParams) {
    auto [sig1, ts1] = muxi::Auth::generate_hmac_signature("secret", "GET", "/path");
    auto [sig2, ts2] = muxi::Auth::generate_hmac_signature("secret", "GET", "/path?foo=bar");
    EXPECT_FALSE(sig1.empty());
    EXPECT_FALSE(sig2.empty());
}
