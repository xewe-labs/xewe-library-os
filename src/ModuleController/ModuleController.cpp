// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-library-os/src/ModuleController/ModuleController.cpp

#include "ModuleController.h"


namespace xewe::os {

ModuleController::ModuleController(ModuleControllerConfig config)
    : config(std::move(config))
    , xewe_cli(serial)
    , system(*this)
{}

void ModuleController::begin() {
    serial.begin(config.serial);
    nvs.set_error_handler([this](std::string_view message) { serial.print(message); });

    if (config.print_banner) print_banner();

    const bool init_setup_flag = !nvs.read<bool>("root", "init_setup_flag");

    for (Module* module : modules) {
        module->begin();
    }

    if (init_setup_flag) {
        serial.print_header("Initial Setup Complete");
        nvs.write<bool>("root", "init_setup_flag", true);
        system.restart();
    }

    serial.print_header("System Setup Complete");
}

void ModuleController::loop() {
    xewe_cli.loop();

    for (Module* module : modules) {
        if (module->is_enabled()) {
            module->loop();
        }
    }
}

bool ModuleController::register_module(Module& module) {
    if (get_module(module.get_id()) != nullptr) return false;

    modules.push_back(&module);
    return true;
}

Module* ModuleController::get_module(std::string_view id) const {
    for (Module* module : modules) {
        if (module->get_id() == id) return module;
    }
    return nullptr;
}

const std::vector<Module*>& ModuleController::get_modules() const { return modules; }

const ModuleControllerConfig& ModuleController::get_config() const { return config; }

void ModuleController::print_banner() {
    serial.print(
        "+------------------------------------------------+\n"
        "|   $$\\   $$\\           $$\\      $$\\             |\n"
        "|   $$ |  $$ |          $$ | $\\  $$ |            |\n"
        "|   \\$$\\ $$  | $$$$$$\\  $$ |$$$\\ $$ | $$$$$$\\    |\n"
        "|    \\$$$$  / $$  __$$\\ $$ $$ $$\\$$ |$$  __$$\\   |\n"
        "|    $$  $$<  $$$$$$$$ |$$$$  _$$$$ |$$$$$$$$ |  |\n"
        "|   $$  /\\$$\\ $$   ____|$$$  / \\$$$ |$$   ____|  |\n"
        "|   $$ /  $$ |\\$$$$$$$\\ $$  /   \\$$ |\\$$$$$$$\\   |\n"
        "|   \\__|  \\__| \\_______|\\__/     \\__| \\_______|  |\n"
        "+------------------------------------------------+",
        xewe::str::kCRLF,
        "",
        'l',
        'c',
        50,
        0,
        0
    );
}

} // namespace xewe::os
