// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/unit/test/test_cli_edges.cpp
//
// Cli edge cases beyond test_cli.cpp / test_tokenizer.cpp: escapes outside quotes,
// argument-count extremes, help forms, registration collisions, overloads,
// re-entrancy (commands that execute or (un)register while running) and buffer lifetime.

#include "test.h"

#include <XeWeCore/Cli.h>

#include <memory>
#include <string>
#include <vector>


namespace {

struct Fx {
    xewe::SerialPort         serial;
    xewe::Cli                cli{serial};
    std::vector<std::string> got;
    std::string              which;
    int                      calls = 0;

    Fx() { Serial.take(); }

    void add(const char* group, const char* name, std::size_t argc, const char* tag = "") {
        if (!cli.get_group(group)) cli.add_group(group, group);
        cli.add_command(group, {name, "", "", argc, [this, tag = std::string(tag)](xewe::span<const std::string> a) {
            ++calls;
            which = tag;
            got.assign(a.begin(), a.end());
        }});
    }
    std::string run(const std::string& line) { Serial.take(); cli.execute(line); return Serial.take(); }
};

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

} // namespace

TEST(cli_edge_backslash_outside_quotes_is_literal) {
    Fx f;
    f.add("t", "one", 1);
    f.add("t", "two", 2);
    f.run("$t one a\\b");
    CHECK(f.got.size() == 1 && f.got[0] == "a\\b");
    f.run("$t two a\\ b");                       // "\ " does not escape the space
    CHECK(f.got.size() == 2 && f.got[0] == "a\\" && f.got[1] == "b");
}

TEST(cli_edge_trailing_backslash_inside_quote_is_unterminated) {
    Fx f;
    f.add("t", "one", 1);
    const std::string out = f.run("$t one \"abc\\\"");   // the \" escapes the closing quote
    CHECK_EQ(f.calls, 0);
    CHECK(has(out, "Unterminated quote"));
    f.run("$t one \"abc\\\\\"");                         // escaped backslash, then a real close
    CHECK(f.calls == 1 && f.got[0] == "abc\\");
}

TEST(cli_edge_empty_quoted_group_and_command) {
    Fx f;
    f.add("t", "one", 1);
    CHECK(has(f.run("$\"\""), "Unknown command group ''"));
    CHECK(has(f.run("$t \"\""), "Missing command in command group 't'"));
    CHECK(has(f.run("$ \t "), "Missing command group"));
    CHECK_EQ(f.calls, 0);
}

TEST(cli_edge_many_args) {
    Fx f;
    f.add("t", "one", 1);
    f.add("t", "many", 300);
    std::string line = "$t one";
    for (int i = 0; i < 10000; ++i) line += " x";
    CHECK(has(f.run(line), "expected 1, got 10000"));
    CHECK_EQ(f.calls, 0);

    line = "$t many";
    for (int i = 0; i < 300; ++i) line += " \"" + std::to_string(i) + "\"";
    f.run(line);
    CHECK(f.calls == 1 && f.got.size() == 300 && f.got[299] == "299");
}

TEST(cli_edge_help_forms) {
    Fx f;
    f.add("t", "one", 1);
    CHECK(has(f.run("$help nosuch"), "Unknown command group 'nosuch'"));
    CHECK(has(f.run("$help t extra"), "Argument count mismatch for '$help'"));
    CHECK(has(f.run("$HELP T"), "Commands [t]"));
    CHECK(has(f.run("$help \"\""), "Commands [t]"));      // empty group -> full listing
    CHECK(has(f.run("$t HELP"), "Commands [t]"));
}

TEST(cli_edge_errors_have_no_blank_line) {
    // wave-1 finding 4: printf() already ends the line; Cli formats added a second CRLF
    Fx f;
    f.add("t", "one", 1);
    const char* lines[] = {"$nosuch", "$t nosuch", "$t one", "$t \"\""};
    for (const char* l : lines) {
        const std::string out = f.run(l);
        CHECK(!has(out, "\r\n\r\n"));
    }
}

TEST(cli_edge_group_id_help_is_unreachable) {
    // a group registered as "help" (or with inner spaces) can never be called from a line
    Fx f;
    f.add("help", "x", 0);
    f.add("my mod", "x", 0);
    f.run("$help x");
    f.run("$my mod x");
    CHECK_EQ(f.calls, 0);
    CHECK(f.cli.execute("my mod", "x", {}));             // only the programmatic path reaches it
}

TEST(cli_edge_group_id_collision_merges) {
    // two modules with the same id: add_group keeps the commands, the last name wins, and
    // the second module's same-named commands are shadowed by the first module's
    Fx f;
    f.cli.add_group(" Dup ", "First");
    f.cli.add_command("dup", {"status", "", "", 0, [&](xewe::span<const std::string>) { f.which = "first"; }});
    f.cli.add_group("DUP", "Second");
    f.cli.add_command("dup", {"status", "", "", 0, [&](xewe::span<const std::string>) { f.which = "second"; }});
    CHECK_EQ(f.cli.get_group("dup")->name, std::string("Second"));
    CHECK_EQ(f.cli.get_group("dup")->commands.size(), std::size_t(2));
    f.run("$dup status");
    CHECK_EQ(f.which, std::string("first"));
}

TEST(cli_edge_overload_by_arg_count) {
    // same name, different arg counts: the line path and the programmatic path agree
    Fx f;
    f.add("t", "set", 1, "set1");
    f.add("t", "set", 2, "set2");
    f.run("$t set a");
    CHECK_EQ(f.which, std::string("set1"));
    f.run("$t set a b");
    CHECK_EQ(f.which, std::string("set2"));
    const std::string args[] = {"a", "b"};
    CHECK(f.cli.execute("t", "set", xewe::span<const std::string>(args, 2)));
    CHECK_EQ(f.which, std::string("set2"));
    const std::string out = f.run("$t set a b c");      // no count matches: first one reported
    CHECK(has(out, "expected 1, got 3"));
}

TEST(cli_edge_rejects_invalid_registration) {
    Fx f;
    CHECK(!f.cli.add_command("nosuch", {"x", "", "", 0, [](xewe::span<const std::string>) {}}));
    f.cli.add_group("g", "G");
    CHECK(!f.cli.add_command("g", {"", "", "", 0, [](xewe::span<const std::string>) {}}));
    CHECK(!f.cli.add_command("g", {"x", "", "", 0, nullptr}));
    CHECK(has(f.run("$g"), "has no CLI commands"));
    CHECK(f.cli.remove_group(" G "));
    CHECK(!f.cli.remove_group("g"));
}

TEST(cli_edge_nested_execute) {
    // buttons / scheduler / web call execute() from inside a command
    Fx f;
    f.add("t", "leaf", 1);
    f.cli.add_group("outer", "Outer");
    f.cli.add_command("outer", {"run", "", "", 1, [&](xewe::span<const std::string> a) {
        const std::string line = "$t leaf " + a[0];
        f.cli.execute(line);
        f.cli.execute(line);
        CHECK_EQ(a[0], std::string("v"));               // own args intact after nested calls
    }});
    f.run("$outer run v");
    CHECK(f.calls == 2 && f.got[0] == "v");
}

TEST(cli_edge_command_removes_its_own_group) {
    // the running std::function must survive its group being erased (use-after-free on HEAD;
    // visible under -fsanitize=address)
    Fx f;
    auto state = std::make_shared<std::string>("alive");
    std::string seen;
    f.cli.add_group("self", "Self");
    f.cli.add_command("self", {"drop", "", "", 0, [&f, &seen, state](xewe::span<const std::string>) {
        f.cli.remove_group("self");
        seen = *state;                                  // captured state read after erase
    }});
    f.run("$self drop");
    CHECK_EQ(seen, std::string("alive"));
    CHECK(f.cli.get_group("self") == nullptr);

    f.cli.add_group("self", "Self");
    f.cli.add_command("self", {"drop", "", "", 0, [&f, &seen, state](xewe::span<const std::string>) {
        f.cli.remove_group("self");
        seen = *state + "2";
    }});
    CHECK(f.cli.execute("self", "drop", {}));
    CHECK_EQ(seen, std::string("alive2"));
}

TEST(cli_edge_command_registers_into_its_own_group) {
    // add_command during execution reallocates the vector holding the running command
    Fx f;
    auto state = std::make_shared<int>(7);
    int  seen  = 0;
    f.cli.add_group("grow", "Grow");
    f.cli.add_command("grow", {"go", "", "", 0, [&f, &seen, state](xewe::span<const std::string>) {
        for (int i = 0; i < 64; ++i) {
            f.cli.add_command("grow", {"n" + std::to_string(i), "", "", 0, [](xewe::span<const std::string>) {}});
        }
        seen = *state;
    }});
    f.run("$grow go");
    CHECK_EQ(seen, 7);
    CHECK_EQ(f.cli.get_group("grow")->commands.size(), std::size_t(65));
}

TEST(cli_edge_input_buffer_lifetime) {
    // execute(string_view) copies the line: a command that clobbers the caller's buffer
    // (e.g. a web handler reusing it) does not change the args it was given
    Fx f;
    std::string buffer = "$t peek abc";
    f.cli.add_group("t", "T");
    f.cli.add_command("t", {"peek", "", "", 1, [&](xewe::span<const std::string> a) {
        buffer.assign(200, 'z');
        f.got.assign(a.begin(), a.end());
    }});
    f.cli.execute(std::string_view(buffer));
    CHECK(f.got.size() == 1 && f.got[0] == "abc");
}

TEST(cli_edge_embedded_nul_in_programmatic_line) {
    // execute(string_view) from the web path can carry a NUL; it is an ordinary character
    Fx f;
    f.add("t", "one", 1);
    f.cli.execute(std::string_view("$t one a\0b", 10));
    CHECK(f.calls == 1 && f.got[0] == std::string("a\0b", 3));
}

TEST(cli_name_error_rules) {
    // shared by add_command (command names) and Os::register_module (module ids)
    using xewe::Cli;
    CHECK(Cli::name_error("status", false) == nullptr);
    CHECK(Cli::name_error("help", false) == nullptr);             // a command may be named help
    CHECK(Cli::name_error("", false) != nullptr);
    CHECK(Cli::name_error("a b", false) != nullptr);
    CHECK(Cli::name_error("ab\t", false) != nullptr);
    CHECK(Cli::name_error(" ab", false) != nullptr);
    CHECK(Cli::name_error("wifi", true) == nullptr);
    CHECK(Cli::name_error("abcdefghijklmno", true) == nullptr);   // 15 chars: NVS limit
    CHECK(has(Cli::name_error("abcdefghijklmnop", true), "15 characters"));
    CHECK(Cli::name_error("abcdefghijklmnop", false) == nullptr); // no limit on command names
    CHECK(has(Cli::name_error("help", true), "reserved"));
    CHECK(has(Cli::name_error("HELP", true), "reserved"));
    CHECK(has(Cli::name_error("my mod", true), "whitespace"));
    CHECK(has(Cli::name_error("", true), "empty"));
}

TEST(cli_edge_rejects_command_name_with_whitespace) {
    Fx f;
    f.cli.add_group("g", "G");
    CHECK(!f.cli.add_command("g", {"a b", "", "", 0, [](xewe::span<const std::string>) {}}));
    CHECK(!f.cli.add_command("g", {" a", "", "", 0, [](xewe::span<const std::string>) {}}));
    CHECK(f.cli.add_command("g", {"a", "", "", 0, [](xewe::span<const std::string>) {}}));
    CHECK_EQ(f.cli.get_group("g")->commands.size(), std::size_t(1));
}
