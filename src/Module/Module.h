// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-library-os/src/Module/Module.h
#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <XeWeUtils.h>
#include <XeWeSerial.h>
#include <XeWeNvs.h>
#include <XeWeCommandExecutor.h>


namespace xewe::os {

using xewe::Command;

class ModuleController;

// Base class for everything that plugs into XeWe OS.
//
// A module registers itself with the controller on construction, so the order
// modules are declared in the sketch is the order they begin in. Per-module
// settings are passed to the derived constructor and stored with their real type.
class Module {
public:
                             Module                    (ModuleController& controller,
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

    // begin logic; begin() is called by ModuleController::begin()
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
    std::string_view         get_description           ()                           const;

protected:
    ModuleController&        controller;
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

    void                     run_with_dots             (const std::function<void()>& work,
                                                        uint32_t duration_ms     = 1000,
                                                        uint32_t dot_interval_ms = 200);

private:
    std::vector<Module*>     required_modules;
    std::vector<Module*>     dependent_modules;

    void                     register_generic_commands ();
};

} // namespace xewe::os
