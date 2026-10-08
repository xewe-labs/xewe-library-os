// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Cli.cpp

#include "Cli.h"

#include <algorithm>
#include <cctype>

#include "Utils.h"


namespace xewe {

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
    const std::string key = lower_copy(trim_copy(id));
    CommandGroup&     group = groups[key];
    group.id                = key;
    group.name              = std::string(name);
    return group;
}

bool Cli::add_command(std::string_view group_id,
                                  Command command) {
    auto it = groups.find(lower_copy(trim_copy(group_id)));
    if (it == groups.end() || command.name.empty() || !command.function) return false;

    it->second.commands.push_back(std::move(command));
    return true;
}

bool Cli::remove_group(std::string_view id) {
    return groups.erase(lower_copy(trim_copy(id))) > 0;
}

const CommandGroup* Cli::get_group(std::string_view id) const {
    auto it = groups.find(lower_copy(trim_copy(id)));
    return it == groups.end() ? nullptr : &it->second;
}

const std::map<std::string, CommandGroup>& Cli::get_groups() const { return groups; }

bool Cli::execute(std::string_view group_id,
                              std::string_view command_name,
                              xewe::span<const std::string> args) const {
    const CommandGroup* group = get_group(group_id);
    if (group == nullptr) return false;

    const std::string name = lower_copy(command_name);

    for (const Command& command : group->commands) {
        if (!command.function)                  continue;
        if (lower_copy(command.name) != name)   continue;
        if (args.size() != command.arg_count)   continue;

        command.function(args);
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

    if (tokens.empty()) {
        serial.print(
            "Error: Missing command group; usage: $<group> <command> [args...]",
            xewe::str::kCRLF
        );
        return;
    }

    if (lower_copy(tokens[0]) == "help") {
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
            "Error: Unknown command group '%s'\r\n",
            tokens[0].c_str()
        );
        return;
    }

    const auto& commands = group->commands;

    if (commands.empty()) {
        serial.printf(
            "Error: Command group '%s' has no CLI commands\r\n",
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
            "Error: Missing command in command group '%s'; usage: $%s <command> [args...]\r\n",
            tokens[0].c_str(),
            tokens[0].c_str()
        );
        return;
    }

    if (lower_copy(tokens[1]) == "help") {
        print_help(tokens[0]);
        return;
    }

    const std::string command_name    = lower_copy(tokens[1]);
    const Command*    matched_command = nullptr;

    for (const Command& command : commands) {
        if (command.name.empty() || !command.function) {
            continue;
        }

        if (lower_copy(command.name) == command_name) {
            matched_command = &command;
            break;
        }
    }

    if (matched_command == nullptr) {
        serial.printf(
            "Error: Unknown command '%s' in command group '%s'\r\n",
            tokens[1].c_str(),
            tokens[0].c_str()
        );
        return;
    }

    const std::size_t provided_arg_count = tokens.size() - 2;
    const std::size_t expected_arg_count = matched_command->arg_count;

    if (provided_arg_count != expected_arg_count) {
        serial.printf(
            "Error: Argument count mismatch for '$%s %s'; expected %u, got %u\r\n",
            tokens[0].c_str(),
            tokens[1].c_str(),
            static_cast<unsigned>(expected_arg_count),
            static_cast<unsigned>(provided_arg_count)
        );

        if (!matched_command->sample_usage.empty()) {
            serial.printf(
                "Usage: %s\r\n",
                std::string(matched_command->sample_usage).c_str()
            );
        }

        return;
    }

    std::vector<std::string> args;
    args.reserve(provided_arg_count);

    for (std::size_t i = 2; i < tokens.size(); ++i) {
        args.push_back(tokens[i]);
    }

    matched_command->function(xewe::span<const std::string>(args.data(), args.size()));
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
            "Error: Unknown command group '%s'\r\n",
            id.c_str()
        );
        return;
    }

    const auto& commands = group->commands;

    if (commands.empty()) {
        serial.printf(
            "Error: Command group '%s' has no CLI commands\r\n",
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

std::string Cli::trim_copy(std::string_view value) {
    const auto is_space = [](unsigned char c) {
        return std::isspace(c) != 0;
    };

    std::size_t begin = 0;
    std::size_t end   = value.size();

    while (begin < end && is_space(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    while (end > begin && is_space(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return std::string(value.substr(begin, end - begin));
}

std::string Cli::lower_copy(std::string_view value) {
    std::string out(value.begin(), value.end());

    std::transform(
        out.begin(),
        out.end(),
        out.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        }
    );

    return out;
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

            if (escape) {
                token.push_back('\\');
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
