// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/unit/test/test_pins.cpp
//
// xewe::pins: the GPIO registry and the per-chip strapping masks.

#include "test.h"

#include <XeWeCore/Utils/Pins.h>

#include <string>
#include <vector>


namespace {
struct PinsFx {
    std::vector<std::string> msgs;
    PinsFx() {
        xewe::pins::error_context = this;
        xewe::pins::error_handler = [](void* self, const char* m) { static_cast<PinsFx*>(self)->msgs.emplace_back(m); };
    }
    ~PinsFx() {
        xewe::pins::error_handler = nullptr;
        xewe::pins::error_context = nullptr;
        for (int g = 0; g < xewe::pins::kMaxGpio; ++g) xewe::pins::detail::owners[g] = nullptr;
    }
};
} // namespace

TEST(pins_claim_conflict_and_release) {
    PinsFx f;
    CHECK(xewe::pins::claim(10, "fan"));
    CHECK(xewe::pins::claim(10, "fan"));                       // same owner again: fine, silent
    CHECK(f.msgs.empty());
    CHECK(!xewe::pins::claim(10, "led"));
    CHECK(f.msgs.size() == 1);
    if (!f.msgs.empty()) CHECK_EQ(f.msgs[0], std::string("! GPIO 10 already claimed by fan, refused for led"));
    CHECK_EQ(std::string(xewe::pins::owner_of(10)), std::string("fan"));
    CHECK(!xewe::pins::release(10, "led"));                    // not the owner
    CHECK(xewe::pins::owner_of(10) != nullptr);
    CHECK(xewe::pins::release(10, "fan"));
    CHECK(xewe::pins::owner_of(10) == nullptr);
    CHECK(!xewe::pins::release(10, "fan"));                    // already free
    CHECK(xewe::pins::claim(10, "led"));
}

TEST(pins_out_of_range_refused) {
    PinsFx f;
    CHECK(!xewe::pins::claim(-1, "fan"));
    CHECK(!xewe::pins::claim(xewe::pins::kMaxGpio, "fan"));
    CHECK(xewe::pins::claim(48, "fan"));
    CHECK(f.msgs.size() == 2);
    if (!f.msgs.empty()) CHECK_EQ(f.msgs[0], std::string("! GPIO -1 out of range (0-48): not claimed by fan"));
    CHECK(xewe::pins::owner_of(-1) == nullptr && xewe::pins::owner_of(99) == nullptr);
    CHECK(!xewe::pins::release(-1, "fan"));
}

TEST(pins_silent_without_handler) {
    CHECK(xewe::pins::claim(20, "a"));
    CHECK(!xewe::pins::claim(20, "b"));
    CHECK(xewe::pins::release(20, "a"));
}

// Datasheet strapping-pin lists (Pins.h cites them); the host has no target, so nothing is strapping
TEST(pins_strapping_masks) {
    using namespace xewe::pins;
    const auto only = [](uint64_t m, std::initializer_list<int> want) {
        uint64_t w = 0;
        for (int g : want) w |= uint64_t{1} << g;
        return m == w;
    };
    CHECK(only(kStrappingEsp32C3, {2, 8, 9}));
    CHECK(only(kStrappingEsp32C6, {4, 5, 8, 9, 15}));
    CHECK(only(kStrappingEsp32S3, {0, 3, 45, 46}));
    CHECK(only(kStrappingEsp32, {0, 2, 5, 12, 15}));
    CHECK(kStrapping == 0);
    CHECK(!is_strapping(8) && !is_strapping(-1) && !is_strapping(64));
}

TEST(pins_strapping_claim_warns_not_refuses) {
    PinsFx f;
    xewe::pins::detail::strapping = xewe::pins::kStrappingEsp32C3;   // as on a C3
    CHECK(xewe::pins::claim(8, "led"));                                // claimed despite the warning
    CHECK_EQ(std::string(xewe::pins::owner_of(8)), std::string("led"));
    CHECK(f.msgs.size() == 1);
    if (!f.msgs.empty())
        CHECK_EQ(f.msgs[0], std::string("! GPIO 8 is a strapping pin (led): its level at reset selects the boot mode"));
    CHECK(xewe::pins::claim(8, "led") && f.msgs.size() == 1);          // re-claim by the owner: no second warning
    CHECK(xewe::pins::claim(7, "led") && f.msgs.size() == 1);          // not strapping on a C3
    xewe::pins::detail::strapping = xewe::pins::kStrapping;
}
