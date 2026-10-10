// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Utils/Listeners.h
//
// xewe::ListenerSet<Iface, N>: a fixed-capacity set of listener pointers for module-to-module
// change notifications. Standard library only (host-includable), no heap, no locking.
//
//   struct FanListener { virtual void on_speed(uint8_t pct, const void* origin) = 0; };
//   xewe::ListenerSet<FanListener> listeners;                  // N = 4 by default
//   listeners.add(&web);                                        // false when full or null
//   listeners.notify([&](FanListener& l) { l.on_speed(pct, origin); });
//
// Origin rule: every event carries the `const void* origin` the caller passed to the setter
// (nullptr from the CLI). A listener that both listens and sets passes `this` as origin and skips
// events whose origin is itself, so a change it made is not echoed back to it:
//
//   void on_speed(uint8_t pct, const void* origin) override { if (origin == this) return; ... }
//
// notify() runs in the caller's task. remove() during notify() is safe (a removed listener that has
// not been called yet is skipped); add() during notify() may or may not see the current event.
// Full documentation: doc/os/module.md#listeners.
#pragma once

#include <cstddef>


namespace xewe {

template <typename Iface, std::size_t N = 4>
class ListenerSet {
    static_assert(N > 0, "ListenerSet needs a capacity of at least 1");

public:
    // true when added or already present (no duplicates); false for nullptr or when full
    bool add(Iface* listener) {
        if (listener == nullptr) return false;
        for (Iface* l : items) if (l == listener) return true;
        for (Iface*& l : items) {
            if (l == nullptr) { l = listener; return true; }
        }
        return false;
    }

    // true when it was registered; its slot is reused by the next add()
    bool remove(Iface* listener) {
        if (listener == nullptr) return false;
        for (Iface*& l : items) {
            if (l == listener) { l = nullptr; return true; }
        }
        return false;
    }

    bool contains(const Iface* listener) const {
        if (listener == nullptr) return false;
        for (Iface* l : items) if (l == listener) return true;
        return false;
    }

    std::size_t size() const {
        std::size_t n = 0;
        for (Iface* l : items) n += (l != nullptr);
        return n;
    }

    static constexpr std::size_t capacity() { return N; }

    // calls fn(Iface&) for every listener in slot order; each slot is re-read before its call,
    // so a listener removed by an earlier callback is not called
    template <typename F>
    void notify(F&& fn) const {
        for (std::size_t i = 0; i < N; ++i) {
            Iface* l = items[i];
            if (l != nullptr) fn(*l);
        }
    }

private:
    Iface* items[N] = {};
};

} // namespace xewe
