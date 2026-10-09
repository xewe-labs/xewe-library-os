"""Shared helpers for the ``$test`` hook tests (wave 2, test_hooks_*.py).

The hooks exist only in firmware built with ``XEWE_TESTING`` (src/XeWeCore/Testing.cpp):

    XEWE_HWTEST_DEFINES=XEWE_TESTING=1 python -m pytest extras/hwtest/test_hooks_*.py ...

or, by hand, ``xewe build --chip s3 --define XEWE_TESTING=1`` then ``xewe flash --no-build``.
Without them every test here skips with a reason naming that command.

Every hook prints ``key=value`` records, one per line; ``Hooks.run`` returns the reply lines
(the console echo of the command removed) and ``Hooks.kv`` merges them into a dict.
"""

from __future__ import annotations

import os
import re
import time
from pathlib import Path

import pytest

from xewe.serialio import Console

NO_HOOKS = (
    "firmware built without the $test hooks; rebuild with XEWE_HWTEST_DEFINES=XEWE_TESTING=1 "
    "(or `xewe build --chip s3 --define XEWE_TESTING=1` + `xewe flash --no-build`)"
)
KV_RX = re.compile(r'(\w+)=("(?:[^"\\]|\\.)*"|\S*)')
QUIET = 0.5


def quote(arg: str) -> str:
    """One CLI argument: wrapped in quotes, with backslash and quote escaped."""
    return '"' + arg.replace("\\", "\\\\").replace('"', '\\"') + '"'


def unquote(value: str) -> str:
    """Inverse of the firmware's quote(): \\" \\\\ and \\xNN."""
    if not (value.startswith('"') and value.endswith('"')):
        return value
    body, out, i = value[1:-1], bytearray(), 0
    while i < len(body):
        c = body[i]
        if c == "\\" and i + 1 < len(body):
            n = body[i + 1]
            if n == "x":
                out.append(int(body[i + 2 : i + 4], 16))
                i += 4
                continue
            out += n.encode()
            i += 2
            continue
        out += c.encode()
        i += 1
    return out.decode("utf-8", errors="replace")


class Hooks:
    def __init__(self, console: Console) -> None:
        self.c = console

    def run(self, cmd: str, quiet: float = QUIET, limit: float = 20, until: str | None = None) -> list[str]:
        """Send ``cmd``; wait for ``until`` (if given), then for ``quiet`` s of silence.

        Returns every line read after the send, the console echo of ``cmd`` removed.
        """
        self.c.send(cmd)
        start = len(self.c.lines)
        if until is not None:
            self.c.expect(until, limit)
        self.c.collect(silence=quiet, limit=limit)
        lines = self.c.lines[start:]
        if lines and lines[0].strip() == cmd.strip():
            lines = lines[1:]
        return lines

    @staticmethod
    def kv(lines: list[str]) -> dict[str, str]:
        out: dict[str, str] = {}
        for line in lines:
            for k, v in KV_RX.findall(line):
                out.setdefault(k, v)
        return out

    def call(self, cmd: str, **kw) -> dict[str, str]:
        return self.kv(self.run(cmd, **kw))

    def heap(self) -> dict[str, int]:
        return {k: int(v) for k, v in self.call("$test heap").items()}

    def alive(self) -> None:
        """The board still answers (no crash/reboot since the last command)."""
        assert "uptime_ms" in self.call("$test uptime"), "board stopped answering"


_present: dict[int, bool] = {}  # id(console) -> hooks present (one probe per session console)


@pytest.fixture
def hooks(serial: Console, request):
    h = Hooks(serial)
    if _present.get(id(serial)) is False:
        pytest.skip(NO_HOOKS)
    reply = h.run("$test uptime")
    if any("Unknown command group 'test'" in line for line in reply):
        _present[id(serial)] = False
        pytest.skip(NO_HOOKS)
    assert any("uptime_ms=" in line for line in reply), f"unexpected reply to $test uptime: {reply}"
    _present[id(serial)] = True
    yield h
    _transcript(request.node.nodeid, serial.lines)


_LOG_STAMP = time.strftime("%H%M")


def _transcript(nodeid: str, lines: list[str]) -> None:
    """Append the test's console lines to $XEWE_HWTEST_LOG_DIR/<HHMM>-wave2-serial.log (if set)."""
    d = os.environ.get("XEWE_HWTEST_LOG_DIR")
    if not d:
        return
    with (Path(d) / f"{_LOG_STAMP}-wave2-serial.log").open("a", encoding="utf-8") as f:
        f.write(f"===== {nodeid}\n")
        f.writelines(line + "\n" for line in lines)


def elapsed(fn) -> tuple[object, float]:
    t0 = time.monotonic()
    r = fn()
    return r, time.monotonic() - t0
