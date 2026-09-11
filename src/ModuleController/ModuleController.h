// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-library-os/src/ModuleController/ModuleController.h
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "../Module/Module.h"
#include "../System/System.h"


namespace xewe::os {

struct ModuleControllerConfig {
    std::string                   project_name    = "xewe-os";
    std::string                   version         = "0.0.0";
    std::string                   build_timestamp = {};
    std::string                   url             = "https://github.com/xewe-labs/xewe-library-os";
    xewe::SerialPortConfig        serial          = {};
    bool                          print_banner    = true;
};

// Owns the core services (serial, nvs, cmd_cli, system) and every registered module.
//
// Declare the controller before any module in the sketch: modules register
// themselves from their constructors, and globals in one file are constructed
// top to bottom.
class ModuleController {
    // declared first: `system` registers itself during construction
    std::vector<Module*>          modules;
    ModuleControllerConfig        config;

public:
    explicit                      ModuleController (ModuleControllerConfig config = {});

                                  ModuleController (const ModuleController&) = delete;
    ModuleController&             operator=        (const ModuleController&) = delete;

    void                          begin            ();
    void                          loop             ();

    bool                          register_module  (Module& module);
    Module*                       get_module       (std::string_view id)     const;
    const std::vector<Module*>&   get_modules      ()                        const;
    const ModuleControllerConfig& get_config       ()                        const;

    xewe::SerialPort              serial;
    xewe::Nvs                     nvs;
    xewe::CmdCli                  cmd_cli;
    System                        system;

private:
    void                          print_banner     ();
};

} // namespace xewe::os
