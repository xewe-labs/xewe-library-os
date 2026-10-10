"""Mixed-traffic soak (``soak.py``) on the session console.

Steady ~1 command/s from every module and the core error paths, a burst of 4 back-to-back lines
every 15th slot, ``$wifi status`` every 30 s, heap/uptime sampled every 20 slots. Duration:
``XEWE_SOAK_SECONDS`` (default 60; the long run is ``soak.py --duration 900`` outside pytest).
"""

import os
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # --import-mode=importlib: soak.py is a sibling
from soak import run_soak  # noqa: E402

DURATION = float(os.environ.get("XEWE_SOAK_SECONDS", "60"))


def test_soak_mixed(cli, log_dir):
    csv_path = None if log_dir is None else log_dir / f"{time.strftime('%H%M')}-soak-mixed.csv"
    res = run_soak(cli.c, DURATION, csv_path=csv_path)
    if log_dir is not None:
        (log_dir / f"{time.strftime('%H%M')}-soak-mixed.log").write_text("\n".join(cli.c.lines) + "\n")
    print(res.summary())
    assert res.ok, res.summary()
    h = res.heap_stats()
    # trend: no steady leak (slope over the run must not lose more than 4 KB per hour)
    if h.get("slope_b_per_min") is not None and DURATION >= 300:
        assert h["slope_b_per_min"] > -4096 / 60, h
    assert all(c for _, c in res.wifi_checks), res.wifi_checks
