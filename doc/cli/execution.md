# Executing commands

`src/XeWeCore/Cli.h` — the two `execute` overloads, the tokenizer, and every error the parsed path
prints.

## execute (parsed)

```cpp
void execute(std::string_view input_line) const;
```

Parses a full line of the form `$<group> <command> [args...]` and dispatches it. Returns nothing:
**errors are printed to the serial port**, not returned.

This is what `loop()` calls for a typed line, and what you call to run a command from code — a
button handler, a schedule, a web request:

```cpp
xewe_cli.execute("$led set 10");
```

### Dispatch order

| Input | Result |
|---|---|
| empty or whitespace only | silently ignored |
| no leading `$` | `Error: commands must start with '$'; type $help` |
| `$` alone, or nothing after tokenizing | `Error: Missing command group; usage: $<group> <command> [args...]` |
| `$help` | [`print_all_commands()`](help.md) |
| `$help <group>` | [`print_help(group)`](help.md) |
| `$help a b` | `Error: Argument count mismatch for '$help'; usage: $help <group>` |
| unknown group | `Error: Unknown command group '<g>'` |
| group with no commands | `Error: Command group '<g>' has no CLI commands` |
| `$<group>` with no command | that group's help |
| `$<group> help` | that group's help — an alias |
| unknown command | `Error: Unknown command '<c>' in command group '<g>'` |
| wrong argument count | `Error: Argument count mismatch for '$<g> <c>'; expected <N>, got <M>`, then `Usage: <sample_usage>` when one is set |

Group ids and command names are both matched **case-insensitively**, so `$LED Set 128` works.

The check order matters: an unknown group is reported before an empty one, and the argument count
is checked before the handler runs, so a handler never sees the wrong number of arguments.

## execute (direct)

```cpp
bool execute(std::string_view             group_id,
             std::string_view             command_name,
             xewe::span<const std::string> args) const;
```

Skips parsing entirely and returns whether a command ran. It prints nothing — not even on failure.

```cpp
std::vector<std::string> args{"128"};
if (!xewe_cli.execute("led", "set", args)) { /* no such command */ }
```

A command matches when its `function` is non-empty, its name matches case-insensitively, **and
`args.size() == command.arg_count`**. Because the argument count participates in matching, two
commands sharing a name but differing in arity act as an overload set here — unlike the parsed
path, which matches on name alone and takes the first hit.

Returns `false` when the group is unknown or nothing matched.

## Handler arguments

A handler receives `xewe::span<const std::string>` over a vector owned by `execute`.

**The span does not outlive the call.** Copy anything you intend to keep:

```cpp
{"name", "Set the name", "$dev name \"Kitchen\"", 1,
 [this](xewe::span<const std::string> args) { stored_name = args[0]; }}   // copy, not a view
```

## Tokenizer

The `$` is stripped, then the rest is split on whitespace:

* A token starting with `"` runs to the closing `"` and may contain spaces.
* Inside quotes, `\` escapes the next character. A trailing dangling `\` is emitted literally.
* **Quoting only applies when `"` is the first character of a token** — `ab"cd"` is one literal
  token including the quotes.
* An unterminated quote prints `Error: Unterminated quote in command.` and **aborts the whole
  line**; nothing runs.
* `$ led set 10`, with a space after the `$`, is accepted.

```
$dev name "Kitchen Lights"       -> args: ["Kitchen Lights"]
$dev name "say \"hi\""           -> args: ["say \"hi\""]
```
