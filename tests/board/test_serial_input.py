"""Black-box tests of the serial input queue (``SerialPort::loop``/``push_line``)."""

import time

CMDS = [
    ("$pins status", "Pins module enabled"),
    ("$time status", "Time module enabled"),
    ("$schedule status", "Scheduler module enabled"),
    ("$buttons status", "Buttons module enabled"),
    ("$wifi status", "Wifi module enabled"),
    ("$web_interface status", "Web Interface module enabled"),
]
OVERFLOW = "! Input overflow: line dropped"


def expect_in_order(cli, pats, timeout=10):
    for p in pats:
        cli.c.expect(p, timeout)


def test_two_lines_one_write(cli):
    """SerialPort queue: 2 lines in one write both run, in order."""
    cli.write("".join(c + "\n" for c, _ in CMDS[:2]).encode())
    expect_in_order(cli, [p for _, p in CMDS[:2]])
    assert OVERFLOW not in cli.settle()


def test_four_lines_one_write(cli):
    """SerialPort queue: 4 lines (the queue depth) in one write all run, in order, no overflow."""
    cli.write("".join(c + "\n" for c, _ in CMDS[:4]).encode())
    start = len(cli.c.lines)
    expect_in_order(cli, [p for _, p in CMDS[:4]])
    cli.settle()
    assert OVERFLOW not in cli.c.lines[start:]


def test_five_lines_one_write_drops_newest(cli):
    """SerialPort::push_line: the 5th queued line is dropped with one overflow notice; 4 replies."""
    start = len(cli.c.lines)
    cli.write("".join(c + "\n" for c, _ in CMDS[:5]).encode())
    expect_in_order(cli, [p for _, p in CMDS[:4]])
    out = cli.c.lines[start:] + cli.settle(1.0)
    assert out.count(OVERFLOW) == 1, out
    assert not any(CMDS[4][1] in l for l in out), "dropped line was executed"


def test_six_separate_sends_back_to_back(cli):
    """SerialPort queue: 6 sends without waiting; replies in order; any loss is reported as overflow."""
    start = len(cli.c.lines)
    for c, _ in CMDS:
        cli.c._ser.write((c + "\n").encode())
    cli.c._ser.flush()
    time.sleep(3)
    out = cli.c.lines[start:] + cli.settle(1.0)
    replies = [p for _, p in CMDS if any(p in l for l in out)]
    idx = [next(i for i, l in enumerate(out) if p in l) for p in replies]
    print(f"6 sends: {len(replies)} replies, {out.count(OVERFLOW)} overflow notices")
    assert idx == sorted(idx), out
    assert len(replies) + out.count(OVERFLOW) == 6, out
    assert len(replies) >= 4


def test_crlf_line_endings(cli):
    """SerialPort::loop: '\\r' is ignored, so CRLF lines work (single and burst)."""
    cli.write(b"$pins status\r\n")
    cli.c.expect("Pins module enabled")
    cli.write(b"$pins status\r\n$time status\r\n")
    expect_in_order(cli, ["Pins module enabled", "Time module enabled"])


def test_binary_garbage_then_command(cli):
    """SerialPort::loop + Cli::execute: random bytes (incl. NUL, 0xFF, ESC) are one bad line; next works."""
    cli.write(bytes(range(256)).replace(b"\n", b"") + b"\n$pins status\n")
    cli.c.expect("Pins module enabled", 10)
    cli.settle()
    cli.write(b"\xff\xfe\x00\x01\x1b[A garbage\n")
    cli.settle()
    assert any("Time module enabled" in l for l in cli.run("$time status"))


def test_command_queued_during_long_reply(cli):
    """SerialPort queue: `$system status` sent while `$help` is still printing is not lost."""
    start = len(cli.c.lines)
    cli.c._ser.write(b"$help\n")
    cli.c._ser.flush()
    cli.c.expect(r"Buttons Commands \[buttons\]", 10)  # help is printing now
    cli.c._ser.write(b"$system status\n")
    cli.c._ser.flush()
    cli.c.expect(r"Wifi Commands \[wifi\]", 15)
    cli.c.expect(r"System Status", 15)
    cli.settle()
    assert OVERFLOW not in cli.c.lines[start:]


def test_responsive_after_input_tests(cli):
    """Sanity: the CLI still answers after the abuse above."""
    assert any("System Status" in l for l in cli.run("$system status"))
