#include <gtest/gtest.h>
#include "muxi/errors.hpp"

TEST(ErrorsTest, Map401ToAuthenticationException) {
    try {
        throw muxi::map_error(401, "INVALID_KEY", "Invalid API key");
    } catch (const muxi::AuthenticationException& e) {
        EXPECT_EQ(e.status_code(), 401);
        SUCCEED();
        return;
    }
    FAIL() << "Expected AuthenticationException";
}

TEST(ErrorsTest, Map403ToAuthorizationException) {
    try {
        throw muxi::map_error(403, "FORBIDDEN", "Access denied");
    } catch (const muxi::AuthorizationException& e) {
        EXPECT_EQ(e.status_code(), 403);
        SUCCEED();
        return;
    }
    FAIL() << "Expected AuthorizationException";
}

TEST(ErrorsTest, Map404ToNotFoundException) {
    try {
        throw muxi::map_error(404, "NOT_FOUND", "Resource not found");
    } catch (const muxi::NotFoundException& e) {
        EXPECT_EQ(e.status_code(), 404);
        SUCCEED();
        return;
    }
    FAIL() << "Expected NotFoundException";
}

TEST(ErrorsTest, Map429ToRateLimitException) {
    try {
        throw muxi::map_error(429, "", "Rate limited", 60);
    } catch (const muxi::RateLimitException& e) {
        EXPECT_EQ(e.status_code(), 429);
        EXPECT_EQ(e.retry_after().value_or(0), 60);
        SUCCEED();
        return;
    }
    FAIL() << "Expected RateLimitException";
}

TEST(ErrorsTest, Map500ToServerException) {
    try {
        throw muxi::map_error(500, "INTERNAL", "Server error");
    } catch (const muxi::ServerException& e) {
        EXPECT_EQ(e.status_code(), 500);
        SUCCEED();
        return;
    }
    FAIL() << "Expected ServerException";
}
