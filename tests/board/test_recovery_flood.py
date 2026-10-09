"""Wave 3: input flood against the 4-line queue (``SerialPort::loop``/``push_line``, core 2.0.1).

Documented behaviour (Serial.h/Serial.cpp): completed lines go to a FIFO of 4; when it is full
the *newest* line is dropped and ``! Input overflow: line dropped`` is printed once per dropped
line; a line longer than 254 chars is
dropped whole at its newline with ``! Input line too long (max 254 chars): dropped`` (core fix B).
"""

import random
import re
import string
import time

OVERFLOW = "! Input overflow: line dropped"
TOO_LONG = "! Input line too long (max 254 chars): dropped"
NOT_CMD = "Error: commands must start with '$'"
UID = r"^uid64 [0-9a-fA-F]{16}"


def _dump(cli, log_dir, name):
    if log_dir is not None:
        (log_dir / f"{time.strftime('%H%M')}-{name}.log").write_text("\n".join(cli.c.lines) + "\n")


def test_flood_20_lines_one_write(cli, log_dir):
    """20 distinct lines in one write: the first 4 run in order, every other line is either run
    (in order) or reported dropped; replies + overflow notices == 20; the CLI answers afterwards."""
    start = len(cli.c.lines)
    cli.write("".join(f"$flood{i:02d} x\n" for i in range(20)).encode())
    time.sleep(3)
    out = cli.c.lines[start:] + cli.settle(1.0)
    text = "\n".join(out)
    ran = [int(n) for n in re.findall(r"Error: Unknown command group 'flood(\d\d)'", text)]
    dropped = text.count(OVERFLOW)
    print(f"flood: ran {ran}, overflow notices {dropped}")
    _dump(cli, log_dir, "flood20")
    assert ran == sorted(set(ran)), f"replies out of order or duplicated: {ran}"
    assert ran[:4] == [0, 1, 2, 3], f"the 4 oldest lines must survive: {ran}"
    assert len(ran) + dropped == 20, f"ran {len(ran)} + dropped {dropped} != 20"
    assert dropped >= 1, "20 lines in one write never overflowed the 4-line queue"
    cli.c.send("$system uid")
    cli.c.expect(UID, 10)


def test_flood_repeated_bursts_stay_consistent(cli, log_dir):
    """Five 20-line floods back to back (100 lines): accounting holds for every burst."""
    totals = []
    for b in range(5):
        start = len(cli.c.lines)
        cli.write("".join(f"$fl{b}{i:02d} x\n" for i in range(20)).encode())
        time.sleep(2)
        text = "\n".join(cli.c.lines[start:] + cli.settle(0.8))
        ran = re.findall(rf"Error: Unknown command group 'fl{b}(\d\d)'", text)
        dropped = text.count(OVERFLOW)
        totals.append((len(ran), dropped))
        assert len(ran) + dropped == 20, (b, ran, dropped)
    print(f"bursts (ran, dropped): {totals}")
    _dump(cli, log_dir, "flood5x20")
    cli.c.send("$system uid")
    cli.c.expect(UID, 10)


def test_1k_printable_garbage_then_newline_then_command(cli, log_dir):
    """1024 random printable chars (no '$', no quote, no newline), then a newline, then a valid
    command: the over-long line is dropped whole (core fix B: one ``! Input line too long``
    message, nothing of it executes, no split lines) and the valid command runs."""
    rng = random.Random(1008)
    alphabet = "".join(ch for ch in string.printable if ch not in "$\"\\\r\n\t\x0b\x0c")
    junk = "".join(rng.choice(alphabet) for _ in range(1024))
    start = len(cli.c.lines)
    cli.write(junk.encode())
    time.sleep(1.0)
    cli.write(b"\n")
    time.sleep(1.0)
    cli.c.send("$system uid")
    m = cli.c.expect(UID, 10)
    text = "\n".join(cli.c.lines[start:] + cli.settle(0.8))
    too_long, rejected, dropped = text.count(TOO_LONG), text.count(NOT_CMD), text.count(OVERFLOW)
    print(f"1k garbage: too long {too_long}, rejected {rejected}, overflow {dropped}; command: {m[0]}")
    _dump(cli, log_dir, "garbage1k")
    assert (too_long, rejected, dropped) == (1, 0, 0)
    # garbage, newline and command in a single write: same result, the command still runs
    start = len(cli.c.lines)
    cli.write(junk.encode() + b"\n$system uid\n")
    cli.c.expect(UID, 10)
    text = "\n".join(cli.c.lines[start:] + cli.settle(0.8))
    too_long, rejected, dropped = text.count(TOO_LONG), text.count(NOT_CMD), text.count(OVERFLOW)
    print(f"1k garbage + command in one write: too long {too_long}, rejected {rejected}, overflow {dropped}")
    _dump(cli, log_dir, "garbage1k-onewrite")
    assert (too_long, rejected, dropped) == (1, 0, 0)


def test_254_chars_is_a_line_255_is_dropped(cli):
    """Boundary: a 254-char line still executes; 255 chars is dropped whole."""
    cmd = "$system uid"
    ok = cmd + " " * (254 - len(cmd))           # trailing spaces are trimmed by the CLI
    cli.write(ok.encode() + b"\n")
    cli.c.expect(UID, 10)
    start = len(cli.c.lines)
    cli.write((ok + " ").encode() + b"\n")
    time.sleep(1.5)
    text = "\n".join(cli.c.lines[start:] + cli.settle(0.8))
    assert text.count(TOO_LONG) == 1 and not re.search(UID, text, re.M), text


def test_responsive_after_flood(cli):
    """Sanity: status table still renders in full after the floods."""
    out = cli.run("$system status")
    assert any("System Status" in l for l in out), out
    assert any("Web Interface" in l for l in out), out
