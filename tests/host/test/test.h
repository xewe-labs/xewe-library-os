// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/host/test/test.h
#pragma once

// Minimal self-registering test harness: no dependencies beyond the standard library.

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace host_test {

struct Case { const char* name; std::function<void()> fn; };

inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int&               failures() { static int f = 0; return f; }

struct Register { Register(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); } };

inline void fail(const char* file, int line, const std::string& what) {
    ++failures();
    std::fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, what.c_str());
}

} // namespace host_test

#define HT_CAT2(a, b) a##b
#define HT_CAT(a, b)  HT_CAT2(a, b)
#define TEST(name)                                                                 \
    static void HT_CAT(test_, name)();                                             \
    static host_test::Register HT_CAT(reg_, name)(#name, HT_CAT(test_, name));     \
    static void HT_CAT(test_, name)()

#define CHECK(cond) \
    do { if (!(cond)) host_test::fail(__FILE__, __LINE__, "CHECK(" #cond ")"); } while (0)

#define CHECK_EQ(a, b)                                                             \
    do {                                                                           \
        const auto& _a = (a); const auto& _b = (b);                                \
        if (!(_a == _b)) host_test::fail(__FILE__, __LINE__, "CHECK_EQ(" #a ", " #b ")"); \
    } while (0)
