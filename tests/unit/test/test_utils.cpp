// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/unit/test/test_utils.cpp

#include "test.h"

#include <XeWeCore/Utils.h>

#include <array>
#include <string>
#include <vector>


TEST(string_case) {
    CHECK_EQ(xewe::str::capitalize("kitchen lights"), std::string("Kitchen Lights"));
    CHECK_EQ(xewe::str::capitalize("hELLO-wORLD 2nd"), std::string("Hello-World 2nd"));
    CHECK_EQ(xewe::str::to_lower("MiXeD"), std::string("mixed"));
    CHECK_EQ(xewe::str::upper("abc"), std::string("ABC"));
}

TEST(string_format) {
    CHECK_EQ(xewe::str::format("%d-%s", 7, "x"), std::string("7-x"));
    const std::string big(300, 'a');
    CHECK_EQ(xewe::str::format("%s", big.c_str()), big);   // longer than any fixed buffer
}

TEST(string_parse_int) {
    int  i = 0;
    CHECK(xewe::str::parse_int("  42 ", i));
    CHECK_EQ(i, 42);
    CHECK(xewe::str::parse_int("-17", i));
    CHECK_EQ(i, -17);
    CHECK(!xewe::str::parse_int("12x", i));
    CHECK(!xewe::str::parse_int("", i));
    uint8_t u = 0;
    CHECK(!xewe::str::parse_int("256", u));                // out of range for uint8_t
    CHECK(xewe::str::parse_int("255", u));
    CHECK_EQ(u, uint8_t{255});
}

TEST(string_parse_float) {
    double d = 0.0;
    CHECK(xewe::str::parse_float("1.5", d));
    CHECK(d == 1.5);
    CHECK(!xewe::str::parse_float("1.5.2", d));
    CHECK(!xewe::str::parse_float("   ", d));
}

TEST(string_parse_time) {
    uint16_t m = 0;
    CHECK(xewe::str::parse_time("07:30", m));
    CHECK_EQ(m, uint16_t{450});
    CHECK(xewe::str::parse_time("24:00", m));
    CHECK_EQ(m, uint16_t{1440});
    CHECK(!xewe::str::parse_time("24:01", m));
    CHECK(!xewe::str::parse_time("7h", m));
}

TEST(string_wrap) {
    const auto w = xewe::str::wrap_words("wrap this sentence into short lines", 12);
    CHECK((w == std::vector<std::string>{"wrap this", "sentence", "into short", "lines"}));
    for (const auto& line : w) CHECK(line.size() <= 12);
    const auto f = xewe::str::wrap_fixed("abcdefg", 3);
    CHECK((f == std::vector<std::string>{"abc", "def", "g"}));
}

TEST(string_layout) {
    CHECK_EQ(xewe::str::align_into("ab", 6, 'r'), std::string("    ab"));
    CHECK_EQ(xewe::str::align_into("ab", 6, 'c'), std::string("  ab  "));
    CHECK_EQ(xewe::str::align_into("ab", 6, 'l'), std::string("ab    "));
    CHECK_EQ(xewe::str::repeat_pattern("-=", 5), std::string("-=-=-"));
    CHECK_EQ(xewe::str::make_rule_line(6), std::string("+----+"));
    CHECK_EQ(xewe::str::make_spacer_line(6), std::string("|    |"));
    CHECK_EQ(xewe::str::compose_box_line("hi", "|", 6, 1, 1, 'c'), std::string("|   hi   |"));
}

TEST(validator) {
    CHECK(xewe::validate<uint8_t>("128", 0, 255).value_or(0) == 128);
    CHECK(!xewe::validate<uint8_t>("300", 0, 255).has_value());
    CHECK(xewe::validate<int32_t>("-5", -10, 10).value_or(0) == -5);
    CHECK(!xewe::validate<int32_t>("-11", -10, 10).has_value());
    CHECK(xewe::validate<double>("1.5", 0.0, 2.0).has_value());
    CHECK(xewe::validate<std::string>("abc", 1, 4).value_or("") == "abc");
    CHECK(!xewe::validate<std::string>("abcde", 1, 4).has_value());
}

TEST(span) {
    std::vector<int>   v{1, 2, 3};
    xewe::span<int>    s(v);
    CHECK_EQ(s.size(), size_t{3});
    CHECK_EQ(s[1], 2);
    int sum = 0;
    for (int x : s) sum += x;
    CHECK_EQ(sum, 6);
    xewe::span<int>    empty;
    CHECK(empty.empty());
}

TEST(color) {
    CHECK((xewe::color::hsv_to_rgb({0, 255, 255}) == std::array<uint8_t, 3>{255, 0, 0}));
    CHECK((xewe::color::hsv_to_rgb({0, 0, 0}) == std::array<uint8_t, 3>{0, 0, 0}));
    CHECK((xewe::color::rgb_to_hsv({255, 0, 0}) == std::array<uint8_t, 3>{0, 255, 255}));
}

TEST(async_timer) {
    host::set_millis(1000);
    xewe::AsyncTimer<double> t(100, 0.0, 10.0);
    CHECK(!t.is_active());
    t.initiate();
    CHECK(t.is_active());
    CHECK(t.get_current_value() == 0.0);
    host::advance(50);
    CHECK(t.get_current_value() == 5.0);
    CHECK(t.is_not_done());
    host::advance(60);
    CHECK(t.is_done());
    CHECK(t.get_current_value() == 10.0);
    t.reset(10.0, 0.0);
    CHECK(!t.is_active());
}

TEST(lock_guard) {
    SemaphoreHandle_t m = xSemaphoreCreateMutex();
    {
        xewe::LockGuard g(m);
        CHECK_EQ(m->held, 1);
    }
    CHECK_EQ(m->held, 0);
    CHECK_EQ(m->takes, 1);
    CHECK_EQ(m->gives, 1);
    vSemaphoreDelete(m);
}
