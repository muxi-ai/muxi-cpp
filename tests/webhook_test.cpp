#include <gtest/gtest.h>
#include "muxi/webhook.hpp"

TEST(WebhookTest, VerifySignatureMissingSecret) {
    EXPECT_THROW(muxi::Webhook::verify_signature("payload", "t=123,v1=abc", ""), std::invalid_argument);
}

TEST(WebhookTest, VerifySignatureEmptyHeader) {
    EXPECT_FALSE(muxi::Webhook::verify_signature("payload", "", "secret"));
}

TEST(WebhookTest, VerifySignatureInvalidSignature) {
    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    std::string header = "t=" + std::to_string(ts) + ",v1=invalidsignature";
    EXPECT_FALSE(muxi::Webhook::verify_signature("payload", header, "secret"));
}

TEST(WebhookTest, ParseCompletedPayload) {
    std::string payload = R"({"status":"completed","content":[{"type":"text","text":"Hello"}]})";
    auto event = muxi::Webhook::parse(payload);
    EXPECT_EQ(event.status, "completed");
    EXPECT_EQ(event.content.size(), 1);
}

TEST(WebhookTest, ParseFailedPayload) {
    std::string payload = R"({"status":"failed","error":{"code":"ERROR","message":"Something went wrong"}})";
    auto event = muxi::Webhook::parse(payload);
    EXPECT_EQ(event.status, "failed");
    EXPECT_TRUE(event.error.has_value());
    EXPECT_EQ(event.error->code, "ERROR");
}

TEST(WebhookTest, ParseClarificationPayload) {
    std::string payload = R"({"status":"awaiting_clarification","clarification":{"question":"Which one?","options":["A","B"]}})";
    auto event = muxi::Webhook::parse(payload);
    EXPECT_EQ(event.status, "awaiting_clarification");
    EXPECT_TRUE(event.clarification.has_value());
    EXPECT_EQ(event.clarification->question, "Which one?");
}
