// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/extras/host/probe/Utils.probe.cpp

// Forces every host-buildable Utils template to be instantiated. Including the
// header is not enough: a template body is only compiled when it is instantiated.
// LockGuard builds against the single-threaded FreeRTOS stand-in in shim/freertos/.
#define DEBUG_Probe 0

#include <XeWeCore/Utils/Debug.h>
#include <XeWeCore/Utils/String.h>
#include <XeWeCore/Utils/Validator.h>
#include <XeWeCore/Utils/AsyncTimer.h>
#include <XeWeCore/Utils/Span.h>
#include <XeWeCore/Utils/Color.h>
#include <XeWeCore/Utils/LockGuard.h>
#include <XeWeCore/Utils.h>

#include <cstdint>
#include <string>
#include <vector>


template class xewe::AsyncTimer<uint8_t>;
template class xewe::AsyncTimer<double>;

void xewe_host_probe_utils() {
    (void)xewe::validate<uint8_t>    ("128",  0,   255);
    (void)xewe::validate<int32_t>    ("-5",  -10,  10);
    (void)xewe::validate<double>     ("1.5",  0.0, 2.0);
    (void)xewe::validate<std::string>("ab",   1,   4);

    int         i = 0;
    double      d = 0.0;
    (void)xewe::str::parse_int  ("42",  i);
    (void)xewe::str::parse_float("4.2", d);
    (void)xewe::str::format("%d", 1);
    (void)xewe::str::to_lower("ABC");
    (void)xewe::str::repeat_pattern("-", 4);

    const std::vector<std::string> v{"a"};
    xewe::span<const std::string>  sp(v);                      // container ctor
    xewe::span<const std::string>  sp2(v.data(), v.size());    // pointer + size
    (void)sp.size();
    (void)sp[0];
    (void)sp2.empty();
    for (const auto& s : sp) (void)s;

    (void)xewe::color::hsv_to_rgb({0, 255, 255});

    DBG_PRINTLN(Probe, "x");
    DBG_PRINTF (Probe, "%d\n", 1);
}
