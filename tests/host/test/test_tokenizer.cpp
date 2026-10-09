// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/host/test/test_tokenizer.cpp
//
// Cli tokenizer cases; the same table runs on the board through `$test echo*` in
// tests/hardware/test_hooks_cli_tokenizer.py (keep the two in sync).

#include "test.h"

#include <XeWeCore/Cli.h>

#include <string>
#include <vector>


namespace {

struct EchoFixture {
    xewe::SerialPort         serial;
    xewe::Cli                xewe_cli{serial};
    std::vector<std::string> got;
    bool                     called = false;

    EchoFixture() {
        Serial.take();
        xewe_cli.add_group("test", "Test hooks");
        const std::pair<const char*, std::size_t> arities[] = {
            {"echo0", 0}, {"echo", 1}, {"echo2", 2}, {"echo3", 3}, {"echo5", 5}};
        for (const auto& [name, argc] : arities) {
            xewe_cli.add_command("test", {name, "", "", argc, [this](xewe::span<const std::string> args) {
                called = true;
                got.assign(args.begin(), args.end());
            }});
        }
    }
};

struct Case {
    const char*              line;
    bool                     dispatched;
    std::vector<std::string> args;
};

const Case kCases[] = {
    {"$test echo hello",                  true,  {"hello"}},
    {"$test echo \"hello world\"",        true,  {"hello world"}},
    {"$test echo \"\"",                   true,  {""}},
    {"$test echo2 \"\" \"\"",             true,  {"", ""}},
    {"$test echo \"a\\\"b\"",             true,  {"a\"b"}},
    {"$test echo \"a\\\\b\"",             true,  {"a\\b"}},
    {"$test echo \"tab\\there\"",         true,  {"tabthere"}},      // \t is not translated
    {"$test echo a\"b",                   true,  {"a\"b"}},          // quotes only open a token
    {"$test echo3 \"a\"b c",              true,  {"a", "b", "c"}},   // a closing quote ends the token
    {"$test echo \"h\xC3\xA9llo \xF0\x9F\x98\x80\"", true, {"h\xC3\xA9llo \xF0\x9F\x98\x80"}},
    {"   $test   echo   spaced   ",       true,  {"spaced"}},
    {"$TEST ECHO Case",                   true,  {"Case"}},
    {"$test\techo\tx",                    true,  {"x"}},
    {"$test echo0",                       true,  {}},
    {"$test echo5 1 \"2 2\" \"\" 4 5",    true,  {"1", "2 2", "", "4", "5"}},
    {"$test echo \"abc",                  false, {}},                // unterminated quote
    {"$test echo \"abc\\",                false, {}},                // unterminated after escape
    {"$test echo a b",                    false, {}},                // arity mismatch
    {"$test echo",                        false, {}},                // arity mismatch
    {"test echo x",                       false, {}},                // missing '$'
};

} // namespace

TEST(tokenizer_table) {
    for (const Case& c : kCases) {
        EchoFixture f;
        f.xewe_cli.execute(c.line);
        if (f.called != c.dispatched) {
            host_test::fail(__FILE__, __LINE__, std::string("dispatch mismatch: ") + c.line);
            continue;
        }
        if (c.dispatched && f.got != c.args) {
            host_test::fail(__FILE__, __LINE__, std::string("args mismatch: ") + c.line);
        }
    }
}

TEST(tokenizer_errors_are_reported) {
    EchoFixture f;
    f.xewe_cli.execute("$test echo \"abc");
    CHECK(Serial.take().find("Unterminated quote") != std::string::npos);
    f.xewe_cli.execute("$test echo a b");
    CHECK(Serial.take().find("expected 1, got 2") != std::string::npos);
}

TEST(tokenizer_embedded_nul_and_long_line_via_rx) {
    EchoFixture f;
    Serial.inject(std::string("$test echo a\0b\n", 15));
    f.xewe_cli.loop();
    CHECK(f.called);
    CHECK(f.got.size() == 1 && f.got[0] == std::string("a\0b", 3));
}
