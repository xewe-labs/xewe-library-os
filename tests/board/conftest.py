"""Helpers for the XeWeCore black-box board tests (wave 1).

The tools plugin provides the session console as the ``serial`` fixture. This conftest adds a
``cli`` fixture wrapping it with the few operations every test file needs: run a command and
collect its reply, write raw bytes, read heap/uptime, restart and wait for the banner, and clean
the button/schedule tables the tests touch.
"""

from __future__ import annotations

import csv
import os
import re
import time
from pathlib import Path

import pytest

from xewe.serialio import BOOT_READY, Console, wait_for_banner

BOOT_TIMEOUT = 90.0
QUIET = 0.6
TEST_PIN = int(os.environ.get("XEWE_TEST_BUTTONS_PIN", "4"))

MEM_RX = re.compile(r"Memory Usage:\s+[\d.]+% \((\d+) / (\d+) bytes\)")
UPTIME_RX = re.compile(r"Uptime:\s+(\d+)d (\d\d):(\d\d):(\d\d)")
BUTTON_ROW_RX = re.compile(r"^\|\s*(\d+)\s*\|\s*(\d+)\s*\|")
SCHEDULE_ID_RX = re.compile(r'"id"\s*:\s*(\d+)')


class Cli:
    def __init__(self, console: Console) -> None:
        self.c = console

    # -------------------------------------------------------------- I/O
    def run(self, cmd: str, quiet: float = QUIET, limit: float = 20) -> list[str]:
        """Send ``cmd``, read until ``quiet`` s of silence; return the reply without the echo."""
        self.c.send(cmd)
        lines = self.c.collect(silence=quiet, limit=limit)
        if lines and lines[0].strip() == cmd.strip():
            lines = lines[1:]
        return lines

    def write(self, data: bytes) -> None:
        """Write raw bytes in one call (one USB transfer for short data); moves the cursor."""
        self.c._ser.write(data)
        self.c._ser.flush()
        self.c.mark()

    def settle(self, quiet: float = QUIET) -> list[str]:
        return self.c.collect(silence=quiet)

    # -------------------------------------------------------------- probes
    def mem(self) -> tuple[int, int, int]:
        """(free_bytes, total_bytes, uptime_s) from ``$web_interface status``."""
        self.c.send("$web_interface status")
        m = self.c.expect(MEM_RX.pattern, 10)
        used, total = int(m[1]), int(m[2])
        self.settle(0.3)
        up = None
        for line in self.c.lines[-8:]:
            u = UPTIME_RX.search(line)
            if u:
                d, h, mi, s = map(int, u.groups())
                up = ((d * 24 + h) * 60 + mi) * 60 + s
        assert up is not None, "no Uptime line in $web_interface status"
        return total - used, total, up

    def restart(self) -> list[str]:
        """``$system restart``; wait for ``System Setup Complete`` across the USB re-enumeration."""
        self.c.send("$system restart")
        self.c.expect(r"Rebooting", 10)
        t0 = time.monotonic()
        wait_for_banner(self.c, BOOT_READY, BOOT_TIMEOUT, reset=False)
        self.boot_seconds = time.monotonic() - t0
        # a native-USB reopen can reset the chip once more: wait until the output is quiet
        tail = self.c.collect(silence=2.0, limit=30)
        if any(BOOT_READY in line for line in tail) is False and any("rst:" in l for l in tail):
            wait_for_banner(self.c, BOOT_READY, BOOT_TIMEOUT, reset=False)
            self.c.collect(silence=2.0, limit=30)
        return tail

    # -------------------------------------------------------------- tables
    def button_ids(self) -> list[int]:
        return [int(m[1]) for line in self.run("$buttons status") if (m := BUTTON_ROW_RX.match(line.strip()))]

    def schedule_ids(self) -> list[int]:
        out = "\n".join(self.run("$schedule status"))
        return [int(x) for x in SCHEDULE_ID_RX.findall(out)]

    def clear_buttons(self) -> None:
        for i in sorted(self.button_ids(), reverse=True):
            self.run(f"$buttons remove {i}")
        assert self.button_ids() == []

    def clear_schedules(self) -> None:
        for i in sorted(self.schedule_ids(), reverse=True):
            self.run(f"$schedule remove {i}")
        assert self.schedule_ids() == []


@pytest.fixture
def cli(serial: Console) -> Cli:
    return Cli(serial)


@pytest.fixture(scope="session")
def log_dir() -> Path | None:
    """``XEWE_HWTEST_LOG_DIR``: where heap/uptime CSVs go (unset: not written)."""
    d = os.environ.get("XEWE_HWTEST_LOG_DIR")
    if not d:
        return None
    p = Path(d)
    p.mkdir(parents=True, exist_ok=True)
    return p


def write_csv(log_dir: Path | None, name: str, header: list[str], rows: list[list]) -> None:
    if log_dir is None:
        return
    stamp = time.strftime("%H%M")
    with (log_dir / f"{stamp}-{name}.csv").open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(header)
        w.writerows(rows)


@pytest.fixture
def csv_writer(log_dir):
    return lambda name, header, rows: write_csv(log_dir, name, header, rows)


# ---------------------------------------------------------------------- wave 2: build defines
DEFINES_ENV = "XEWE_HWTEST_DEFINES"


@pytest.fixture(scope="session")
def compiled(xewe):
    """The plugin's ``compiled``, plus ``--define`` values from ``$XEWE_HWTEST_DEFINES``.

    Space-separated ``KEY=VALUE`` items, e.g. ``XEWE_HWTEST_DEFINES=XEWE_TESTING=1`` builds the
    ``$test`` hooks in (test_hooks_*.py). Unset: identical to the plugin's fixture.
    """
    from xewe import build
    from xewe.report import XeweError

    p = xewe.project()
    try:
        defines = build.parse_defines(os.environ.get(DEFINES_ENV, "").split())
        res = build.build_chip(p, xewe.lock, xewe.chip, defines)
    except XeweError as exc:
        pytest.fail(f"build failed for {xewe.chip}: {exc}", pytrace=False)
    if not res.ok or res.binary is None:
        pytest.fail(f"build failed for {xewe.chip}; see {p.rel(p.out_dir(xewe.chip) / 'compile.log')}", pytrace=False)
    return res.binary
