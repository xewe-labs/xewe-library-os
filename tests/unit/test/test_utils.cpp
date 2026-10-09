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

// Pins hsv_to_rgb bit for bit. xewe-os-modules led fx/Math.h hsv_spectrum is a copy and must stay
// identical until it is deleted in favour of Utils/Color.h. The checksum covers a 256 x 256 x 16 grid
// and assumes run.sh's flags (no -O, so no FMA contraction; -O2 -ffp-contract=fast changes it).
TEST(color_hsv_to_rgb_pinned) {
    using A = std::array<uint8_t, 3>;
    CHECK((xewe::color::hsv_to_rgb({85, 255, 255}) == A{0, 255, 0}));
    CHECK((xewe::color::hsv_to_rgb({170, 255, 255}) == A{0, 0, 255}));
    CHECK((xewe::color::hsv_to_rgb({43, 200, 180}) == A{178, 180, 38}));
    CHECK((xewe::color::hsv_to_rgb({128, 128, 128}) == A{63, 127, 128}));
    CHECK((xewe::color::hsv_to_rgb({255, 255, 255}) == A{255, 0, 0}));
    CHECK((xewe::color::hsv_to_rgb({200, 100, 50}) == A{44, 30, 50}));
    CHECK((xewe::color::hsv_to_rgb({254, 254, 254}) == A{254, 0, 6}));
    uint64_t h = 1469598103934665603ULL;   // FNV-1a over every output byte
    for (int hue = 0; hue < 256; ++hue)
        for (int sat = 0; sat < 256; ++sat)
            for (int val = 0; val < 256; val += 17) {
                const A rgb = xewe::color::hsv_to_rgb({uint8_t(hue), uint8_t(sat), uint8_t(val)});
                for (uint8_t c : rgb) h = (h ^ c) * 1099511628211ULL;
            }
    CHECK_EQ(h, uint64_t{0x2101a41e7915ca6eULL});
}

TEST(hex_color_parse) {
    uint8_t r = 1, g = 2, b = 3;
    CHECK(xewe::str::parse_hex_color("ff8000", r, g, b));
    CHECK(r == 0xFF && g == 0x80 && b == 0x00);
    CHECK(xewe::str::parse_hex_color("#12abEF", r, g, b));
    CHECK(r == 0x12 && g == 0xAB && b == 0xEF);
    CHECK(xewe::str::parse_hex_color("#000000", r, g, b));
    CHECK(r == 0 && g == 0 && b == 0);
}

TEST(hex_color_rejects_malformed) {
    const char* bad[] = {"", "#", "#ff", "ff", "ff00zz", "#ff00zz", "#ff00001", "ff00001", "fff",
                         "##00ffff", "0xff00", "-12345", "+12345", " 12345", "12345 ", "123456#",
                         "#12345G", "red", "ff 000"};
    for (const char* s : bad) {
        uint8_t r = 7, g = 8, b = 9;
        if (xewe::str::parse_hex_color(s, r, g, b)) host_test::fail(__FILE__, __LINE__, std::string("accepted ") + s);
        CHECK(r == 7 && g == 8 && b == 9);   // untouched on failure
    }
}

TEST(hex_color_format_round_trip) {
    CHECK_EQ(xewe::str::to_hex_color(0xFF, 0x80, 0x00), std::string("#FF8000"));
    CHECK_EQ(xewe::str::to_hex_color(0, 0, 0), std::string("#000000"));
    uint8_t r = 0, g = 0, b = 0;
    CHECK(xewe::str::parse_hex_color(xewe::str::to_hex_color(0x0A, 0xB0, 0xC3), r, g, b));
    CHECK(r == 0x0A && g == 0xB0 && b == 0xC3);
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
