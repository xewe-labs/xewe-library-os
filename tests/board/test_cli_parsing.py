"""Black-box tests of the command line parser (``Cli::execute``, ``Cli::tokenize``, ``Cli::print_help``)."""

import re

HEADER_RX = re.compile(r"^\|\s+(.+) Commands \[(\w+)\]\s+\|$")


def headers(lines):
    return [m[2] for line in lines if (m := HEADER_RX.match(line.strip()))]


def test_help_lists_every_group_in_order(cli):
    """Cli::print_all_commands: one table per group with commands, sorted by id (std::map).

    The group set depends on the image (testing v1: 9 modules + the template's your_module and
    your_mod_full, plus `test` on a hooks image), so the test checks the order, the groups the
    core tests need, and `test` exactly when the hooks are built in."""
    out = cli.run("$help", quiet=1.0)
    groups = headers(out)
    assert groups == sorted(set(groups)), groups
    assert set(cli.required_groups) <= set(groups), groups
    assert ("test" in groups) == cli.hooks, groups
    assert sum("| Command " in l for l in out) == len(groups)


def test_group_help_forms(cli):
    """Cli::execute: `$<group> help`, `$<group>` alone and `$help <group>` all print the group table."""
    for cmd in ("$pins help", "$pins", "$help pins", "$PINS HELP"):
        out = cli.run(cmd)
        assert headers(out) == ["pins"], (cmd, out)
        assert any(re.match(r"^\|\s*adc_read\s*\|\s*1\s*\|", l) for l in out), cmd


def test_help_errors(cli):
    """Cli::execute: `$help` with two args, and `$help <unknown>`."""
    assert "Error: Argument count mismatch for '$help'; usage: $help <group>" in cli.run("$help system extra")
    assert "Error: Unknown command group 'nope'" in cli.run("$help nope")


def test_case_insensitive(cli):
    """Cli::execute/lower_copy: group and command names are case-insensitive."""
    assert any("Pins module enabled" in l for l in cli.run("$PINS Status"))
    assert any("System Status" in l for l in cli.run("$SyStEm STATUS"))


def test_missing_dollar(cli):
    """Cli::execute: a line not starting with '$'."""
    assert cli.run("system status") == ["Error: commands must start with '$'; type $help"]


def test_bare_dollar(cli):
    """Cli::execute: `$` and `$   ` → missing group."""
    for cmd in ("$", "$    "):
        assert "Error: Missing command group; usage: $<group> <command> [args...]" in cli.run(cmd)


def test_unknown_group_and_command(cli):
    """Cli::execute: unknown group, unknown command (names echoed as typed)."""
    assert "Error: Unknown command group 'nope'" in cli.run("$nope status")
    assert "Error: Unknown command 'Frob' in command group 'system'" in cli.run("$system Frob")


def test_wrong_arg_count(cli):
    """Cli::execute: argument count mismatch prints expected/got and the sample usage."""
    out = cli.run("$system restart now")
    assert "Error: Argument count mismatch for '$system restart'; expected 0, got 1" in out
    assert "Usage: $system restart" in out
    out = cli.run("$pins adc_read")
    assert "Error: Argument count mismatch for '$pins adc_read'; expected 1, got 0" in out


def test_unterminated_quote(cli):
    """Cli::tokenize: an unclosed quote is rejected before dispatch."""
    assert "Error: Unterminated quote in command." in cli.run('$system set_device_name "abc')


def test_quoted_args_with_spaces_and_escapes(cli):
    """Cli::tokenize: quoted arg keeps spaces, \\" becomes ", round-trips through $buttons add/status."""
    cli.clear_buttons()
    out = cli.run(f'$buttons add {cli.test_pin} "$system \\"a b\\" status" pullup on_press 50')
    assert "Successfully added button mapping." in out
    rows = [l for l in cli.run("$buttons status") if l.strip().startswith("| 0 ")]
    assert rows and '$system "a b" status' in rows[0], rows
    assert "Successfully removed button mapping." in cli.run("$buttons remove 0")


def test_quoted_pipe_list_in_schedule(cli):
    """Cli::tokenize: double spaces inside quotes are kept; escaped quotes survive into JSON."""
    cli.clear_schedules()
    assert "Scheduler: schedule saved" in cli.run('$schedule add 1439 1439 6 00FF00 "$pins  status|\\"q\\""')
    out = "\n".join(cli.run("$schedule status"))
    assert '"commands":["$pins  status","\\"q\\""]' in out, out
    assert "Scheduler: schedule removed" in cli.run("$schedule remove 0")


def test_empty_and_whitespace_lines_ignored(cli):
    """Cli::execute: empty and whitespace-only lines print nothing (only the echo)."""
    for cmd in ("", "   ", " \t  "):
        out = cli.run(cmd)
        assert all(l.strip() == "" for l in out), (cmd, out)


def test_leading_trailing_and_inner_spaces(cli):
    """Cli::execute/trim_copy/tokenize: surrounding and repeated spaces are ignored."""
    for cmd in ("   $pins status   ", "$   pins    status", "\t$pins\tstatus\t"):
        assert any("Pins module enabled" in l for l in cli.run(cmd)), repr(cmd)


def test_254_char_line_is_one_command(cli):
    """SerialPort::loop: 254 usable chars per line; a line of exactly 254 chars runs whole."""
    line = "$pins status".ljust(254)
    assert len(line) == 254
    out = cli.run(line)
    assert out.count("Pins module enabled") == 1
    assert not any(l.startswith("Error") for l in out), out


TOO_LONG = "! Input line too long (max 254 chars): dropped"


def test_300_char_line_is_dropped_whole(cli):
    """SerialPort::loop: a line past 254 chars is discarded up to its newline, with one notice."""
    line = "$pins status".ljust(254) + "x" * 46
    out = cli.run(line)
    assert sum(TOO_LONG in l for l in out) == 1, out
    assert not any("Pins module enabled" in l for l in out), out
    assert not any(l.startswith("Error") for l in out), out
    assert any("Time module enabled" in l for l in cli.run("$time status"))


def test_long_line_tail_does_not_run(cli):
    """SerialPort::loop: nothing of an over-long line executes, not even a '$' command in the tail."""
    line = "$pins status".ljust(254) + "$time status"
    out = cli.run(line)
    assert sum(TOO_LONG in l for l in out) == 1, out
    assert not any("Pins module enabled" in l or "Time module enabled" in l for l in out), out
    assert any("Pins module enabled" in l for l in cli.run("$pins status"))
