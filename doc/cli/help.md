# Help output

`src/XeWeCore/Cli.h` — the generated `$help` tables.

## print_all_commands

```cpp
void print_all_commands() const;
```

Prints one table per group, in **alphabetical order by group id**. This is what `$help` runs.

**Groups with no commands are skipped**, so a group registered but never populated is invisible in
help — a useful check when a command seems missing.

## print_help

```cpp
void print_help(std::string_view group_id) const;
```

Prints one group's table. The id is trimmed; an **empty id falls through to
`print_all_commands()`**. This is what `$help <group>`, `$<group>` and `$<group> help` run.

## What a table looks like

The table is rendered with
[`SerialPort::print_table`](../serial/output.md#print_table-and-render_table)
using all of that function's defaults — 30-character column cap, `|` edges, `+` corners. Its title
is `<group name> Commands [<group id>]`, and the columns are fixed:

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

Long descriptions wrap inside their column rather than widening the table.
