// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/extras/host/test/test_cli.cpp

#include "test.h"

#include <XeWeCore/Cli.h>

#include <string>
#include <vector>


namespace {

struct Fixture {
    xewe::SerialPort         serial;
    xewe::Cli                xewe_cli{serial};
    std::vector<std::string> got;
    int                      calls = 0;

    Fixture() {
        Serial.take();
        xewe_cli.add_group("dev", "Device");
        xewe_cli.add_command("dev", {"name", "Set the name", "$dev name \"x\"", 1,
            [this](xewe::span<const std::string> args) {
                ++calls;
                got.assign(args.begin(), args.end());
            }});
    }
};

bool contains(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

} // namespace

TEST(cli_quoting) {
    Fixture f;
    f.xewe_cli.execute("$dev name \"Kitchen Lights\"");
    CHECK_EQ(f.calls, 1);
    CHECK(f.got.size() == 1 && f.got[0] == "Kitchen Lights");

    f.xewe_cli.execute("$dev name \"say \\\"hi\\\"\"");
    CHECK(f.got.size() == 1 && f.got[0] == "say \"hi\"");
}

TEST(cli_case_insensitive_group) {
    Fixture f;
    f.xewe_cli.execute("$DEV name x");
    CHECK_EQ(f.calls, 1);
    CHECK(f.xewe_cli.get_group("Dev") != nullptr);
}

TEST(cli_errors) {
    Fixture f;
    f.xewe_cli.execute("$dev name");
    CHECK_EQ(f.calls, 0);
    CHECK(contains(Serial.take(), "Argument count mismatch"));

    f.xewe_cli.execute("$nope run");
    CHECK(contains(Serial.take(), "Unknown command group"));

    f.xewe_cli.execute("$dev nope");
    CHECK(contains(Serial.take(), "Unknown command"));
}

TEST(cli_direct_execute) {
    Fixture f;
    const std::vector<std::string> args{"abc"};
    CHECK(f.xewe_cli.execute("dev", "name", args));
    CHECK_EQ(f.calls, 1);
    CHECK(!f.xewe_cli.execute("dev", "missing", args));
}

TEST(cli_help_lists_group) {
    Fixture f;
    f.xewe_cli.execute("$help");
    const std::string out = Serial.take();
    CHECK(contains(out, "Device"));
    CHECK(contains(out, "Set the name"));
}

TEST(cli_loop_reads_rx) {
    Fixture f;
    Serial.inject("$dev name fromrx\r\n");
    f.xewe_cli.loop();
    CHECK_EQ(f.calls, 1);
    CHECK(f.got.size() == 1 && f.got[0] == "fromrx");
}
