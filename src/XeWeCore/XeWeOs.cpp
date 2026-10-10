// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/XeWeOs.cpp

#include "XeWeOs.h"

#include <cstdarg>
#include <cstdio>
#include "Testing.h"

// XEWE_DEVICE_NAME may come from the tools' generated header, like XEWE_TESTING
#if !defined(XEWE_DEVICE_NAME) && defined(__has_include)
#if __has_include(<XeWeBuildInfo.h>)
#include <XeWeBuildInfo.h>
#endif
#endif


namespace xewe {

// ---- Os ------------------------------------------------------------------

Os::Os(OsConfig config)
    : config(std::move(config))
    , cli{serial}
    , system(*this)
{
    // modules may claim pins from their constructors: report_error queues until begin()
    pins::error_context = this;
    pins::error_handler = [](void* os, const char* message) { static_cast<Os*>(os)->report_error("%s", message); };
}

void Os::begin() {
    serial.begin(config.serial);
    nvs.set_error_handler([this](std::string_view message) { serial.print(message); });
    flex_error_handler = [this](std::string_view message) { serial.print(message); };

    if (config.print_banner) print_banner();

    begun = true;
    if (!pending_errors.empty()) serial.print(pending_errors);
    pending_errors = {};

#ifdef XEWE_TESTING
    testing::register_commands(*this);
#endif

    const bool init_setup_flag = !nvs.read<bool>("root", "init_setup_flag");

    for (Module* module : modules) {
        module->begin();
    }

    if (init_setup_flag) {
        serial.print_header("Initial Setup Complete");
        // restart only if the flag stuck: with broken NVS the flag would read false on every boot
        // and the device would boot-loop; stay up instead so it is reachable over the CLI
        if (nvs.write<bool>("root", "init_setup_flag", true)) system.restart();
        else serial.print("! NVS write failed: init_setup_flag not saved, not restarting");
    }

    serial.print_header("System Setup Complete");
}

void Os::loop() {
    cli.loop();

    for (Module* module : modules) {
        if (module->is_enabled()) {
            module->loop();
        }
    }
}

bool Os::register_module(Module& module) {
    const std::string_view id  = module.get_id();
    const char*            why = Cli::name_error(id, true);
    // Cli group ids are case-insensitive: "Foo" would merge into the group of "foo"
    if (!why && (get_module(id) || (module.has_cli_cmds() && cli.get_group(id)))) why = "is already registered";
    if (why) {
        report_error("! Module id '%.*s' %s: module not registered", int(id.size()), id.data(), why);
        return false;
    }

    modules.push_back(&module);
    return true;
}

void Os::report_error(const char* fmt, ...) {
    char    message[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);
    if (begun) {
        serial.print(message);
        return;
    }
    if (!pending_errors.empty()) pending_errors += '\n';
    pending_errors += message;
}

Module* Os::get_module(std::string_view id) const {
    for (Module* module : modules) {
        if (module->get_id() == id) return module;
    }
    return nullptr;
}

const std::vector<Module*>& Os::get_modules() const { return modules; }

const OsConfig& Os::get_config() const { return config; }

void Os::print_banner() {
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


// ---- System --------------------------------------------------------------


System::System(Os& os)
    : Module(os,
          /* id                  */ "system",
          /* name                */ "System",
          /* description         */ "Stores integral commands and routines",
          /* requires_init_setup */ true,
          /* can_be_disabled     */ false,
          /* has_cli_cmds        */ true
    )
{
    register_command(Command{
        "restart",
        "Restart the ESP",
        std::string("$") + id + " restart",
        0,
        [this](xewe::span<const std::string>) {
            restart(1000);
        }
    });

    register_command(Command{
        "reboot",
        "Restart the ESP",
        std::string("$") + id + " reboot",
        0,
        [this](xewe::span<const std::string>) {
            restart(1000);
        }
    });

    register_command(Command{
        "info",
        "Chip and build info",
        std::string("$") + id + " info",
        0,
        [this](xewe::span<const std::string>) {
            esp_chip_info_t ci;
            esp_chip_info(&ci);

            uint8_t mac[6];
            esp_read_mac(mac, ESP_MAC_WIFI_STA);

            std::size_t flash_sz = ESP.getFlashChipSize();
            uint32_t    flash_hz = ESP.getFlashChipSpeed();

            std::string s;
            s += "Model ";
            s += std::to_string(static_cast<int>(ci.model));
            s += "  Cores ";
            s += std::to_string(static_cast<int>(ci.cores));
            s += "  Rev ";
            s += std::to_string(static_cast<int>(ci.revision));
            s += "\n";

            s += "IDF ";
            s += esp_get_idf_version();
            s += "\n";

            s += "Flash ";
            s += std::to_string(static_cast<unsigned>(flash_sz));
            s += " bytes @ ";
            s += std::to_string(static_cast<unsigned>(flash_hz));
            s += " Hz\n";

            char macs[18];
            snprintf(
                macs,
                sizeof(macs),
                "%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0],
                mac[1],
                mac[2],
                mac[3],
                mac[4],
                mac[5]
            );

            s += "MAC ";
            s += macs;

            this->os.serial.print(s.c_str(), xewe::str::kCRLF);
        }
    });

    register_command(Command{
        "set_device_name",
        "Set device name",
        std::string("$") + id + " set_device_name \"Kitchen Lights\"",
        1,
        [this](xewe::span<const std::string> args) {
            if (args.empty() || args[0].empty()) {
                this->os.serial.print(
                    ("Usage: $" + id + " set_device_name \"<name>\"").c_str(),
                    xewe::str::kCRLF
                );
                return;
            }

            std::string new_name = args[0];

            if (new_name.empty()) {
                this->os.serial.print(
                    "Device name cannot be empty",
                    xewe::str::kCRLF
                );
                return;
            }

            this->os.nvs.write<std::string>(id, "device_name", new_name);

            this->os.serial.print(
                ("Device name set to: " + new_name).c_str(),
                xewe::str::kCRLF
            );
        }
    });

    register_command(Command{
        "schema",
        "Settings of every module as JSON lines",
        std::string("$") + id + " schema",
        0,
        [this](xewe::span<const std::string>) {
            print_schema_all();
        }
    });

    register_command(Command{
        "mac",
        "Print MAC addresses",
        std::string("$") + id + " mac",
        0,
        [this](xewe::span<const std::string>) {
            struct Item {
                const char*    name;
                esp_mac_type_t type;
            };

            Item items[] = {
                {"wifi_sta", ESP_MAC_WIFI_STA},
                {"wifi_ap", ESP_MAC_WIFI_SOFTAP},
                {"bt", ESP_MAC_BT},
                {"eth", ESP_MAC_ETH},
            };

            for (const auto& item : items) {
                uint8_t mac[6];

                if (esp_read_mac(mac, item.type) == ESP_OK) {
                    char line[40];
                    snprintf(
                        line,
                        sizeof(line),
                        "%s %02X:%02X:%02X:%02X:%02X:%02X",
                        item.name,
                        mac[0],
                        mac[1],
                        mac[2],
                        mac[3],
                        mac[4],
                        mac[5]
                    );

                    this->os.serial.print(line, xewe::str::kCRLF);
                }
            }
        }
    });

    register_command(Command{
        "uid",
        "Device UID from eFuse base MAC (and SHA256-64)",
        std::string("$") + id + " uid",
        0,
        [this](xewe::span<const std::string>) {
            uint8_t mac[6];
            esp_efuse_mac_get_default(mac);

            uint8_t dig[32];
            mbedtls_sha256(mac, sizeof(mac), dig, 0 /* is224 */);

            this->os.serial.print(
                ("base_mac " + xewe::str::to_hex(mac, sizeof(mac))).c_str(),
                xewe::str::kCRLF
            );

            this->os.serial.print(
                ("uid64 " + xewe::str::to_hex(dig, 8)).c_str(),
                xewe::str::kCRLF
            );
        }
    });
}

void System::begin_routines_required() {
    const auto& cfg    = os.get_config();
    std::string header = cfg.project_name + "\\sep" + "Version " + cfg.version;
    if (!cfg.build_timestamp.empty()) header += "\nBuild Timestamp " + cfg.build_timestamp;
    if (!cfg.url.empty())             header += "\\sep" + cfg.url;

    os.serial.print_header(header);
    esp_log_level_set("*", ESP_LOG_NONE);
}

void System::begin_routines_init() {
#ifdef XEWE_DEVICE_NAME
    // build-time name: no prompt; a name already in NVS is kept (doc/os/os.md, "Build-time device name")
    if (os.nvs.read<std::string>(id, "device_name").empty()) {
        os.nvs.write<std::string>(id, "device_name", XEWE_DEVICE_NAME);
    }
    os.serial.print("Device name: " + os.nvs.read<std::string>(id, "device_name"));
#else
    std::string name      = "";
    bool        confirmed = false;
    while (!confirmed) {
        name      = os.serial.get_string("Name your device (ex: Kitchen Lights):");
        confirmed = os.serial.get_yn("Confirm \"" + name + "\"?");
    }
    os.nvs.write<std::string>(id, "device_name", name);
#endif
}

void System::reset(const bool verbose,
                   const bool do_restart,
                   const bool keep_enabled) {
    bool disable_confirmed = false;

    if (verbose) {
        os.serial.print_header("[WARNING]\nResetting System\nWill reset all modules");
        // bounded like Module::disable: two attempts of 15 s, anything but a clear "yes" cancels
        bool answered     = false;
        disable_confirmed = os.serial.get_yn("OK?", 2, 15000, false, answered);
        if (!answered) os.serial.print("! No answer: reset cancelled");
    }

    if (!disable_confirmed) {
        os.serial.print("Aborted");
        return;
    }

    const auto& modules = os.get_modules();

    for (Module* module : modules) {
        if (module == nullptr || module == this) continue;
        module->reset(true, false, false);
    }

    // factory reset: also wipes data stored outside module namespaces
    os.nvs.erase_all();

    Module::reset(verbose, do_restart, keep_enabled);
}

std::string System::status(const bool verbose) const {
    if (verbose) {
        std::vector<std::vector<std::string_view>> table_data;
        table_data.push_back({"Module Name", "Enabled", "Status"});

        const auto&              modules = os.get_modules();

        std::vector<std::string> string_storage;
        string_storage.reserve(modules.size() * 2);

        for (Module* mod : modules) {
            if (mod == nullptr) continue;

            std::string_view name = mod->get_name();

            string_storage.push_back(mod->is_enabled() ? "Yes" : "No");
            std::string_view enabled_view = string_storage.back();

            string_storage.push_back(mod->status(false));
            std::string_view status_view = string_storage.back();

            table_data.push_back({name, enabled_view, status_view});
        }

        os.serial.print_table(
            table_data,
            "System Status"
        );
    }

    return "System OK";
}

void System::print_schema_all() {
    const auto&                   modules = os.get_modules();
    std::vector<std::string_view> ids;
    ids.reserve(modules.size());
    for (Module* m : modules) ids.push_back(m->get_id());

    os.serial.print(Settings::header(XEWE_CORE_VERSION, get_device_name(), ids));
    SchemaOut out(os.serial);
    for (Module* m : modules) {
        out.set_module(m->get_id());
        m->print_schema(out);
    }
    os.serial.printf("{\"end\":\"%s\",\"count\":%u}", id.c_str(), static_cast<unsigned>(out.count()));
}

std::string System::get_device_name() {
    return os.nvs.read<std::string>(id, "device_name");
}

void System::restart(uint16_t delay_ms) {
    os.serial.print_header("Rebooting");
    delay(delay_ms);
    ESP.restart();
}

} // namespace xewe
