// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Module.h
#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Utils.h"
#include "Serial.h"
#include "Nvs.h"
#include "Cli.h"
#include "Settings.h"


namespace xewe {

class Os;

// Base class for everything that plugs into XeWe OS.
//
// A module registers itself with the Os object on construction, so the order
// modules are declared in the sketch is the order they begin in. Per-module
// settings are passed to the derived constructor and stored with their real type.
class Module {
public:
                             Module                    (Os&               os,
                                                        std::string       id,
                                                        std::string       name,
                                                        std::string       description,
                                                        bool              requires_init_setup,
                                                        bool              can_be_disabled,
                                                        bool              has_cli_commands);

    virtual                  ~Module                   ()                           noexcept = default;

                             Module                    (const Module&)              = delete;
    Module&                  operator=                 (const Module&)              = delete;
                             Module                    (Module&&)                   = delete;
    Module&                  operator=                 (Module&&)                   = delete;

    // begin logic; begin() is called by Os::begin()
    void                     begin                     ();
    virtual void             begin_routines_required   ();
    virtual void             begin_routines_init       ();
    virtual void             begin_routines_regular    ();
    virtual void             begin_routines_common     ();

    void                     add_requirement           (Module& other);

    // loop and flow logic
    virtual void             loop                      ();

    virtual void             enable                    (const bool verbose    = false,
                                                        const bool do_restart = true);
    virtual void             disable                   (const bool verbose    = false,
                                                        const bool do_restart = true);
    virtual void             reset                     (const bool verbose      = false,
                                                        const bool do_restart   = true,
                                                        const bool keep_enabled = true);

    // info
    virtual std::string      status                    (const bool verbose = false) const;
    bool                     is_enabled                (const bool verbose = false) const;
    bool                     is_disabled               (const bool verbose = false) const;
    bool                     init_setup_complete       (const bool verbose = false) const;
    bool                     has_cli_cmds              ()                           const;

    // getters
    std::string_view         get_id                    ()                           const;
    std::string_view         get_name                  ()                           const;

    // settings table (doc/os/settings.md). Empty by default: no commands, no NVS reads,
    // no schema rows. Override to return {table, this}; read at begin() before every routine.
    virtual Settings         settings                  ()                           const;
    // extra schema rows after the table's (e.g. mode parameters with "group":"mode:<name>")
    virtual void             schema_extra              (SchemaOut& out)             const;
    // table rows then schema_extra; what `$<id> schema` and `$system schema` print per module
    void                     print_schema              (SchemaOut& out)             const;
    // `$<id> set` without the CLI: validate, apply, persist, on_setting_changed. verbose prints
    // the result or the error line. false on an unknown key or a bad value.
    bool                     apply_setting             (std::string_view key,
                                                        std::string_view value,
                                                        const bool       verbose = false);

protected:
    Os&                      os;
    std::string              id;
    std::string              name;
    std::string              description;

    bool                     requires_init_setup;
    bool                     can_be_disabled;
    bool                     has_cli_commands;
    bool                     enabled;

    // adds a command to this module's `$<id>` group; requires has_cli_commands
    bool                     register_command          (Command command);

    bool                     requirements_enabled      (const bool verbose = false) const;

    // called after a table setting was applied and saved (`$<id> set`, apply_setting), also while
    // the module is disabled; not called by the load at begin()
    virtual void             on_setting_changed        (const SettingDef& def);

    void                     run_with_dots             (const std::function<void()>& work,
                                                        uint32_t duration_ms     = 1000,
                                                        uint32_t dot_interval_ms = 200);

private:
    friend void              settings_attach           (Module& module);

    std::vector<Module*>     required_modules;
    std::vector<Module*>     dependent_modules;

    void                     register_generic_commands ();
    void                     register_settings_commands();
    void                     settings_command          (std::size_t                   which,
                                                        xewe::span<const std::string> args);
    void                     load_settings             ();
};

} // namespace xewe
