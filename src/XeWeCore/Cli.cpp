// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Cli.cpp

#include "Cli.h"

#include <cctype>

#include "Utils.h"


namespace xewe {

namespace {

std::string trim_copy(std::string_view value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return std::string(value.substr(begin, end - begin));
}

// trimmed and lower-cased: the key of a group id
std::string group_key(std::string_view id) {
    std::string key = trim_copy(id);
    for (char& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return key;
}

// ASCII case-insensitive equality, no allocation
bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    }
    return true;
}

} // namespace

Cli::Cli(SerialPort& serial)
    : serial(serial)
{}

void Cli::loop() {
    serial.loop();
    if (serial.has_line()) {
        execute(serial.read_line());
    }
}

CommandGroup& Cli::add_group(std::string_view id,
                                         std::string_view name) {
    const std::string key = group_key(id);
    CommandGroup&     group = groups[key];
    group.id                = key;
    group.name              = std::string(name);
    return group;
}

bool Cli::add_command(std::string_view group_id,
                                  Command command) {
    auto it = groups.find(group_key(group_id));
    if (it == groups.end() || name_error(command.name, false) || !command.function) return false;

    it->second.commands.push_back(std::move(command));
    return true;
}

const char* Cli::name_error(std::string_view name, bool is_module_id) {
    if (name.empty()) return "is empty";
    for (unsigned char c : name) {
        if (std::isspace(c)) return "contains whitespace";
    }
    if (!is_module_id) return nullptr;
    if (iequals(name, "help")) return "is reserved ($help)";
    if (name.size() > 15) return "is longer than 15 characters (NVS namespace limit)";
    return nullptr;
}

bool Cli::remove_group(std::string_view id) {
    return groups.erase(group_key(id)) > 0;
}

const CommandGroup* Cli::get_group(std::string_view id) const {
    auto it = groups.find(group_key(id));
    return it == groups.end() ? nullptr : &it->second;
}

const std::map<std::string, CommandGroup>& Cli::get_groups() const { return groups; }

bool Cli::execute(std::string_view group_id,
                              std::string_view command_name,
                              xewe::span<const std::string> args) const {
    const CommandGroup* group = get_group(group_id);
    if (group == nullptr) return false;

    for (const Command& command : group->commands) {
        if (!command.function)                  continue;
        if (!iequals(command.name, command_name)) continue;
        if (args.size() != command.arg_count)   continue;

        // call a copy: the command may add/remove commands or groups, which can destroy
        // the stored std::function while it is still running
        const command_function_t function = command.function;
        function(args);
        return true;
    }
    return false;
}

void Cli::execute(std::string_view input_line) const {
    std::string local = trim_copy(input_line);

    if (local.empty()) return;

    if (local[0] != '$') {
        serial.print(
            "Error: commands must start with '$'; type $help",
            xewe::str::kCRLF
        );
        return;
    }

    local.erase(0, 1);
    local = trim_copy(local);

    if (local.empty()) {
        serial.print(
            "Error: Missing command group; usage: $<group> <command> [args...]",
            xewe::str::kCRLF
        );
        return;
    }

    std::vector<std::string> tokens;

    if (!tokenize(local, tokens)) return;

    if (iequals(tokens[0], "help")) {
        if (tokens.size() == 1) {
            print_all_commands();
            return;
        }

        if (tokens.size() != 2) {
            serial.print(
                "Error: Argument count mismatch for '$help'; usage: $help <group>",
                xewe::str::kCRLF
            );
            return;
        }

        print_help(tokens[1]);
        return;
    }

    const CommandGroup* group = get_group(tokens[0]);

    if (group == nullptr) {
        serial.printf(
            "Error: Unknown command group '%s'",
            tokens[0].c_str()
        );
        return;
    }

    const auto& commands = group->commands;

    if (commands.empty()) {
        serial.printf(
            "Error: Command group '%s' has no CLI commands",
            tokens[0].c_str()
        );
        return;
    }

    if (tokens.size() == 1) {
        print_help(tokens[0]);
        return;
    }

    if (tokens[1].empty()) {
        serial.printf(
            "Error: Missing command in command group '%s'; usage: $%s <command> [args...]",
            tokens[0].c_str(),
            tokens[0].c_str()
        );
        return;
    }

    if (iequals(tokens[1], "help")) {
        print_help(tokens[0]);
        return;
    }

    const Command*    matched_command = nullptr;

    const std::size_t provided_arg_count = tokens.size() - 2;

    // same rule as execute(group, name, args): a command name may be registered more than
    // once with different arg counts; pick the one whose count matches, else report the first
    for (const Command& command : commands) {
        if (command.name.empty() || !command.function) {
            continue;
        }

        if (iequals(command.name, tokens[1])) {
            if (matched_command == nullptr) matched_command = &command;
            if (command.arg_count == provided_arg_count) {
                matched_command = &command;
                break;
            }
        }
    }

    if (matched_command == nullptr) {
        serial.printf(
            "Error: Unknown command '%s' in command group '%s'",
            tokens[1].c_str(),
            tokens[0].c_str()
        );
        return;
    }

    const std::size_t expected_arg_count = matched_command->arg_count;

    if (provided_arg_count != expected_arg_count) {
        serial.printf(
            "Error: Argument count mismatch for '$%s %s'; expected %u, got %u",
            tokens[0].c_str(),
            tokens[1].c_str(),
            static_cast<unsigned>(expected_arg_count),
            static_cast<unsigned>(provided_arg_count)
        );

        if (!matched_command->sample_usage.empty()) {
            serial.printf(
                "Usage: %s",
                matched_command->sample_usage.c_str()
            );
        }

        return;
    }

    // call a copy (see execute(group, name, args)); `commands` may be invalid afterwards
    const command_function_t function = matched_command->function;
    function(xewe::span<const std::string>(tokens.data() + 2, provided_arg_count));
}

void Cli::print_help(std::string_view group_id) const {
    const std::string id = trim_copy(group_id);

    if (id.empty()) {
        print_all_commands();
        return;
    }

    const CommandGroup* group = get_group(id);

    if (group == nullptr) {
        serial.printf(
            "Error: Unknown command group '%s'",
            id.c_str()
        );
        return;
    }

    const auto& commands = group->commands;

    if (commands.empty()) {
        serial.printf(
            "Error: Command group '%s' has no CLI commands",
            id.c_str()
        );
        return;
    }

    std::vector<std::vector<std::string_view>> table_data;
    table_data.push_back({"Command", "Args", "Description", "Sample Usage"});

    std::vector<std::string> arg_counts;
    arg_counts.reserve(commands.size());

    for (const Command& command : commands) {
        if (command.name.empty() || !command.function) {
            continue;
        }

        arg_counts.push_back(std::to_string(command.arg_count));

        table_data.push_back({
            command.name,
            arg_counts.back(),
            command.description,
            command.sample_usage
        });
    }

    const std::string header =
        group->name +
        " Commands [" +
        group->id +
        "]";

    serial.print_table(table_data, header);
}

void Cli::print_all_commands() const {
    for (const auto& [id, group] : groups) {
        if (!group.commands.empty()) {
            print_help(id);
        }
    }
}

bool Cli::tokenize(std::string_view input,
                               std::vector<std::string>& out) const {
    out.clear();

    std::size_t pos = 0;

    while (pos < input.size()) {
        while (pos < input.size() &&
               std::isspace(static_cast<unsigned char>(input[pos])) != 0) {
            ++pos;
        }

        if (pos >= input.size()) {
            break;
        }

        std::string token;

        if (input[pos] == '"') {
            ++pos;

            bool closed = false;
            bool escape = false;

            while (pos < input.size()) {
                const char c = input[pos++];

                if (escape) {
                    token.push_back(c);
                    escape = false;
                    continue;
                }

                if (c == '\\') {
                    escape = true;
                    continue;
                }

                if (c == '"') {
                    closed = true;
                    break;
                }

                token.push_back(c);
            }

            if (!closed) {
                serial.print(
                    "Error: Unterminated quote in command.",
                    xewe::str::kCRLF
                );
                return false;
            }
        } else {
            while (pos < input.size() &&
                   std::isspace(static_cast<unsigned char>(input[pos])) == 0) {
                token.push_back(input[pos++]);
            }
        }

        out.push_back(token);
    }

    return true;
}

} // namespace xewe
