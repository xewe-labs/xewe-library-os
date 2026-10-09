// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/host/probe/Cli.probe.cpp

// Compiles Cli against the shim and instantiates a command handler, which is where
// xewe::span appears in the public signature.
#include <XeWeCore/Cli.h>

#include <string>


void xewe_host_probe_cli() {
    xewe::SerialPort serial;
    xewe::Cli        xewe_cli(serial);   // never write cli( : it is a core macro on ESP32

    xewe_cli.add_group("demo", "Demo");
    xewe_cli.add_command("demo", {"run", "runs it", "$demo run", 0,
                                  [](xewe::span<const std::string> args) { (void)args.size(); }});
    xewe_cli.execute("$demo run");
    xewe_cli.loop();
}
