// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/unit/test/test_parsers.cpp
//
// Parser/validator cases; the same table runs on the board through `$test validate` in
// tests/board/test_hooks_utils.py (keep the two in sync). Covers 64-bit overflow (rejected, never
// saturated), "-1" into an unsigned type, and validate<T> refusing values that do not fit T.

#include "test.h"

#include <XeWeCore/Utils.h>

#include <cstdint>
#include <string>


TEST(parse_int_64bit_overflow_rejected) {
    long long v = 0;
    CHECK(!xewe::str::parse_int("9223372036854775808", v));    // overflow: rejected, not saturated
    CHECK(!xewe::str::parse_int("-9223372036854775809", v));
    CHECK(xewe::str::parse_int("-9223372036854775808", v));
    CHECK(v == INT64_MIN);
    uint64_t u = 0;
    CHECK(!xewe::str::parse_int("18446744073709551616", u));   // overflow: rejected, not saturated
    CHECK(xewe::str::parse_int("18446744073709551615", u));
    CHECK(u == UINT64_MAX);
}

TEST(parse_int_unsigned_rejects_minus) {
    uint64_t u = 7;
    CHECK(!xewe::str::parse_int("-1", u));                      // no wrap to UINT64_MAX
    CHECK(u == 7);
    uint8_t b = 0;
    CHECK(!xewe::str::parse_int("-0", b));
    CHECK(!xewe::str::parse_int(" -5 ", b));
    uint32_t w = 0;
    CHECK(xewe::str::parse_int("+5", w));
    CHECK(w == 5);
}

TEST(parse_int_table) {
    long long v = 0;
    CHECK(!xewe::str::parse_int("0x10", v));
    CHECK(!xewe::str::parse_int(" ", v));
    CHECK(!xewe::str::parse_int("1 2", v));
    CHECK(!xewe::str::parse_int("1e3", v));
    CHECK(xewe::str::parse_int("+5", v) && v == 5);
    CHECK(xewe::str::parse_int("\t-42\t", v) && v == -42);
}

TEST(validate_does_not_truncate_to_type) {
    CHECK(!xewe::validate<int8_t>("300", 0, 1000).has_value());   // does not fit int8_t
    CHECK(!xewe::validate<uint8_t>("256", 0, 1000).has_value());  // does not fit uint8_t
    CHECK(xewe::validate<int8_t>("-128", -1000, 1000).value_or(0) == -128);
    CHECK(xewe::validate<uint32_t>("4294967295", 0ULL, 4294967295ULL).value_or(0) == 4294967295u);
    CHECK(!xewe::validate<uint32_t>("-1", 0ULL, 4294967295ULL).has_value());
    CHECK(xewe::validate<int64_t>("-5", -10LL, 10LL).value_or(0) == -5);
}

TEST(parse_float_table) {
    double d = 0;
    CHECK(xewe::str::parse_float(" 1.5 ", d) && d == 1.5);
    CHECK(!xewe::str::parse_float("1e400", d));        // overflow
    CHECK(!xewe::str::parse_float("abc", d));
    CHECK(!xewe::str::parse_float("1.5x", d));
    // documented leniency (strtod): nan, inf and hex floats are accepted
    CHECK(xewe::str::parse_float("0x10", d) && d == 16.0);
    CHECK(!xewe::validate<float>("nan", 0.0, 1.0).has_value());   // NaN never passes a range
}

TEST(parse_gmt_offset_table) {
    struct G { const char* in; bool ok; const char* out; };
    const G cases[] = {
        {"GMT", true, "GMT+00:00"},  {"utc", true, "GMT+00:00"},    {"GMT+5", true, "GMT+05:00"},
        {"GMT+0530", true, "GMT+05:30"}, {"GMT-14", true, "GMT-14:00"}, {"GMT+5:30", true, "GMT+05:30"},
        {"GMT+14:01", false, ""},    {"GMT+5:60", false, ""},        {"GMT+", false, ""},
        {"GMT+123456", false, ""},   {"EST", false, ""},             {"GMT*5", false, ""},
        {"GMT+05:30", true, "GMT+05:30"}, {"gmt-8", true, "GMT-08:00"}, {"GMT+530", true, "GMT+05:30"},
        {"GMT+5x", false, ""},       {"GMT+5:30x", false, ""},       {"GMT+0530x", false, ""},
        {"GMT+5:30abc", false, ""},
        {"GMT+5:30 ", false, ""},    {"GMT++5", false, ""},          {"GMT+ 5", false, ""},
        {"GMT+5:3", false, ""},      {"GMT+:30", false, ""},         {"GMT+12345", false, ""},
    };
    for (const G& g : cases) {
        std::string out;
        const bool  ok = xewe::str::parse_gmt_offset(g.in, out);
        if (ok != g.ok || (ok && out != g.out)) host_test::fail(__FILE__, __LINE__, g.in);
    }
}

TEST(parse_day_and_time_table) {
    uint8_t day = 9;
    CHECK(xewe::str::parse_day("mo", day) && day == 0);
    CHECK(xewe::str::parse_day("SU", day) && day == 6);
    CHECK(!xewe::str::parse_day("MON", day));
    uint16_t m = 0;
    CHECK(xewe::str::parse_time("24:00", m) && m == 1440);
    CHECK(!xewe::str::parse_time("24:01", m));
    CHECK(xewe::str::parse_time("7:5", m) && m == 425);
    CHECK(!xewe::str::parse_time("7", m));
    CHECK(!xewe::str::parse_time("12:60", m));
}

TEST(extract_commands_and_escape) {
    const auto cmds = xewe::str::extract_commands(R"("$a b" "$c \"d\"")");
    CHECK(cmds.size() == 2 && cmds[0] == "$a b" && cmds[1] == "$c \"d\"");
    CHECK(xewe::str::extract_commands("no quotes").empty());
    CHECK_EQ(xewe::str::escape_json("a\"b\\c"), std::string("a\\\"b\\\\c"));
}
