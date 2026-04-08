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
