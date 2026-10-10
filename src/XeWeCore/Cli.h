// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Cli.h
#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "Serial.h"
#include "Utils.h"


namespace xewe {

using command_function_t = std::function<void(xewe::span<const std::string> args)>;

struct Command {
    std::string              name;
    std::string              description;
    std::string              sample_usage;
    std::size_t              arg_count;
    command_function_t       function;
};

struct CommandGroup {
    std::string              id;
    std::string              name;
    std::vector<Command>     commands;
};

// Parses lines of the form `$<group> <command> [args...]` and dispatches them to
// registered commands. Quoted arguments ("with spaces") and \-escapes are supported.
// `$help` lists every group, `$help <group>` / `$<group>` lists one.
class Cli {
public:
    explicit                 Cli                (SerialPort& serial);

    // non-blocking: polls the serial port and executes any complete line
    void                     loop               ();

    // registration; group ids are case-insensitive. add_group on an existing id
    // keeps its commands and only updates the display name.
    CommandGroup&            add_group          (std::string_view id,
                                                 std::string_view name);
    bool                     add_command        (std::string_view group_id,
                                                 Command          command);
    bool                     remove_group       (std::string_view id);
    const CommandGroup*      get_group          (std::string_view id)            const;
    const std::map<std::string, CommandGroup>&
                             get_groups         ()                               const;

    // why `name` cannot be registered (nullptr if it can): empty or contains whitespace (the
    // tokenizer splits on it). A module id is also a group id and an NVS namespace: `help` is
    // reserved and more than 15 characters is rejected. Used by add_command and Os::register_module.
    static const char*       name_error         (std::string_view name,
                                                 bool             is_module_id);

    // execution
    void                     execute            (std::string_view input_line)    const;
    bool                     execute            (std::string_view             group_id,
                                                 std::string_view             command_name,
                                                 xewe::span<const std::string> args) const;

    void                     print_help         (std::string_view group_id)      const;
    void                     print_all_commands ()                               const;

private:
    SerialPort&                         serial;
    std::map<std::string, CommandGroup> groups;

    bool                     tokenize           (std::string_view          input,
                                                 std::vector<std::string>& out) const;
};

} // namespace xewe
