// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Module.cpp

#include "Module.h"
#include "XeWeOs.h"


namespace xewe {

Module::Module(Os&               os,
               std::string       id,
               std::string       name,
               std::string       description,
               bool              requires_init_setup,
               bool              can_be_disabled,
               bool              has_cli_commands)
    : os(os)
    , id(std::move(id))
    , name(std::move(name))
    , description(std::move(description))
    , requires_init_setup(requires_init_setup)
    , can_be_disabled(can_be_disabled)
    , has_cli_commands(has_cli_commands)
    , enabled(true) {
    if (!os.register_module(*this)) {
        // rejected (reported by Os): never begins or loops, and adds nothing to another module's group
        this->has_cli_commands = false;
        return;
    }
    if (has_cli_commands) {
        os.cli.add_group(this->id, this->name);
        register_generic_commands();
    }
}

void Module::begin() {
    bool first_boot = !os.nvs.read<bool>(id, "not_first_boot");
    enabled         = first_boot || os.nvs.read<bool>(id, "is_enabled");

    if (can_be_disabled || requires_init_setup) {
        os.serial.print_header(name + " Setup");
    }

    if (!requirements_enabled(true)) {
        enabled = false;
        os.nvs.write<bool>(id, "is_enabled", false);
        os.nvs.write<bool>(id, "not_first_boot", true);
        return;
    } else {
        // if the module was disabled due to inactive requirements, re-enable
        if (!can_be_disabled) {
            enabled = true;
            os.nvs.write<bool>(id, "is_enabled", true);
        }
    }

    if (is_disabled(true)) return;

    if (first_boot) {
        if (can_be_disabled) {
            os.serial.print_header(std::string("Would you like to enable ") + name + " module?\n\n" + description);
            enabled = os.serial.get_yn();

            if (!enabled) {
                os.nvs.write<bool>(id, "is_enabled", false);
                os.nvs.write<bool>(id, "not_first_boot", true);
                return;
            }
        }
        os.nvs.write<bool>(id, "is_enabled", true);
        os.nvs.write<bool>(id, "not_first_boot", true);
    }

    begin_routines_required();

    if (!init_setup_complete()) {
        begin_routines_init();
        if (enabled) { // could have been disabled during begin_routines_init()
            os.nvs.write<bool>(id, "init_complete", true);
        }
    } else {
        begin_routines_regular();
    }

    begin_routines_common();
}

void Module::begin_routines_required() {}

void Module::begin_routines_init() {}

void Module::begin_routines_regular() {}

void Module::begin_routines_common() {}

void Module::add_requirement(Module& other) {
    if (&other == this) return;
    required_modules.push_back(&other);
    other.dependent_modules.push_back(this);
}

void Module::loop() {}

void Module::enable(const bool verbose,
                    const bool do_restart) {
    if (is_enabled()) {
        if (verbose) os.serial.printf("%s module already enabled", name.c_str());
        return;
    }

    if (!requirements_enabled(true)) return;

    enabled = true;
    os.nvs.write<bool>(id, "is_enabled", true);

    if (verbose) os.serial.printf("%s module enabled", name.c_str());

    if (do_restart) os.system.restart();
}

void Module::disable(const bool verbose,
                     const bool do_restart) {
    if (is_disabled()) {
        if (verbose) os.serial.printf("%s module already disabled", name.c_str());
        return;
    }
    if (!can_be_disabled) {
        if (verbose) os.serial.printf("%s module can't be disabled", name.c_str());
        return;
    }

    bool disable_confirmed = true;

    if (verbose) {
        std::string msg = "[WARNING]\nDisabling " + name + "\nWill reset it";
        if (!dependent_modules.empty()) {
            msg += ", and all dependents: \\sep";
            for (auto* m : dependent_modules) {
                msg += m->name + "\n";
            }
            if (!msg.empty() && msg.back() == '\n')
                msg.pop_back();
        }
        os.serial.print_header(msg);
        // bounded: a disable issued by the scheduler, a button or the web UI must not freeze an
        // unattended device. Two attempts of 15 s (a typo re-prompts once); anything but a clear
        // "yes" cancels. Worst-case stall: 30 s.
        bool answered     = false;
        disable_confirmed = os.serial.get_yn("OK?", 2, 15000, false, answered);
        if (!answered) os.serial.print("! No answer: disable cancelled");
    }

    if (!disable_confirmed) {
        os.serial.print("Aborted");
        return;
    }

    if (!dependent_modules.empty()) {
        for (auto* m : dependent_modules) {
            if (verbose) os.serial.printf("%s module reset and disabled", m->name.c_str());
            m->disable(false, false); // cascade disable with no verbose, and dont reboot
        }
    }
    if (verbose) {
        os.serial.printf("%s module disabled", name.c_str());
    }

    reset(verbose, do_restart, false);
    return;
}

void Module::reset(const bool verbose,
                   const bool do_restart,
                   const bool keep_enabled) {
    os.nvs.reset_ns(id);
    os.nvs.write<bool>(id, "not_first_boot", true);

    enabled = (!can_be_disabled || keep_enabled) && requirements_enabled();

    if (enabled) { // re-enable the module
        os.nvs.write<bool>(id, "is_enabled", true);
    }

    if (verbose) os.serial.printf("%s module reset", name.c_str());

    if (do_restart) os.system.restart();
}

std::string Module::status(bool verbose) const {
    std::string status_str = (name + " module " + (os.nvs.read<bool>(id, "is_enabled") ? "enabled" : "disabled"));
    if (verbose) os.serial.print(status_str);
    return status_str;
}

// only print the debug msg if true
bool Module::is_enabled(bool verbose) const {
    if (verbose && enabled) os.serial.printf("%s module enabled", name.c_str());
    return enabled;
}

// only print the debug msg if true
bool Module::is_disabled(bool verbose) const {
    if (verbose && !enabled) {
        // case 1: requirements are not enabled
        if (!requirements_enabled()) {
            os.serial.printf("%s module disabled", name.c_str());
            requirements_enabled(true); // this will print list of requirements
            // case 2: disabled by user
        } else {
            os.serial.printf("%s module disabled; to enable:\n$%s enable", name.c_str(), id.c_str());
        }
    }
    return !enabled;
}

bool Module::init_setup_complete(bool verbose) const {
    return !requires_init_setup || os.nvs.read<bool>(id, "init_complete");
}

bool Module::has_cli_cmds() const { return has_cli_commands; }

std::string_view Module::get_id() const { return id; }

std::string_view Module::get_name() const { return name; }

std::string_view Module::get_description() const { return description; }

bool Module::register_command(Command command) {
    if (!has_cli_commands) return false;
    const char* why = Cli::name_error(command.name, false);
    if (!why && !command.function) why = "has no function";
    if (why) {
        os.report_error("! $%s command '%s' %s: not registered", id.c_str(), command.name.c_str(), why);
        return false;
    }
    return os.cli.add_command(id, std::move(command));
}

bool Module::requirements_enabled(bool verbose) const {
    bool all_enabled    = true;
    bool printed_header = false;

    for (auto* r : required_modules) {
        if (r->is_disabled()) {
            if (!verbose) return false;

            all_enabled = false;

            if (!printed_header) {
                printed_header = true;
                os.serial.printf("%s module requires:", name.c_str());
            }
            os.serial.printf("%s, use: $%s enable", r->name.c_str(), r->id.c_str());
        }
    }
    return all_enabled;
}

void Module::register_generic_commands() {
    register_command(Command{
        "status",
        "Get module status",
        std::string("$") + id + " status",
        0,
        [this](xewe::span<const std::string>) {
            status(true);
        }
    });

    register_command(Command{
        "reset",
        "Reset the module",
        std::string("$") + id + " reset",
        0,
        [this](xewe::span<const std::string>) {
            reset(true, true);
        }
    });

    if (can_be_disabled) {
        register_command(Command{
            "enable",
            "Enable this module",
            std::string("$") + id + " enable",
            0,
            [this](xewe::span<const std::string>) {
                enable(true, true);
            }
        });

        register_command(Command{
            "disable",
            "Disable this module",
            std::string("$") + id + " disable",
            0,
            [this](xewe::span<const std::string>) {
                disable(true, true);
            }
        });
    }
}

void Module::run_with_dots(const std::function<void()>& work,
                           uint32_t duration_ms,
                           uint32_t dot_interval_ms) {
    if (dot_interval_ms == 0) dot_interval_ms = 1;

    const uint32_t start = millis();
    uint32_t       next  = start; // first dot at t=0

    while ((uint32_t)(millis() - start) < duration_ms) {
        work(); // run the target function

        const uint32_t now = millis();
        if ((int32_t)(now - next) >= 0) {
            os.serial.print(std::string_view{"."}, "");

            // If we're late by multiple intervals, skip ahead (prevents dot bursts)
            const uint32_t late      = now - next;
            const uint32_t intervals = 1u + (late / dot_interval_ms);
            next += intervals * dot_interval_ms;
        }
    }
    os.serial.print();
}

} // namespace xewe
