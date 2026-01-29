#include <gtest/gtest.h>
#include "muxi/errors.hpp"

TEST(ErrorsTest, Map401ToAuthenticationException) {
    auto ex = muxi::map_error(401, "INVALID_KEY", "Invalid API key");
    EXPECT_EQ(ex.status_code(), 401);
    EXPECT_EQ(ex.error_code(), "INVALID_KEY");
}

TEST(ErrorsTest, Map403ToAuthorizationException) {
    auto ex = muxi::map_error(403, "FORBIDDEN", "Access denied");
    EXPECT_EQ(ex.status_code(), 403);
    EXPECT_EQ(ex.error_code(), "FORBIDDEN");
}

TEST(ErrorsTest, Map404ToNotFoundException) {
    auto ex = muxi::map_error(404, "NOT_FOUND", "Resource not found");
    EXPECT_EQ(ex.status_code(), 404);
    EXPECT_EQ(ex.error_code(), "NOT_FOUND");
}

TEST(ErrorsTest, Map429ToRateLimitException) {
    auto ex = muxi::map_error(429, "", "Rate limited", 60);
    EXPECT_EQ(ex.status_code(), 429);
    EXPECT_EQ(ex.retry_after().value_or(0), 60);
}

TEST(ErrorsTest, Map500ToServerException) {
    auto ex = muxi::map_error(500, "INTERNAL", "Server error");
    EXPECT_EQ(ex.status_code(), 500);
    EXPECT_EQ(ex.error_code(), "INTERNAL");
}
