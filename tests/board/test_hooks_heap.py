"""Heap probe (``$test heap``) and a leak check across every hook family."""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # --import-mode=importlib: hooks.py is a sibling
from hooks import hooks  # noqa: F401,E402


def test_heap_probe_sane(hooks):
    h = hooks.heap()
    assert set(h) == {"free", "min_free", "largest"}
    assert 0 < h["min_free"] <= h["free"]
    assert 0 < h["largest"] <= h["free"]
    assert h["free"] > 50_000


def test_uptime_monotonic(hooks):
    a = int(hooks.call("$test uptime")["uptime_ms"])
    hooks.call("$test sleep_ms 200")
    b = int(hooks.call("$test uptime")["uptime_ms"])
    assert b - a >= 200


def test_sleep_bounds(hooks):
    assert any("error=" in l for l in hooks.run("$test sleep_ms 10001"))
    assert any("error=" in l for l in hooks.run("$test sleep_ms -1"))


def test_no_leak_over_mixed_hooks(hooks, csv_writer):
    cmds = [
        "$test echo leak",
        '$test flex "{\\"s\\":\\"abc\\",\\"v\\":[1,2,3]}"',
        "$test flex_bad longstr",
        "$test flex_bad blob_veccount",
        "$test validate gmt GMT+5:30",
        "$test nvs xtheap k str value",
        "$test nvs_read xtheap k str",
        "$test nvs_del xtheap k",
    ]
    for c in cmds:  # warm-up: first-use allocations (NVS handles, JSON pools)
        hooks.run(c)
    rows = []
    base = hooks.heap()
    rows.append([0, base["free"], base["min_free"], base["largest"]])
    for rnd in range(1, 6):
        for _ in range(4):
            for c in cmds:
                hooks.run(c, quiet=0.15)
        h = hooks.heap()
        rows.append([rnd * 4 * len(cmds), h["free"], h["min_free"], h["largest"]])
    hooks.call("$test nvs_reset xtheap")
    csv_writer("wave2-heap-hooks", ["commands", "free", "min_free", "largest"], rows)
    print("heap rows:", rows)
    assert rows[-1][1] >= base["free"] - 2048, rows
