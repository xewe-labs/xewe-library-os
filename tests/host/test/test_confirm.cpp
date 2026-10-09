// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/host/test/test_confirm.cpp
//
// The bounded confirmation used by Module::disable and System::reset:
// get_yn("OK?", 2, 15000, false, answered): two attempts of 15 s, worst stall 30 s. Module/System need the ESP32 core and NVS, so
// they are not built on the host; this drives the exact same get_yn call with the fake clock.

#include "test.h"

#include <XeWeCore/Serial.h>

#include <string>


namespace {

constexpr uint32_t kConfirmTimeoutMs = 15000;  // per attempt, Module.cpp / XeWeOs.cpp
constexpr uint16_t kConfirmAttempts  = 2;

// on every yield() the clock moves 10 ms; at `inject_at_ms` the scripted answer arrives
unsigned long t0           = 0;
unsigned long inject_at_ms = 0;
std::string   scripted;

void tick() {
    host::advance(10);
    if (!scripted.empty() && host::now_ms - t0 >= inject_at_ms) {
        Serial.inject(scripted);
        scripted.clear();
    }
}

struct ConfirmFixture {
    xewe::SerialPort serial;

    ConfirmFixture(std::string answer = {}, unsigned long at_ms = 0) {
        Serial.rx.clear();
        Serial.take();
        xewe::SerialPortConfig cfg;
        cfg.startup_delay_ms = 0;
        cfg.echo             = false;
        serial.begin(cfg);
        Serial.take();
        t0           = host::now_ms;
        inject_at_ms = at_ms;
        scripted     = std::move(answer);
        host::on_yield = tick;
    }
    ~ConfirmFixture() { host::on_yield = nullptr; scripted.clear(); }

    bool ask(bool& answered) { return serial.get_yn("OK?", kConfirmAttempts, kConfirmTimeoutMs, false, answered); }
};

std::size_t count(const std::string& hay, const std::string& needle) {
    std::size_t n = 0;
    for (std::size_t p = hay.find(needle); p != std::string::npos; p = hay.find(needle, p + 1)) ++n;
    return n;
}

} // namespace

TEST(confirm_times_out_after_30s_with_default_false) {
    ConfirmFixture f;  // nobody answers
    bool answered = true;
    const bool yes = f.ask(answered);
    const unsigned long elapsed = host::now_ms - t0;
    CHECK(!yes);
    CHECK(!answered);
    CHECK(elapsed >= 2 * kConfirmTimeoutMs);
    CHECK(elapsed < 2 * kConfirmTimeoutMs + 100);  // two attempts only, no third 15 s wait
    const std::string out = Serial.take();
    CHECK_EQ(count(out, "! Timeout."), std::size_t{2});
    CHECK_EQ(count(out, "(y/n) > "), std::size_t{2});
}

TEST(confirm_answer_in_second_attempt_after_timeout) {
    ConfirmFixture f("y\n", kConfirmTimeoutMs + 1000);
    bool answered = false;
    CHECK(f.ask(answered));
    CHECK(answered);
    const std::string out = Serial.take();
    CHECK_EQ(count(out, "! Timeout."), std::size_t{1});
    CHECK_EQ(count(out, "(y/n) > "), std::size_t{2});
}

TEST(confirm_yes_within_timeout) {
    ConfirmFixture f("y\n", 5000);
    bool answered = false;
    CHECK(f.ask(answered));
    CHECK(answered);
    CHECK(host::now_ms - t0 < kConfirmTimeoutMs);
    CHECK_EQ(count(Serial.take(), "Timeout"), std::size_t{0});
}

TEST(confirm_no_within_timeout) {
    ConfirmFixture f("n\n", 14000);
    bool answered = false;
    CHECK(!f.ask(answered));
    CHECK(answered);  // a real "no", not a fallback
}

TEST(confirm_invalid_answer_reprompts_once_then_answer_taken) {
    ConfirmFixture f("maybe\n", 1000);
    bool answered = false;
    const auto inject_second = [] {
        tick();
        // once the first (invalid) answer is consumed, script a real one 1 s later
        if (scripted.empty() && inject_at_ms < 2000) { scripted = "n\n"; inject_at_ms = 2000; }
    };
    host::on_yield = inject_second;
    CHECK(!f.ask(answered));
    CHECK(answered);  // a real "no" on the second attempt
    const std::string out = Serial.take();
    CHECK_EQ(count(out, "! Please answer 'y' or 'n'."), std::size_t{1});
    CHECK_EQ(count(out, "(y/n) > "), std::size_t{2});
}

TEST(confirm_invalid_answer_reprompts_once_second_invalid_cancels) {
    ConfirmFixture f("maybe\nfoo\n", 1000);  // second line is read by the re-prompt
    bool answered = true;
    CHECK(!f.ask(answered));
    CHECK(!answered);
    CHECK(host::now_ms - t0 < kConfirmTimeoutMs);  // cancelled on the second invalid answer, no wait
    const std::string out = Serial.take();
    CHECK_EQ(count(out, "! Please answer 'y' or 'n'."), std::size_t{2});
    CHECK_EQ(count(out, "(y/n) > "), std::size_t{2});
}

TEST(confirm_answer_after_both_timeouts_is_not_taken) {
    ConfirmFixture f("y\n", 2 * kConfirmTimeoutMs + 1000);
    bool answered = true;
    CHECK(!f.ask(answered));
    CHECK(!answered);
    f.serial.clear_input();
}
