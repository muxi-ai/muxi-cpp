#include <gtest/gtest.h>
#include "muxi/formation_client.hpp"
#include "muxi/errors.hpp"

TEST(SseParserTest, FlushesEventOnlyDoneFrame) {
    muxi::detail::SseEventParser parser;
    EXPECT_FALSE(parser.process_line(": keepalive").has_value());
    EXPECT_FALSE(parser.process_line("").has_value());
    EXPECT_FALSE(parser.process_line("event: done").has_value());

    auto event = parser.process_line("");
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->event, "done");
    EXPECT_EQ(event->data, "");
}

TEST(SseParserTest, PreservesMultilineData) {
    muxi::detail::SseEventParser parser;
    parser.process_line("event: planning");
    parser.process_line("data: one");
    parser.process_line("data: two");

    auto event = parser.process_line("");
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->event, "planning");
    EXPECT_EQ(event->data, "one\ntwo");
}

TEST(SseParserTest, ParseUiWidgetsDecodesUiFrame) {
    muxi::SseEvent event{
        "ui",
        "{\"ui\":[{\"type\":\"options\",\"id\":\"w1\",\"prompt\":\"Which?\","
        "\"options\":[{\"value\":\"us\",\"label\":\"United States\"}]},"
        "{\"type\":\"action_link\",\"id\":\"w2\",\"label\":\"Dash\",\"url\":\"https://x.io\"}]}"};

    auto widgets = muxi::parse_ui_widgets(event);

    ASSERT_EQ(widgets.size(), 2u);
    EXPECT_EQ(widgets[0]["type"], "options");
    EXPECT_EQ(widgets[0]["options"][0]["label"], "United States");
    EXPECT_EQ(widgets[1]["url"], "https://x.io");
}

TEST(SseParserTest, ParseUiWidgetsIgnoresOtherFrames) {
    EXPECT_TRUE(muxi::parse_ui_widgets({"message", "hi"}).empty());
    EXPECT_TRUE(muxi::parse_ui_widgets({"ui", "not json"}).empty());
    EXPECT_TRUE(muxi::parse_ui_widgets({"ui", "{\"ui\":{}}"}).empty());
}

TEST(SseParserTest, UnwrapEnvelopeSurfacesIdempotencyKey) {
    auto env = muxi::json::parse(
        "{\"object\":\"api_response\",\"timestamp\":123,"
        "\"request\":{\"id\":\"req-1\",\"idempotency_key\":\"idem-42\"},"
        "\"data\":{\"foo\":\"bar\"},\"success\":true}");

    auto out = muxi::detail::unwrap_envelope(env);

    EXPECT_EQ(out["foo"], "bar");
    EXPECT_EQ(out["request_id"], "req-1");
    EXPECT_EQ(out["idempotency_key"], "idem-42");
}

TEST(SseParserTest, UnwrapEnvelopeOmitsIdempotencyKeyWhenAbsent) {
    auto env = muxi::json::parse(
        "{\"object\":\"api_response\",\"request\":{\"id\":\"req-1\"},"
        "\"data\":{\"foo\":\"bar\"},\"success\":true}");

    auto out = muxi::detail::unwrap_envelope(env);

    EXPECT_FALSE(out.contains("idempotency_key"));
}

TEST(SseParserTest, RouteLevelErrorsThrow) {
    muxi::detail::SseEventParser parser;
    parser.process_line("event: error");
    parser.process_line("data: {\"error\":\"boom\",\"type\":\"RUNTIME_ERROR\"}");

    try {
        (void)parser.process_line("");
        FAIL() << "Expected muxi::MuxiException";
    } catch (const muxi::MuxiException& ex) {
        EXPECT_EQ(ex.error_code(), "RUNTIME_ERROR");
        EXPECT_EQ(ex.status_code(), 0);
    }
}
