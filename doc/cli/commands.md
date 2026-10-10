# Commands and groups

`src/XeWeCore/Cli.h` — the types a command is made of, and how groups are registered.

## Types

```cpp
using command_function_t = std::function<void(xewe::span<const std::string> args)>;

struct Command {
    std::string        name;
    std::string        description;
    std::string        sample_usage;
    std::size_t        arg_count;
    command_function_t function;
};

struct CommandGroup {
    std::string          id;
    std::string          name;
    std::vector<Command> commands;
};
```

**No member has a default initializer**, so aggregate initialization must supply all five fields
of `Command`, in order:

```cpp
{"set", "Set level 0-255", "$led set 128", 1,
 [](xewe::span<const std::string> args) { analogWrite(8, atoi(args[0].c_str())); }}
```

| Field | |
|---|---|
| `name` | typed after the group; matched **case-insensitively** |
| `description` | shown in the `$help` table |
| `sample_usage` | printed when the argument count is wrong; may be empty |
| `arg_count` | exact number of arguments required, not a minimum |
| `function` | the handler; must not be empty |

`CommandGroup::id` holds the normalized (trimmed, lowercased) id.

## Commands a module gets from the core

`xewe::Module` registers `status` and `reset` in every module's group, plus `enable` and `disable`
when the module can be disabled. A module with a [settings table](../os/settings.md) also gets
`set`, `get` and `schema` at `begin()`, except names the module registered itself. They are
ordinary commands; see [`os/module.md`](../os/module.md#generic-cli-commands).

## Constructor

```cpp
explicit Cli(SerialPort& serial);
```

The `Cli` stores a **reference** to the port, which must outlive it. Both are normally globals:

```cpp
xewe::SerialPort serial;
xewe::Cli        xewe_cli(serial);
```

**Do not write `cli(` or `cli (`.** The ESP32 Arduino core defines `cli` as a function-like macro,
so `xewe::Cli cli(serial);` expands to something that does not compile. Brace initialization
(`cli{serial}`) and member access are fine: `XeWeOs` names its member `cli` (`os.cli.execute(...)`)
and initialises it with braces. A standalone object in a sketch is best named `xewe_cli`.

## loop

```cpp
void loop();
```

Non-blocking: calls `serial.loop()` and, when a complete line is available, executes it. Call it
every sketch iteration.

A handler blocks this loop while it runs: a command that calls `delay()` stops everything else.

## add_group

```cpp
CommandGroup& add_group(std::string_view id, std::string_view name);
```

Creates the group, or returns the existing one. The `id` is trimmed and lowercased; it is what the
user types after `$`. The `name` is the display title in help output.

**Re-adding an existing id keeps its commands and only updates the display name.** The returned
reference is a live handle into the internal map.

## add_command

```cpp
bool add_command(std::string_view group_id, Command command);
```

Appends a command to a group. Returns `false` and adds nothing when:

* the group does not exist,
* `command.name` is empty or contains whitespace (the tokenizer splits on it, so it could never be
  typed), or
* `command.function` is empty.

Through [`Module::register_command`](../os/module.md) a rejected command is also reported, for
example `! $<id> command '<name>' contains whitespace: not registered`. The other reasons are
`is empty` and `has no function`.

**Duplicate command names are not rejected.** Commands with the same name act as an overload set
on `arg_count`: both [`execute`](execution.md) overloads pick the command whose name **and**
argument count match. When no count matches, the parsed path reports the mismatch against the
first command of that name. Of two commands with the same name and count, the first one registered
wins; the second is unreachable.

**Module ids are checked before their group is created.** `Os::register_module` refuses a module
whose id is empty, contains whitespace, equals `help` (`$help` is intercepted), is longer than 15
characters (the NVS namespace limit), or is already registered. The last check also compares
case-insensitively against existing groups. It prints, for example,
`! Module id '<id>' is already registered: module not registered`. A refused module gets no group
and no commands, so a duplicate never merges into the first module's group. The checks are
`Cli::name_error(name, is_module_id)`.

`add_group` itself does not validate. Called directly, it merges into an existing id, and a group
id of `help` or one with whitespace is registered but reachable only through the direct `execute`
overload.

Commands are stored in registration order, which is the order `$help` lists them in.

## remove_group

```cpp
bool remove_group(std::string_view id);
```

Erases a group and all of its commands. Returns `true` if something was erased.

## get_group and get_groups

```cpp
const CommandGroup*                        get_group (std::string_view id) const;
const std::map<std::string, CommandGroup>& get_groups()                    const;
```

`get_group` returns `nullptr` for an unknown id. `get_groups` exposes the map keyed by lowercase
id, so iterating it is alphabetical by id.
