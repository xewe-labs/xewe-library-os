#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
# SPDX-License-Identifier: GPL-3.0-only
"""XeWeCore soak: mixed CLI traffic at a steady rate with periodic bursts, heap/uptime sampling.

Runs standalone (outside pytest) against a provisioned board, or is imported by
``test_soak_*.py`` (``run_soak`` on the session console). Run with the harness venv:

    flock <hw-s3>/.board.lock -c '<hw-s3>/build/tools/.venv/bin/python tests/hardware/soak.py \
        --port /dev/ttyACM0 --duration 900 --csv <logs>/HHMM-soak.csv --log <logs>/HHMM-soak.log'

Every command has an expected-reply regex; a command whose reply does not arrive within its timeout
is "unanswered". Every ``--sample-every`` commands the heap and uptime are sampled
(``$test heap`` when the firmware has the XEWE_TESTING group, plus ``$web_interface status``
for uptime and ESP.getFreeHeap()). ``$wifi status`` is sent every ``--wifi-every`` seconds.

Exit status: 0 = all answered, no reboot, heap drop <= --max-heap-drop; 1 = any of those failed;
2 = board did not boot / port error before the soak started.
"""

from __future__ import annotations

import argparse
import csv
import dataclasses
import random
import re
import sys
import time
from pathlib import Path
from typing import Any, Callable

from xewe.report import XeweError
from xewe.serialio import BOOT_READY, BOOT_UNPROVISIONED, Console, ExpectTimeout, wait_for_banner

# ---------------------------------------------------------------- command mix
# (command, reply regex, timeout s). Read-only or error-path commands only: nothing here writes
# NVS, reboots, or changes Wi-Fi/pin state.
CORE_COMMANDS: list[tuple[str, str, float]] = [
    ("$system status", r"System Status", 5),
    ("$system info", r"^MAC [0-9A-F:]{17}", 5),
    ("$system mac", r"^wifi_sta [0-9A-F:]{17}", 5),
    ("$system uid", r"^uid64 [0-9a-fA-F]{16}", 5),
    ("$help system", r"System Commands \[system\]", 5),
    ("$SYSTEM Uid", r"^uid64 ", 5),                                         # case-insensitive lookup
    ("$nosuch cmd", r"Error: Unknown command group 'nosuch'", 5),
    ("$system nosuch", r"Error: Unknown command 'nosuch' in command group 'system'", 5),
    ("$system restart now", r"Error: Argument count mismatch for '\$system restart'", 5),
    ("hello", r"Error: commands must start with '\$'", 5),
    ('$system "unterminated', r"Error: Unterminated quote", 5),
    ("$", r"Error: Missing command group", 5),
]
MODULE_COMMANDS: list[tuple[str, str, float]] = [
    ("$wifi status", r"^Wifi module enabled", 5),
    ("$time status", r"^Time module enabled", 5),
    ("$schedule status", r"^Scheduler module enabled", 5),
    ("$pins status", r"^Pins module enabled", 5),
    ("$buttons status", r"^Buttons module enabled", 5),
    ("$web_interface status", r"^Web Interface module enabled", 5),
    ("$time set_zone bogus", r"^Invalid format\. Use GMT", 5),
    ("$schedule remove 999", r"^Scheduler: invalid ID or out of bounds", 5),
    ("$schedule remove 250", r"^Scheduler: schedule not found", 5),
    ("$buttons remove abc", r"^Error: invalid button ID", 5),
    ("$buttons remove 4242", r"^Error: button ID not found", 5),
]
# rare and slow: network round trips
SLOW_COMMANDS: list[tuple[str, str, float]] = [
    ("$help", r"Wifi Commands \[wifi\]", 10),                              # every group's table
    ("$time fetch", r"Current time: |Unable to reach time server", 25),
]
SLOW_EVERY = 60
"""A slow command replaces the normal pick every this many commands."""

BOOT_LINE = re.compile(r"^ESP-ROM:|^rst:0x|System Setup Complete|\|\s+Rebooting\s+\||Name your device")
UPTIME_RE = re.compile(r"^\s*- Uptime:\s+(\d+)d (\d+):(\d+):(\d+)")
MEMORY_RE = re.compile(r"^\s*- Memory Usage: [\d.]+% \((\d+) / (\d+) bytes\)")
WIFI_CONNECTED_RE = re.compile(r"^Connected to ")
TEST_HEAP_RE = re.compile(r"^free=(\d+) min_free=(\d+) largest=(\d+)")   # Testing.cpp `$test heap`
TEST_UPTIME_RE = re.compile(r"^uptime_ms=(\d+)")                          # Testing.cpp `$test uptime`


@dataclasses.dataclass
class Sample:
    t_s: float
    commands_sent: int
    uptime_s: int | None
    uptime_ms: int | None
    free: int | None
    min_free: int | None
    largest: int | None
    total: int | None
    source: str


@dataclasses.dataclass
class SoakResult:
    duration_s: float = 0.0
    sent: int = 0
    answered: int = 0
    unanswered: list[tuple[int, str]] = dataclasses.field(default_factory=list)
    reboots: list[str] = dataclasses.field(default_factory=list)
    samples: list[Sample] = dataclasses.field(default_factory=list)
    wifi_checks: list[tuple[float, bool]] = dataclasses.field(default_factory=list)
    bursts: int = 0
    max_reply_s: float = 0.0
    reasons: list[str] = dataclasses.field(default_factory=list)

    @property
    def ok(self) -> bool:
        return not self.reasons

    def heap_stats(self) -> dict[str, Any]:
        free = [s.free for s in self.samples if s.free is not None]
        mins = [s.min_free for s in self.samples if s.min_free is not None]
        out: dict[str, Any] = {"n": len(free)}
        if free:
            out.update(start=free[0], end=free[-1], min=min(free), max=max(free),
                       drop_from_start=free[0] - min(free), slope_b_per_min=_slope(self.samples))
        if mins:
            out["min_free_ever"] = min(mins)
        return out

    def summary(self) -> str:
        h = self.heap_stats()
        up = [s.uptime_s for s in self.samples if s.uptime_s is not None]
        wifi_ok = sum(1 for _, c in self.wifi_checks if c)
        lines = [
            f"soak: {self.duration_s:.0f} s, sent {self.sent}, answered {self.answered}, "
            f"unanswered {len(self.unanswered)}, bursts {self.bursts}, reboots {len(self.reboots)}, "
            f"max reply {self.max_reply_s:.2f} s",
            f"heap: {h}",
            f"uptime: first {up[0] if up else None} s, last {up[-1] if up else None} s, samples {len(up)}",
            f"wifi: {wifi_ok}/{len(self.wifi_checks)} checks connected",
            "result: " + ("PASS" if self.ok else "FAIL: " + "; ".join(self.reasons)),
        ]
        return "\n".join(lines)


def _slope(samples: list[Sample]) -> float | None:
    pts = [(s.t_s, s.free) for s in samples if s.free is not None]
    if len(pts) < 3:
        return None
    n = len(pts)
    mx = sum(p[0] for p in pts) / n
    my = sum(p[1] for p in pts) / n
    den = sum((p[0] - mx) ** 2 for p in pts)
    if den == 0:
        return None
    return 60.0 * sum((p[0] - mx) * (p[1] - my) for p in pts) / den


class Soaker:
    def __init__(self, console: Console, say: Callable[[str], None] = print, seed: int = 20261008) -> None:
        self.c = console
        self.say = say
        self.rng = random.Random(seed)
        self.res = SoakResult()
        self._scan_from = len(console.lines)
        self.has_test_heap = False

    # ------------------------------------------------------------ stream checks
    def scan_for_boot(self) -> None:
        """Record every boot/reboot marker line printed since the last scan."""
        lines = self.c.lines
        for i in range(self._scan_from, len(lines)):
            if BOOT_LINE.search(lines[i]):
                self.res.reboots.append(f"line {i}: {lines[i]!r}")
        self._scan_from = len(lines)

    def _expect(self, pattern: str, timeout: float) -> re.Match[str] | None:
        try:
            return self.c.expect(pattern, timeout)
        except ExpectTimeout:
            return None
        except XeweError as exc:  # port vanished: the board rebooted
            self.res.reboots.append(f"port lost: {exc}")
            try:
                wait_for_banner(self.c, f"{BOOT_READY}|{BOOT_UNPROVISIONED}", 90, reset=False)
            except (ExpectTimeout, XeweError):
                pass
            return None

    # ------------------------------------------------------------ commands
    def one(self, cmd: str, pattern: str, timeout: float) -> re.Match[str] | None:
        t0 = time.monotonic()
        self.c.send(cmd)
        self.res.sent += 1
        m = self._expect(pattern, timeout)
        self._account(cmd, m, time.monotonic() - t0)
        return m

    def _account(self, cmd: str, m: re.Match[str] | None, dt: float) -> None:
        if m is None:
            self.res.unanswered.append((self.res.sent, cmd))
            self.say(f"UNANSWERED #{self.res.sent}: {cmd!r}")
        else:
            self.res.answered += 1
            self.res.max_reply_s = max(self.res.max_reply_s, dt)

    def burst(self, cmds: list[tuple[str, str, float]]) -> None:
        """Send up to 4 commands back to back (one write), then match the replies in order."""
        assert len(cmds) <= 4, "the firmware queue holds 4 lines"
        t0 = time.monotonic()
        payload = "".join(c + "\n" for c, _, _ in cmds)
        self.c._ser.write(payload.encode())
        self.c._ser.flush()
        self.c.mark()
        self.c._emit("> [burst] " + " | ".join(c for c, _, _ in cmds))
        self.res.bursts += 1
        for cmd, pattern, timeout in cmds:
            self.res.sent += 1
            m = self._expect(pattern, timeout)
            self._account(cmd, m, time.monotonic() - t0)

    def pick(self, slot: int) -> tuple[str, str, float]:
        if slot % SLOW_EVERY == 0:
            return SLOW_COMMANDS[(slot // SLOW_EVERY) % len(SLOW_COMMANDS)]
        pool = CORE_COMMANDS if self.rng.random() < 0.5 else MODULE_COMMANDS
        return self.rng.choice(pool)

    # ------------------------------------------------------------ sampling
    def probe_test_group(self) -> None:
        self.c.send("$test heap")
        m = self._expect(r"Error: Unknown command group 'test'|Error: Unknown command 'heap'|" + TEST_HEAP_RE.pattern, 5)
        self.has_test_heap = bool(m) and not m.group(0).startswith("Error")
        self.say(f"$test heap available: {self.has_test_heap}")
        self.c.collect(silence=0.3, limit=2)

    def sample(self, t_start: float) -> Sample:
        s = Sample(round(time.monotonic() - t_start, 1), self.res.sent, None, None, None, None, None, None, "")
        if self.has_test_heap:
            if (m := self.one("$test heap", TEST_HEAP_RE.pattern, 5)):
                s.free, s.min_free, s.largest = int(m[1]), int(m[2]), int(m[3])
                s.source = "test"
            if (m := self.one("$test uptime", TEST_UPTIME_RE.pattern, 5)):
                s.uptime_ms = int(m[1])
        mu = self.one("$web_interface status", UPTIME_RE.pattern, 5)
        if mu:
            s.uptime_s = int(mu[1]) * 86400 + int(mu[2]) * 3600 + int(mu[3]) * 60 + int(mu[4])
            mm = self._expect(MEMORY_RE.pattern, 3)
            if mm:
                s.total = int(mm[2])
                if s.free is None:
                    s.free = s.total - int(mm[1])
                    s.source = "web_interface"
        self.res.samples.append(s)
        return s

    def wifi_check(self, t_start: float) -> None:
        m = self.one("$wifi status", r"^Connected to |^disconnected", 5)
        connected = bool(m) and m.group(0).startswith("Connected")
        self.res.wifi_checks.append((round(time.monotonic() - t_start, 1), connected))


def run_soak(
    console: Console,
    duration: float,
    rate: float = 1.0,
    burst_every: int = 15,
    sample_every: int = 20,
    wifi_every: float = 30.0,
    max_heap_drop: int = 8192,
    csv_path: Path | None = None,
    say: Callable[[str], None] = print,
    seed: int = 20261008,
) -> SoakResult:
    """Soak an already booted board on an open console; returns the result (never raises on a miss)."""
    s = Soaker(console, say, seed)
    console.collect(silence=0.5, limit=3)
    s._scan_from = len(console.lines)
    s.probe_test_group()
    t_start = time.monotonic()
    writer = None
    fh = None
    if csv_path is not None:
        csv_path.parent.mkdir(parents=True, exist_ok=True)
        fh = csv_path.open("w", newline="")
        writer = csv.writer(fh)
        writer.writerow(["t_s", "commands_sent", "uptime_s", "uptime_ms", "free", "min_free", "largest", "total", "source"])

    def take_sample() -> None:
        smp = s.sample(t_start)
        if writer:
            writer.writerow(dataclasses.astuple(smp))
            fh.flush()  # type: ignore[union-attr]
        say(f"[{smp.t_s:7.1f}s] sent {s.res.sent} uptime {smp.uptime_s} free {smp.free} min {smp.min_free}")

    try:
        take_sample()
        last_wifi = -1e9
        period = 1.0 / rate if rate > 0 else 0.0
        n = 0
        while time.monotonic() - t_start < duration:
            t_cmd = time.monotonic()
            n += 1
            if t_cmd - last_wifi >= wifi_every:
                last_wifi = t_cmd
                s.wifi_check(t_start)
            elif burst_every and n % burst_every == 0:
                pool = CORE_COMMANDS[:6] + MODULE_COMMANDS
                s.burst([s.rng.choice(pool) for _ in range(4)])
            else:
                s.one(*s.pick(n))
            if sample_every and n % sample_every == 0:
                take_sample()
            s.scan_for_boot()
            sleep = period - (time.monotonic() - t_cmd)
            if sleep > 0:
                console.collect(silence=sleep, limit=sleep)  # keep reading while idle
        take_sample()
        console.collect(silence=0.5, limit=3)
        s.scan_for_boot()
    finally:
        if fh:
            fh.close()

    r = s.res
    r.duration_s = time.monotonic() - t_start
    if r.unanswered:
        r.reasons.append(f"{len(r.unanswered)} unanswered: {r.unanswered[:5]}")
    if r.reboots:
        r.reasons.append(f"reboot detected: {r.reboots[:3]}")
    ups = [x.uptime_ms for x in r.samples if x.uptime_ms is not None] or \
        [x.uptime_s for x in r.samples if x.uptime_s is not None]
    if any(b < a for a, b in zip(ups, ups[1:])):
        r.reasons.append(f"uptime not monotonic: {ups}")
    h = r.heap_stats()
    if h.get("n", 0) and h["drop_from_start"] > max_heap_drop:
        r.reasons.append(f"heap dropped {h['drop_from_start']} B from start (> {max_heap_drop})")
    if not h.get("n"):
        r.reasons.append("no heap sample")
    return r


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--duration", type=float, required=True, help="seconds of traffic")
    ap.add_argument("--rate", type=float, default=1.0, help="commands per second (steady part)")
    ap.add_argument("--burst-every", type=int, default=15, help="every Nth slot is a burst of 4")
    ap.add_argument("--sample-every", type=int, default=20, help="heap/uptime sample every N slots")
    ap.add_argument("--wifi-every", type=float, default=30.0, help="$wifi status every S seconds")
    ap.add_argument("--max-heap-drop", type=int, default=8192)
    ap.add_argument("--csv", type=Path, help="heap/uptime CSV")
    ap.add_argument("--log", type=Path, help="timestamped transcript of every line")
    ap.add_argument("--boot-timeout", type=float, default=90.0)
    ap.add_argument("--seed", type=int, default=20261008)
    a = ap.parse_args(argv)

    console = Console(a.port, log_path=a.log)
    try:
        console.open()
    except Exception as exc:  # noqa: BLE001
        print(f"cannot open {a.port}: {exc}", file=sys.stderr)
        return 2
    with console:
        # opening a native-USB S3 resets it: wait for that boot (or force one if it stayed silent)
        try:
            m = wait_for_banner(console, f"{BOOT_READY}|{BOOT_UNPROVISIONED}", a.boot_timeout, reset=False)
        except (ExpectTimeout, XeweError):
            try:
                m = wait_for_banner(console, f"{BOOT_READY}|{BOOT_UNPROVISIONED}", a.boot_timeout, reset=True)
            except (ExpectTimeout, XeweError) as exc:
                print(f"board did not boot: {exc}", file=sys.stderr)
                return 2
        # native USB: the port reopen after a reset can reset the chip once more; wait that out
        while m.group(0) == BOOT_READY:
            tail = console.collect(silence=2.0, limit=30)
            rst = [i for i, line in enumerate(tail) if line.startswith("rst:")]
            if not rst or any(BOOT_READY in line for line in tail[rst[-1]:]):
                break
            m = wait_for_banner(console, f"{BOOT_READY}|{BOOT_UNPROVISIONED}", a.boot_timeout, reset=False)
        if m.group(0) == BOOT_UNPROVISIONED:
            print("board is unprovisioned (Name your device); provision it first", file=sys.stderr)
            return 2
        res = run_soak(console, a.duration, a.rate, a.burst_every, a.sample_every, a.wifi_every,
                       a.max_heap_drop, a.csv, seed=a.seed)
    print(res.summary())
    return 0 if res.ok else 1


if __name__ == "__main__":
    sys.exit(main())
