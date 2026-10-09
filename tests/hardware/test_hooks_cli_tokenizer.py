"""CLI tokenizer on the board through ``$test echo*`` (fixed arities 0, 1, 2, 3, 5).

Same table as tests/host/test/test_tokenizer.cpp (keep in sync). Each echo prints
``argc=N`` then ``argK len=L hex=HH.. text="..."``; arguments are compared byte for byte.
"""

from __future__ import annotations

import re
import time

import pytest

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # --import-mode=importlib: hooks.py is a sibling
from hooks import hooks  # noqa: F401,E402

ARG_RX = re.compile(r"^arg(\d+) len=(\d+) hex=([0-9A-F]*) text=")

# (line, dispatched, args)
CASES = [
    ("$test echo hello", True, ["hello"]),
    ('$test echo "hello world"', True, ["hello world"]),
    ('$test echo ""', True, [""]),
    ('$test echo2 "" ""', True, ["", ""]),
    ('$test echo "a\\"b"', True, ['a"b']),
    ('$test echo "a\\\\b"', True, ["a\\b"]),
    ('$test echo "tab\\there"', True, ["tabthere"]),
    ('$test echo a"b', True, ['a"b']),
    ('$test echo3 "a"b c', True, ["a", "b", "c"]),
    ('$test echo "héllo \U0001F600"', True, ["héllo \U0001F600"]),
    ("   $test   echo   spaced   ", True, ["spaced"]),
    ("$TEST ECHO Case", True, ["Case"]),
    ("$test\techo\tx", True, ["x"]),
    ("$test echo0", True, []),
    ('$test echo5 1 "2 2" "" 4 5', True, ["1", "2 2", "", "4", "5"]),
    ('$test echo "abc', False, None),
    ('$test echo "abc\\', False, None),
    ("$test echo a b", False, None),
    ("$test echo", False, None),
    ("test echo x", False, None),
]


def echo(hooks, line: str) -> tuple[int | None, list[bytes], list[str]]:
    reply = hooks.run(line)
    argc = None
    args: list[bytes] = []
    for text in reply:
        if text.startswith("argc="):
            argc = int(text[5:])
        m = ARG_RX.match(text)
        if m:
            args.append(bytes.fromhex(m[3]))
            assert int(m[2]) == len(args[-1])
    return argc, args, reply


@pytest.mark.parametrize("line,dispatched,expected", CASES, ids=[c[0].strip()[:28] for c in CASES])
def test_tokenizer(hooks, line, dispatched, expected):
    argc, args, reply = echo(hooks, line)
    if not dispatched:
        assert argc is None, reply
        assert any(line.startswith("Error:") for line in reply), reply
        return
    assert argc == len(expected), reply
    assert args == [a.encode("utf-8") for a in expected]


def test_error_messages(hooks):
    assert any("Unterminated quote" in line for line in hooks.run('$test echo "abc'))
    assert any("expected 1, got 2" in line for line in hooks.run("$test echo a b"))
    assert any("commands must start with '$'" in line for line in hooks.run("test echo x"))


def test_embedded_nul_and_control_bytes(hooks):
    hooks.c._ser.write(b"$test echo a\x00b\x01c\x7f\n")
    hooks.c._ser.flush()
    hooks.c.mark()
    hooks.c.expect(r"^arg0 ", 5)
    line = hooks.c.lines[hooks.c._cursor - 1]
    assert ARG_RX.match(line)[3] == "6100620163" + "7F"


def test_crlf_line_ending(hooks):
    hooks.c._ser.write(b"$test echo crlf\r\n")
    hooks.c._ser.flush()
    hooks.c.mark()
    hooks.c.expect(r"^arg0 len=4 hex=63726C66 ", 5)


def test_longest_line(hooks):
    """254 usable chars per line: an echo argument filling the line arrives intact."""
    prefix = "$test echo "
    arg = "y" * (254 - len(prefix))
    _, args, reply = echo(hooks, prefix + arg)
    assert args == [arg.encode()], reply[-3:]


def test_queue_while_loop_blocked(hooks):
    """Lines sent while a command blocks (sleep_ms) queue up: 4 run in order, more are dropped."""
    hooks.c.send("$test sleep_ms 1500")
    time.sleep(0.3)  # let the firmware take the command line before the rest arrives
    lines = "".join(f"$test echo q{i}\n" for i in range(6)).encode()
    hooks.c._ser.write(lines)
    hooks.c._ser.flush()
    hooks.c.expect(r"^slept_ms=1500$", 5)
    hooks.c.collect(silence=1.0, limit=10)
    got = [bytes.fromhex(ARG_RX.match(l)[3]).decode() for l in hooks.c.lines if ARG_RX.match(l)]
    dropped = sum(1 for l in hooks.c.lines if "Input overflow" in l)
    assert got == ["q0", "q1", "q2", "q3"], hooks.c.lines
    assert dropped == 2


def test_queue_within_limit_all_run(hooks):
    hooks.c.send("$test sleep_ms 1000")
    time.sleep(0.3)
    hooks.c._ser.write(b"$test echo r0\n$test echo r1\n$test echo r2\n")
    hooks.c._ser.flush()
    hooks.c.expect(r"^slept_ms=1000$", 5)
    hooks.c.collect(silence=1.0, limit=10)
    got = [bytes.fromhex(ARG_RX.match(l)[3]).decode() for l in hooks.c.lines if ARG_RX.match(l)]
    assert got == ["r0", "r1", "r2"]
    assert not any("Input overflow" in l for l in hooks.c.lines)
