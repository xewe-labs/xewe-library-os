// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/unit/test/test_main.cpp

#include "test.h"

int main() {
    int run = 0;
    for (const auto& c : host_test::registry()) {
        const int before = host_test::failures();
        c.fn();
        ++run;
        std::printf("%s %s\n", host_test::failures() == before ? "ok  " : "FAIL", c.name);
    }
    std::printf("%d tests, %d failed checks\n", run, host_test::failures());
    return host_test::failures() == 0 ? 0 : 1;
}
