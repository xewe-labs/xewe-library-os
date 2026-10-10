// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/unit/test/test_serial_output.cpp
//
// Exact console bytes of the SerialPort printers and prompt messages: print with edges, alignment,
// wrapping and margins, printf, printf_fmt, separators, spacers, headers, tables, and the error
// lines of every prompt type. Prompt input arrives one line at a time from yield().

#include "test.h"

#include <XeWeCore/Serial.h>

#include <deque>
#include <string>
#include <vector>


namespace {

std::deque<std::string> script;
int                     ticks_since_feed = 0;

// queue the lines of `all` (each ending in '\n'); the fake clock hands them out on yield()
void feed(const std::string& all) {
    std::size_t a = 0;
    for (std::size_t b; (b = all.find('\n', a)) != std::string::npos; a = b + 1) script.push_back(all.substr(a, b - a + 1));
}

void tick() {
    host::advance(10);
    if (++ticks_since_feed >= 5 && Serial.rx.empty() && !script.empty()) {
        Serial.inject(script.front());
        script.pop_front();
        ticks_since_feed = 0;
    }
}

struct Case {
    const char* name;
    void        (*run)(xewe::SerialPort& s);
};

std::vector<Case>& cases() {
    static std::vector<Case> c;
    return c;
}

struct AddCase {
    AddCase(const char* name, void (*run)(xewe::SerialPort&)) { cases().push_back({name, run}); }
};

#define CASE(n)                             \
    void   n(xewe::SerialPort& s);          \
    AddCase add_##n(#n, n);                 \
    void   n(xewe::SerialPort& s)


CASE(print_lines) { s.print("a\nb\r\nc"); }
CASE(print_box_words) { s.print("hello world foo bar", xewe::str::kCRLF, "|", 'c', 'w', 10, 1, 1); }
CASE(print_box_chars) { s.print("abcdefghijkl", "", "#", 'r', 'c', 5, 0, 2); }
CASE(print_empty) { s.print(); s.print("", "END"); }
CASE(printf_basic) { s.printf("x=%d %s", 5, "y"); s.printf(nullptr); s.printf("100%%"); }
CASE(printf_fmt_box) { s.printf_fmt(xewe::str::kCRLF, "|", 'c', 'w', 20, 1, 1, "n=%u and more words here", 7u); s.printf_fmt("", "", 'l', 'w', 0, 0, 0, nullptr); }
CASE(separators) { s.print_separator(); s.print_separator(10, "=-", ""); s.print_separator(1, "-", "++"); s.print_separator(3, "-", "++"); s.print_separator(0); s.print_separator(6, "", "|"); }
CASE(spacers) { s.print_spacer(); s.print_spacer(8, "|"); s.print_spacer(5, ""); s.print_spacer(2, "||"); s.print_spacer(0, "|"); }
CASE(header) { s.print_header("Title\\sepSub line that is long enough to wrap around the box width\nsecond"); s.print_header("x", 0); s.print_header("narrow", 6, "", "*", "="); }
CASE(table) {
    std::vector<std::vector<std::string_view>> t = {{"Name", "Value", "Notes"}, {"a", "1"}, {"multi\nline", "2", "a long cell that has to wrap because it is wider than the column"}, {"", "", ""}};
    s.print_table(t, "My Table");
    s.print_table(t);
    s.print_table(t, "Narrow\nheader", 8, "#", "*", "~=");
    s.print_table({}, "none");
}
CASE(render_equals_print) { std::vector<std::vector<std::string_view>> t = {{"k", "v"}, {"x", "y"}}; std::string r = s.render_table(t, "H"); Serial.take(); s.print_table(t, "H"); if (Serial.tx != r) Serial.tx = "MISMATCH"; }
CASE(get_int_range) { feed("999\nabc\n5\n"); int v = s.get_int("Pick", 1, 10, 0, 0, 0); s.printf("v=%d", v); }
CASE(get_uint8_bad) { feed("300\n-1\n7\n"); unsigned v = s.get_uint8("U8", 0, 9, 3, 0, 4); s.printf("v=%u", v); }
CASE(get_uint16_32) { feed("70000\n65535\n"); unsigned v = s.get_uint16("U16"); feed("5000000000\n4000000000\n7\n"); unsigned long w = s.get_uint32("U32", 10, 5); s.printf("v=%u w=%lu", v, w); }
CASE(get_uint32_swap_timeout) { bool ok = true; unsigned long w = s.get_uint32("T", 9, 3, 2, 50, 8, ok); s.printf("w=%lu ok=%d", w, int(ok)); }
CASE(get_string_len) { feed("ab\nabcdefghijk\nabcd\n"); std::string v = s.get_string("Str", 3, 6); s.printf("v=%s", v.c_str()); }
CASE(get_float_range) { feed("x\nnan\n12.5\n2.5 \n"); float v = s.get_float("F", 0.0f, 10.0f); s.printf("v=%g", double(v)); }
CASE(get_yn_flow) { feed("maybe\nYES\n"); bool v = s.get_yn("Q"); s.printf("v=%d", int(v)); }
CASE(menu) { feed("0\n3\n2\n"); unsigned v = s.get_menu_choice("Menu", {"one", "two"}); s.printf("v=%u", v); feed("4\n"); v = s.get_menu_choice("Bare", {}, 1, 5); s.printf("v=%u", v); }

#undef CASE

struct Golden {
    const char* name;
    const char* out;
};

const Golden kGolden[] = {
    {"print_lines", "a\r\n"
        "b\r\n"
        "c\r\n"
        ""},
    {"print_box_words", "|   hello    |\r\n"
        "| world foo  |\r\n"
        "|    bar     |\r\n"
        ""},
    {"print_box_chars", "#abcde  #\r\n"
        "#fghij  #\r\n"
        "#   kl  #"},
    {"print_empty", "\r\n"
        "END"},
    {"printf_basic", "x=5 y\r\n"
        "\r\n"
        "100%\r\n"
        ""},
    {"printf_fmt_box", "|  n=7 and more words  |\r\n"
        "|         here         |\r\n"
        ""},
    {"separators", "+------------------------------------------------+\r\n"
        "=-=-=-=-=-\r\n"
        "+\r\n"
        "++\r\n"
        "\r\n"
        "|    |\r\n"
        ""},
    {"spacers", "                                                  \r\n"
        "|      |\r\n"
        "     \r\n"
        "||\r\n"
        "\r\n"
        ""},
    {"header", "+------------------------------------------------+\r\n"
        "|                     Title                      |\r\n"
        "+------------------------------------------------+\r\n"
        "|  Sub line that is long enough to wrap around   |\r\n"
        "|                 the box width                  |\r\n"
        "|                     second                     |\r\n"
        "+------------------------------------------------+\r\n"
        "\r\n"
        "| x |\r\n"
        "\r\n"
        "*====*\r\n"
        " narrow \r\n"
        "*====*\r\n"
        ""},
    {"table", "+----------------------------------------------+\r\n"
        "|                   My Table                   |\r\n"
        "+-------+-------+------------------------------+\r\n"
        "| Name  | Value | Notes                        |\r\n"
        "+-------+-------+------------------------------+\r\n"
        "| a     | 1     |                              |\r\n"
        "+-------+-------+------------------------------+\r\n"
        "| multi | 2     | a long cell that has to wrap |\r\n"
        "| line  |       | because it is wider than the |\r\n"
        "|       |       | column                       |\r\n"
        "+-------+-------+------------------------------+\r\n"
        "|       |       |                              |\r\n"
        "+-------+-------+------------------------------+\r\n"
        "+-------+-------+------------------------------+\r\n"
        "| Name  | Value | Notes                        |\r\n"
        "+-------+-------+------------------------------+\r\n"
        "| a     | 1     |                              |\r\n"
        "+-------+-------+------------------------------+\r\n"
        "| multi | 2     | a long cell that has to wrap |\r\n"
        "| line  |       | because it is wider than the |\r\n"
        "|       |       | column                       |\r\n"
        "+-------+-------+------------------------------+\r\n"
        "|       |       |                              |\r\n"
        "+-------+-------+------------------------------+\r\n"
        "*~=~=~=~=~=~=~=~=~=~=~=~=*\r\n"
        "#         Narrow         #\r\n"
        "#         header         #\r\n"
        "*~=~=~=~*~=~=~=~*~=~=~=~=*\r\n"
        "# Name  # Value # Notes  #\r\n"
        "*~=~=~=~*~=~=~=~*~=~=~=~=*\r\n"
        "# a     # 1     #        #\r\n"
        "*~=~=~=~*~=~=~=~*~=~=~=~=*\r\n"
        "# multi # 2     # a long #\r\n"
        "# line  #       # cell   #\r\n"
        "#       #       # that   #\r\n"
        "#       #       # has to #\r\n"
        "#       #       # wrap   #\r\n"
        "#       #       # becaus #\r\n"
        "#       #       # e it   #\r\n"
        "#       #       # is     #\r\n"
        "#       #       # wider  #\r\n"
        "#       #       # than   #\r\n"
        "#       #       # the    #\r\n"
        "#       #       # column #\r\n"
        "*~=~=~=~*~=~=~=~*~=~=~=~=*\r\n"
        "#       #       #        #\r\n"
        "*~=~=~=~*~=~=~=~*~=~=~=~=*\r\n"
        ""},
    {"render_equals_print", "+-------+\r\n"
        "|   H   |\r\n"
        "+---+---+\r\n"
        "| k | v |\r\n"
        "+---+---+\r\n"
        "| x | y |\r\n"
        "+---+---+\r\n"
        ""},
    {"get_int_range", "Pick\r\n"
        "> \r\n"
        "! Out of range [1..10].\r\n"
        "> \r\n"
        "! Invalid number. Please enter a base-10 integer.\r\n"
        "> \r\n"
        "v=5\r\n"
        ""},
    {"get_uint8_bad", "U8\r\n"
        "> \r\n"
        "! Invalid number. Please enter a base-10 integer.\r\n"
        "> \r\n"
        "! Invalid number. Please enter a base-10 integer.\r\n"
        "> \r\n"
        "v=7\r\n"
        ""},
    {"get_uint16_32", "U16\r\n"
        "> \r\n"
        "! Invalid number. Please enter a base-10 integer.\r\n"
        "> \r\n"
        "U32\r\n"
        "> \r\n"
        "! Invalid number. Please enter a base-10 integer.\r\n"
        "> \r\n"
        "! Out of range [5..10].\r\n"
        "> \r\n"
        "v=65535 w=7\r\n"
        ""},
    {"get_uint32_swap_timeout", "T\r\n"
        "> \r\n"
        "! Timeout.\r\n"
        "> \r\n"
        "! Timeout.\r\n"
        "w=8 ok=0\r\n"
        ""},
    {"get_string_len", "Str\r\n"
        "> \r\n"
        "! Length must be in [3..6] chars.\r\n"
        "> \r\n"
        "! Length must be in [3..6] chars.\r\n"
        "> \r\n"
        "v=abcd\r\n"
        ""},
    {"get_float_range", "F\r\n"
        "> \r\n"
        "! Invalid number. Please enter a decimal value.\r\n"
        "> \r\n"
        "! Invalid number.\r\n"
        "> \r\n"
        "! Out of range [0..10].\r\n"
        "> \r\n"
        "v=2.5\r\n"
        ""},
    {"get_yn_flow", "Q\r\n"
        "(y/n) > \r\n"
        "! Please answer 'y' or 'n'.\r\n"
        "(y/n) > \r\n"
        "v=1\r\n"
        ""},
    {"menu", "Menu\r\n"
        "  1) one\r\n"
        "  2) two\r\n"
        "Choice\r\n"
        "> \r\n"
        "! Out of range [1..2].\r\n"
        "> \r\n"
        "! Out of range [1..2].\r\n"
        "> \r\n"
        "v=2\r\n"
        "Bare\r\n"
        "> \r\n"
        "v=4\r\n"
        ""},
};

} // namespace

TEST(serial_output_golden) {
    CHECK_EQ(cases().size(), sizeof(kGolden) / sizeof(kGolden[0]));
    host::on_yield = tick;
    for (std::size_t i = 0; i < cases().size() && i < sizeof(kGolden) / sizeof(kGolden[0]); ++i) {
        xewe::SerialPort       s;
        xewe::SerialPortConfig cfg;
        cfg.startup_delay_ms = 0;
        cfg.echo             = false;
        s.begin(cfg);
        Serial.rx.clear();
        Serial.take();
        script.clear();
        cases()[i].run(s);
        const std::string out = Serial.take();
        if (out != kGolden[i].out) host_test::fail(__FILE__, __LINE__, std::string("output of ") + cases()[i].name + ":\n" + out);
    }
    host::on_yield = nullptr;
    script.clear();
}
