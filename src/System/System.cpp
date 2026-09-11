// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-library-os/src/System/System.cpp

#include "System.h"
#include "../ModuleController/ModuleController.h"


namespace xewe::os {


System::System(ModuleController& controller)
    : Module(controller,
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
        [this](std::span<const std::string>) {
            restart(1000);
        }
    });

    register_command(Command{
        "reboot",
        "Restart the ESP",
        std::string("$") + id + " reboot",
        0,
        [this](std::span<const std::string>) {
            restart(1000);
        }
    });

    register_command(Command{
        "info",
        "Chip and build info",
        std::string("$") + id + " info",
        0,
        [this](std::span<const std::string>) {
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

            this->controller.serial.print(s.c_str(), xewe::str::kCRLF);
        }
    });

    register_command(Command{
        "set_device_name",
        "Set device name",
        std::string("$") + id + " set_device_name \"Kitchen Lights\"",
        1,
        [this](std::span<const std::string> args) {
            if (args.empty() || args[0].empty()) {
                this->controller.serial.print(
                    ("Usage: $" + id + " set_device_name \"<name>\"").c_str(),
                    xewe::str::kCRLF
                );
                return;
            }

            std::string new_name = args[0];

            if (new_name.empty()) {
                this->controller.serial.print(
                    "Device name cannot be empty",
                    xewe::str::kCRLF
                );
                return;
            }

            this->controller.nvs.write<std::string>(id, "device_name", new_name);

            this->controller.serial.print(
                ("Device name set to: " + new_name).c_str(),
                xewe::str::kCRLF
            );
        }
    });

    register_command(Command{
        "mac",
        "Print MAC addresses",
        std::string("$") + id + " mac",
        0,
        [this](std::span<const std::string>) {
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

                    this->controller.serial.print(line, xewe::str::kCRLF);
                }
            }
        }
    });

    register_command(Command{
        "uid",
        "Device UID from eFuse base MAC (and SHA256-64)",
        std::string("$") + id + " uid",
        0,
        [this](std::span<const std::string>) {
            uint8_t mac[6];
            esp_efuse_mac_get_default(mac);

            uint8_t dig[32];
            mbedtls_sha256(mac, sizeof(mac), dig, 0 /* is224 */);

            this->controller.serial.print(
                ("base_mac " + xewe::str::to_hex(mac, sizeof(mac))).c_str(),
                xewe::str::kCRLF
            );

            this->controller.serial.print(
                ("uid64 " + xewe::str::to_hex(dig, 8)).c_str(),
                xewe::str::kCRLF
            );
        }
    });
}

void System::begin_routines_required() {
    const auto& cfg    = controller.get_config();
    std::string header = cfg.project_name + "\\sep" + "Version " + cfg.version;
    if (!cfg.build_timestamp.empty()) header += "\nBuild Timestamp " + cfg.build_timestamp;
    if (!cfg.url.empty())             header += "\\sep" + cfg.url;

    controller.serial.print_header(header);
    esp_log_level_set("*", ESP_LOG_NONE);
}

void System::begin_routines_init() {
    std::string name      = "";
    bool        confirmed = false;
    while (!confirmed) {
        name      = controller.serial.get_string("Name your device (ex: Kitchen Lights):");
        confirmed = controller.serial.get_yn("Confirm \"" + name + "\"?");
    }
    controller.nvs.write<std::string>(id, "device_name", name);
}

void System::reset(const bool verbose,
                   const bool do_restart,
                   const bool keep_enabled) {
    bool disable_confirmed = false;

    if (verbose) {
        controller.serial.print_header("[WARNING]\nResetting System\nWill reset all modules");
        disable_confirmed = controller.serial.get_yn("OK?");
    }

    if (!disable_confirmed) {
        controller.serial.print("Aborted");
        return;
    }

    const auto& modules = controller.get_modules();

    for (Module* module : modules) {
        if (module == nullptr || module == this) continue;
        module->reset(true, false, false);
    }

    // factory reset: also wipes data stored outside module namespaces
    controller.nvs.erase_all();

    Module::reset(verbose, do_restart, keep_enabled);
}

std::string System::status(const bool verbose) const {
    if (verbose) {
        std::vector<std::vector<std::string_view>> table_data;
        table_data.push_back({"Module Name", "Enabled", "Status"});

        const auto&              modules = controller.get_modules();

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

        controller.serial.print_table(
            table_data,
            "System Status"
        );
    }

    return "System OK";
}

std::string System::get_device_name() {
    return controller.nvs.read<std::string>(id, "device_name");
}

void System::restart(uint16_t delay_ms) {
    controller.serial.print_header("Rebooting");
    delay(delay_ms);
    ESP.restart();
}

} // namespace xewe::os
