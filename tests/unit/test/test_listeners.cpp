// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/unit/test/test_listeners.cpp
//
// xewe::ListenerSet (Utils/Listeners.h): capacity, no duplicates, slot reuse, notify order,
// remove during notify, and the origin-echo rule.

#include "test.h"

#include <XeWeCore/Utils/Listeners.h>

#include <vector>


namespace {

struct SpeedListener {
    virtual ~SpeedListener() = default;
    virtual void on_speed(int value, const void* origin) = 0;
};

std::vector<int> calls;   // listener tags in call order

struct Tagged : SpeedListener {
    int tag;
    int last = -1;
    explicit Tagged(int t) : tag(t) {}
    void on_speed(int value, const void* origin) override {
        if (origin == this) return;   // the origin rule: skip our own echo
        last = value;
        calls.push_back(tag);
    }
};

// a "module" that owns a set and a setter carrying the origin
struct Fan {
    xewe::ListenerSet<SpeedListener> listeners;
    int speed = 0;
    void set_speed(int v, const void* origin = nullptr) {
        speed = v;
        listeners.notify([&](SpeedListener& l) { l.on_speed(v, origin); });
    }
};

} // namespace

TEST(listeners_capacity_and_duplicates) {
    xewe::ListenerSet<SpeedListener, 2> set;
    Tagged a(1), b(2), c(3);
    CHECK_EQ(set.capacity(), std::size_t(2));
    CHECK_EQ(set.size(), std::size_t(0));
    CHECK(!set.add(nullptr));
    CHECK(set.add(&a));
    CHECK(set.add(&a));                       // already present: true, not added twice
    CHECK_EQ(set.size(), std::size_t(1));
    CHECK(set.add(&b));
    CHECK(!set.add(&c));                      // full
    CHECK_EQ(set.size(), std::size_t(2));
    CHECK(set.contains(&a));
    CHECK(!set.contains(&c));
    CHECK(set.remove(&a));
    CHECK(!set.remove(&a));                   // not registered any more
    CHECK(!set.remove(nullptr));
    CHECK(set.add(&c));                       // the freed slot is reused
    CHECK_EQ(set.size(), std::size_t(2));
    static_assert(xewe::ListenerSet<SpeedListener>::capacity() == 4, "default capacity is 4");
}

TEST(listeners_notify_order_and_origin_echo) {
    Fan    fan;
    Tagged a(1), b(2), c(3);
    fan.listeners.add(&a);
    fan.listeners.add(&b);
    fan.listeners.add(&c);
    calls.clear();
    fan.set_speed(40);                        // CLI: origin nullptr, everyone hears it
    CHECK_EQ(calls, (std::vector<int>{1, 2, 3}));
    calls.clear();
    fan.set_speed(55, &b);                    // b made the change: b skips its echo
    CHECK_EQ(calls, (std::vector<int>{1, 3}));
    CHECK_EQ(b.last, 40);
    CHECK_EQ(a.last, 55);
}

TEST(listeners_remove_during_notify_is_safe) {
    struct Remover : SpeedListener {
        Fan*           fan    = nullptr;
        SpeedListener* victim = nullptr;
        int            hits   = 0;
        void on_speed(int, const void*) override {
            ++hits;
            fan->listeners.remove(victim);
            fan->listeners.remove(this);      // removing itself mid-notify is fine too
        }
    };
    Fan     fan;
    Remover r;
    Tagged  later(7);
    r.fan    = &fan;
    r.victim = &later;
    fan.listeners.add(&r);
    fan.listeners.add(&later);
    calls.clear();
    fan.set_speed(10);
    CHECK_EQ(r.hits, 1);
    CHECK(calls.empty());                     // removed before its turn: not called
    CHECK_EQ(fan.listeners.size(), std::size_t(0));
    fan.set_speed(11);
    CHECK_EQ(r.hits, 1);
}
