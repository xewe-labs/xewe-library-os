# Help output

`src/XeWeCore/Cli.h` — the generated `$help` tables.

## print_all_commands

```cpp
void print_all_commands() const;
```

Prints one table per group, in **alphabetical order by group id**. This is what `$help` runs.

**Groups with no commands are skipped.** A group registered but never filled does not appear in
help.

## print_help

```cpp
void print_help(std::string_view group_id) const;
```

Prints one group's table. The id is trimmed and matched case-insensitively; an **empty id runs
`print_all_commands()`**. `$help <group>`, `$<group>` and `$<group> help` run it. An unknown id
prints `Error: Unknown command group '<g>'`; a group without commands prints
`Error: Command group '<g>' has no CLI commands`.

## What a table looks like

The table is rendered with
[`SerialPort::print_table`](../serial/output.md#print_table-and-render_table)
with that function's defaults: 30-character column cap, `|` edges, `+` corners. Its title is
`<group name> Commands [<group id>]`, and the columns are fixed:

```
+-------------------------------------------------+
|               LED Commands [led]                |
+---------+------+-----------------+--------------+
| Command | Args | Description     | Sample Usage |
+---------+------+-----------------+--------------+
| set     | 1    | Set level 0-255 | $led set 128 |
+---------+------+-----------------+--------------+
```

Commands are listed in registration order. A command with an empty `name` or an empty `function`
is skipped, which is the same condition that makes it unreachable from
[`execute`](execution.md).

Long descriptions wrap inside their column instead of widening the table.
