// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/extras/host/test/test_serial_input.cpp

#include "test.h"

#include <XeWeCore/Serial.h>

#include <string>


namespace {

struct InputFixture {
    xewe::SerialPort serial;

    InputFixture() {
        Serial.rx.clear();  // tests share the global shim
        Serial.take();
        xewe::SerialPortConfig cfg;
        cfg.startup_delay_ms = 0;
        cfg.echo             = false;  // keep Serial.tx to what the input path prints itself
        serial.begin(cfg);
        Serial.take();
    }
};

bool contains(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

std::size_t count(const std::string& hay, const std::string& needle) {
    std::size_t n = 0;
    for (std::size_t p = hay.find(needle); p != std::string::npos; p = hay.find(needle, p + 1)) ++n;
    return n;
}

} // namespace

TEST(serial_input_two_lines_in_order) {
    InputFixture f;
    Serial.inject("first\nsecond\n");
    f.serial.loop();
    CHECK(f.serial.has_line());
    CHECK_EQ(f.serial.read_line(), std::string("first"));
    CHECK(f.serial.has_line());
    CHECK_EQ(f.serial.read_line(), std::string("second"));
    CHECK(!f.serial.has_line());
}

TEST(serial_input_complete_plus_partial) {
    InputFixture f;
    Serial.inject("whole\npart");
    f.serial.loop();
    CHECK_EQ(f.serial.read_line(), std::string("whole"));
    CHECK(!f.serial.has_line());
    Serial.inject("ial\n");
    f.serial.loop();
    CHECK_EQ(f.serial.read_line(), std::string("partial"));
    CHECK(!f.serial.has_line());
}

TEST(serial_input_queue_overflow_drops_newest) {
    InputFixture f;
    Serial.inject("l1\nl2\nl3\nl4\nl5\n");
    f.serial.loop();
    const std::string out = Serial.take();
    CHECK(contains(out, "! Input overflow: line dropped"));
    CHECK_EQ(count(out, "Input overflow"), std::size_t{1});
    CHECK_EQ(f.serial.read_line(), std::string("l1"));
    CHECK_EQ(f.serial.read_line(), std::string("l2"));
    CHECK_EQ(f.serial.read_line(), std::string("l3"));
    CHECK_EQ(f.serial.read_line(), std::string("l4"));
    CHECK(!f.serial.has_line());
}

TEST(serial_input_long_line_split) {
    InputFixture f;
    std::string line;
    for (int i = 0; i < 300; ++i) line.push_back(static_cast<char>('a' + i % 26));
    Serial.inject(line + "\n");
    f.serial.loop();
    const std::string a = f.serial.read_line();
    const std::string b = f.serial.read_line();
    CHECK_EQ(a.size(), std::size_t{254});
    CHECK_EQ(a, line.substr(0, 254));
    CHECK_EQ(b, line.substr(254));  // the 255th character starts the next line
    CHECK(!f.serial.has_line());
    CHECK(!contains(Serial.take(), "overflow"));
}

TEST(serial_input_clear_input) {
    InputFixture f;
    Serial.inject("a\nb\npartial");
    f.serial.loop();
    Serial.inject("pending");
    f.serial.clear_input();
    CHECK(!f.serial.has_line());
    CHECK_EQ(Serial.available(), 0);
    Serial.inject("fresh\n");
    f.serial.loop();
    CHECK_EQ(f.serial.read_line(), std::string("fresh"));
    CHECK(!f.serial.has_line());
}

TEST(serial_input_crlf) {
    InputFixture f;
    Serial.inject("one\r\ntwo\r\n");
    f.serial.loop();
    CHECK_EQ(f.serial.read_line(), std::string("one"));
    CHECK_EQ(f.serial.read_line(), std::string("two"));
    CHECK(!f.serial.has_line());
}
