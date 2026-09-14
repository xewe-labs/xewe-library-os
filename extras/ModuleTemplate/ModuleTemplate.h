// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// <project>/src/ModuleTemplate/ModuleTemplate.h
//
// Starting point for a XeWe OS module. Copy this folder to src/<Name>/ in your firmware,
// rename ModuleTemplate -> <Name> in both files, and delete the hooks you don't need.
#pragma once

#include <XeWeOS.h>


// Settings passed from the sketch: ModuleTemplate module(os, {.interval_ms = 500});
struct ModuleTemplateConfig {
    uint32_t interval_ms = 1000;
};

class ModuleTemplate : public xewe::os::Module {
public:
    // Take other modules this one uses by reference and call add_requirement() for each;
    // they must be declared before this module in the sketch.
    explicit    ModuleTemplate          (xewe::os::ModuleController& controller,
                                         ModuleTemplateConfig        config = {});

    void        begin_routines_required ()                               override;   // every boot
    void        begin_routines_init     ()                               override;   // until init completes once
    void        begin_routines_regular  ()                               override;   // every boot after init
    void        begin_routines_common   ()                               override;   // every boot, last

    void        loop                    ()                               override;   // must not block

    void        reset                   (const bool verbose      = false,
                                         const bool do_restart   = true,
                                         const bool keep_enabled = true) override;
    std::string status                  (const bool verbose = false)     const override;

    // public API used by other modules
    void        do_something            ();

private:
    ModuleTemplateConfig config;
    uint32_t             last_run_ms = 0;
};
