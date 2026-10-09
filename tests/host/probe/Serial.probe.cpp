// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/host/probe/Serial.probe.cpp

// Compiles SerialPort against the shim. The shim has no setTxBufferSize /
// setRxBufferSize, so this fails unless the ESP32-only guard in begin() is correct.
#include <XeWeCore/Serial.h>

#include <string>


void xewe_host_probe_serial() {
    xewe::SerialPort serial;
    serial.begin();
    serial.loop();
    serial.print("x");
    serial.printf("%d", 1);
    serial.print_separator();
    (void)serial.has_line();
}
