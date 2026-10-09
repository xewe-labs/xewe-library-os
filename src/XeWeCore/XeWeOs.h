// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/XeWeOs.h
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_mac.h>
#include <mbedtls/sha256.h>

#include "esp_log.h"
#include "Module.h"


namespace xewe {

struct OsConfig {
    std::string      project_name    = "xewe-device";
    std::string      version         = "0.0.0";
    std::string      build_timestamp = {};
    std::string      url             = "https://github.com/xewe-labs/xewe-os-core";
    SerialPortConfig serial          = {};
    bool             print_banner    = true;
};

class System : public Module {
public:
    explicit    System                  (Os& os);

    void        begin_routines_required ()                               override;
    void        begin_routines_init     ()                               override;
    void        reset                   (const bool verbose      = false,
                                         const bool do_restart   = true,
                                         const bool keep_enabled = true) override;
    std::string status                  (const bool verbose = false)     const override;

    std::string get_device_name         ();
    void        restart                 (uint16_t delay_ms = 1000);
};

// Owns the core services (serial, nvs, cli, system) and every registered module.
//
// Declare the Os object before any module in the sketch: modules register
// themselves from their constructors, and globals in one file are constructed
// top to bottom.
class Os {
    // declared first: `system` registers itself during construction
    std::vector<Module*>        modules;
    std::string                 pending_errors;   // registration errors raised before begin(), one per line
    bool                        begun = false;
    OsConfig                    config;

public:
    explicit                    Os              (OsConfig config = {});

                                Os              (const Os&) = delete;
    Os&                         operator=       (const Os&) = delete;

    void                        begin           ();
    void                        loop            ();

    bool                        register_module (Module& module);
    // printf-style; prints the message, or before begin() (static constructors, no Serial yet)
    // queues it for begin() to print. Truncated at 127 characters.
    void                        report_error    (const char* fmt, ...);
    Module*                     get_module      (std::string_view id) const;
    const std::vector<Module*>& get_modules     ()                    const;
    const OsConfig&             get_config      ()                    const;

    SerialPort                  serial;
    Nvs                         nvs;
    // The ESP32 core defines `cli` as a function-like macro: initialise this member with braces (cli{serial}), never with parentheses.
    Cli                         cli;
    System                      system;

private:
    void                        print_banner    ();
};

} // namespace xewe

using XeWeOs = xewe::Os;
