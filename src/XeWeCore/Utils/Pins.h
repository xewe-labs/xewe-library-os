// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Utils/Pins.h
//
// GPIO registry: which module owns which pin, and which pins are strapping pins on this chip.
// Fixed table, no heap; nothing is linked in unless claim/release/owner_of is called.
// Host-includable (standard library only; <sdkconfig.h> when the ESP32 core provides it).
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#if defined(__has_include)
#if __has_include(<sdkconfig.h>)
#include <sdkconfig.h>
#endif
#endif


namespace xewe::pins {

// table size: GPIO 0-48 (ESP32-S3 has 49 pads; C3 22, C6 31, ESP32 40)
inline constexpr int kMaxGpio = 49;

// Strapping pins, from each chip's datasheet ("Strapping Pins" / "Boot Configurations"):
//   ESP32    GPIO0, 2, 5, 12 (MTDI), 15 (MTDO)
//   ESP32-C3 GPIO2, 8, 9
//   ESP32-C6 GPIO4 (MTMS), 5 (MTDI), 8, 9, 15
//   ESP32-S3 GPIO0, 3, 45, 46
namespace detail {
constexpr uint64_t mask(std::initializer_list<int> gpios) {
    uint64_t m = 0;
    for (int g : gpios) m |= uint64_t{1} << g;
    return m;
}
} // namespace detail
inline constexpr uint64_t kStrappingEsp32   = detail::mask({0, 2, 5, 12, 15});
inline constexpr uint64_t kStrappingEsp32C3 = detail::mask({2, 8, 9});
inline constexpr uint64_t kStrappingEsp32C6 = detail::mask({4, 5, 8, 9, 15});
inline constexpr uint64_t kStrappingEsp32S3 = detail::mask({0, 3, 45, 46});

#if defined(CONFIG_IDF_TARGET_ESP32C3)
inline constexpr uint64_t kStrapping = kStrappingEsp32C3;
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
inline constexpr uint64_t kStrapping = kStrappingEsp32C6;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
inline constexpr uint64_t kStrapping = kStrappingEsp32S3;
#elif defined(CONFIG_IDF_TARGET_ESP32)
inline constexpr uint64_t kStrapping = kStrappingEsp32;
#else
inline constexpr uint64_t kStrapping = 0;   // host, or a chip not listed above
#endif

constexpr bool is_strapping(int gpio) {
    return gpio >= 0 && gpio < 64 && ((kStrapping >> gpio) & 1u);
}

// Conflicts and warnings go here, called as error_handler(error_context, message); the Os
// constructor points it at Os::report_error (queued before begin()). Unset means silent.
// A plain function pointer, not std::function: nothing is linked into firmware that never claims.
inline void (*error_handler)(void* context, const char* message) = nullptr;
inline void* error_context = nullptr;

namespace detail {
inline const char* owners[kMaxGpio] = {};   // owner name per GPIO, nullptr = free
inline uint64_t    strapping        = kStrapping;   // what claim() warns about (host tests set a chip's mask)

inline void report(const char* fmt, int gpio, const char* a, const char* b = "") {
    if (!error_handler) return;
    char msg[112];
    std::snprintf(msg, sizeof(msg), fmt, gpio, a, b);
    error_handler(error_context, msg);
}
} // namespace detail

// The owner of `gpio`, or nullptr when free or out of range.
inline const char* owner_of(int gpio) {
    return (gpio >= 0 && gpio < kMaxGpio) ? detail::owners[gpio] : nullptr;
}

// Claims `gpio` for `owner` (a module id; the pointer is stored, so it must outlive the claim).
// true when the pin was free or already `owner`'s. false, with "! GPIO n already claimed by <x>",
// when another owner holds it, and for a pin outside 0-48. A strapping pin is claimed with a
// warning, never refused.
inline bool claim(int gpio, const char* owner) {
    if (!owner) owner = "?";
    if (gpio < 0 || gpio >= kMaxGpio) {
        detail::report("! GPIO %d out of range (0-48): not claimed by %s", gpio, owner);
        return false;
    }
    const char*& slot = detail::owners[gpio];
    if (slot) {
        if (std::strcmp(slot, owner) == 0) return true;
        detail::report("! GPIO %d already claimed by %s, refused for %s", gpio, slot, owner);
        return false;
    }
    slot = owner;
    if ((detail::strapping >> gpio) & 1u) detail::report("! GPIO %d is a strapping pin (%s): its level at reset selects the boot mode", gpio, owner);
    return true;
}

// Frees `gpio` if `owner` holds it; false (and nothing changes) otherwise.
inline bool release(int gpio, const char* owner) {
    const char* held = owner_of(gpio);
    if (!held || !owner || std::strcmp(held, owner) != 0) return false;
    detail::owners[gpio] = nullptr;
    return true;
}

} // namespace xewe::pins
