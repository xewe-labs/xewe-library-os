// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// <project>/src/ModuleTemplate/ModuleTemplate.cpp

#include "ModuleTemplate.h"


ModuleTemplate::ModuleTemplate(xewe::os::ModuleController& controller,
                               ModuleTemplateConfig        config)
    : Module(controller,
          /* id                  */ "template",          // CLI group ($template) and NVS namespace; <= 15 chars
          /* name                */ "Module Template",
          /* description         */ "What this module does, shown when asking to enable it",
          /* requires_init_setup */ false,
          /* can_be_disabled     */ true,
          /* has_cli_commands    */ true)
    , config(config) {
    register_command({
        "do",
        "Run the module's action once",
        "$template do",
        0,
        [this](std::span<const std::string>) { do_something(); }
    });
}

void ModuleTemplate::begin_routines_required() {}

void ModuleTemplate::begin_routines_init() {
    // one-time setup, e.g. prompt with controller.serial.get_*() and store with controller.nvs.write()
}

void ModuleTemplate::begin_routines_regular() {
    // load settings saved by begin_routines_init(), e.g. controller.nvs.read<uint32_t>(id, "key", default)
}

void ModuleTemplate::begin_routines_common() {}

void ModuleTemplate::loop() {
    if (millis() - last_run_ms < config.interval_ms) return;
    last_run_ms = millis();
}

void ModuleTemplate::reset(const bool verbose,
                           const bool do_restart,
                           const bool keep_enabled) {
    // clear module state held in RAM here; Module::reset wipes this module's NVS namespace
    Module::reset(verbose, do_restart, keep_enabled);
}

std::string ModuleTemplate::status(const bool verbose) const {
    std::string s = Module::status(false);
    if (verbose) controller.serial.print(s);
    return s;
}

void ModuleTemplate::do_something() {
    if (is_disabled(true)) return;          // other modules may call in while this one is disabled
    controller.serial.print("ModuleTemplate: did something");
}
