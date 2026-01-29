#include <gtest/gtest.h>
#include "muxi/auth.hpp"

TEST(AuthTest, GenerateHmacSignature) {
    auto [sig, ts] = muxi::Auth::generate_hmac_signature("GET", "/rpc/status", "key123", "secret456");
    EXPECT_FALSE(sig.empty());
    EXPECT_FALSE(ts.empty());
}

TEST(AuthTest, BuildAuthHeader) {
    std::string header = muxi::Auth::build_auth_header("key123", "sig456", "1234567890");
    EXPECT_NE(header.find("MUXI-HMAC-SHA256"), std::string::npos);
    EXPECT_NE(header.find("key123"), std::string::npos);
    EXPECT_NE(header.find("sig456"), std::string::npos);
}

TEST(AuthTest, SignatureStripsQueryParams) {
    auto [sig1, ts1] = muxi::Auth::generate_hmac_signature("GET", "/path", "key", "secret");
    auto [sig2, ts2] = muxi::Auth::generate_hmac_signature("GET", "/path?foo=bar", "key", "secret");
    EXPECT_FALSE(sig1.empty());
    EXPECT_FALSE(sig2.empty());
}
