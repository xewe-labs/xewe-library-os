"""The one destructive test. Factory reset (`$system reset` -> ``System::reset``: every
module's ``reset``, then ``Nvs::erase_all``, then restart) must bring the board back to first boot
(``Name your device``), and ``xewe provision`` must bring it back to a fully working state.

Opt-in only: runs when ``XEWE_HWTEST_DESTRUCTIVE=1``. If re-provisioning fails twice the board is
left UNPROVISIONED and the test fails saying so.
"""

import os
import re
import subprocess
import sys
import time
from pathlib import Path

import pytest

from xewe.board.serialio import BOOT_READY, BOOT_UNPROVISIONED, wait_for_banner

BOOT_TIMEOUT = 90.0
MODULES = ["System", "Buttons", "Pins", "Wifi", "Time", "Scheduler", "Web Interface"]

pytestmark = pytest.mark.skipif(os.environ.get("XEWE_HWTEST_DESTRUCTIVE") != "1",
                                reason="destructive (erases NVS, re-provisions); set XEWE_HWTEST_DESTRUCTIVE=1")


def _provision(project: Path, port: str, log: Path) -> tuple[int, str]:
    cmd = [sys.executable, "-m", "xewe", "provision", "--port", port, "--log", str(log)]
    p = subprocess.run(cmd, cwd=project, capture_output=True, text=True, timeout=600)
    out = p.stdout + p.stderr
    log.with_suffix(".stdout.txt").write_text(out)
    return p.returncode, out


def test_factory_reset_then_reprovision(cli, xewe, log_dir, tmp_path):
    c = cli.c
    logs = log_dir or tmp_path
    stamp = time.strftime("%H%M")
    # 1. abort path first: `n` keeps everything
    c.send("$system reset")
    c.expect(r"Resetting System", 10)
    c.expect(r"^\(y/n\) >", 10)
    c.send("n")
    c.expect(r"Aborted", 10)
    cli.settle()
    assert any(re.match(r"uid64 ", l) for l in cli.run("$system uid"))

    # 1b. no answer: the prompt cancels itself after 2 attempts x 15 s (~30 s)
    t_to = time.monotonic()
    c.send("$system reset")
    c.expect(r"^\(y/n\) >", 10)
    c.expect(r"No answer: reset cancelled", 45)
    waited = time.monotonic() - t_to
    c.expect(r"Aborted", 5)
    print(f"reset prompt timed out after {waited:.1f} s")
    assert 28 <= waited <= 34, waited
    cli.settle()
    assert any(re.match(r"uid64 ", l) for l in cli.run("$system uid"))

    # 2. confirm: wipe and reboot into first boot
    start = len(c.lines)
    c.send("$system reset")
    c.expect(r"^\(y/n\) >", 10)
    t0 = time.monotonic()
    c.send("y")
    c.expect(r"Rebooting", 30)
    m = wait_for_banner(c, f"{BOOT_READY}|{BOOT_UNPROVISIONED}", BOOT_TIMEOUT, reset=False)
    wipe_to_prompt = time.monotonic() - t0
    (logs / f"{stamp}-nvs-wipe.log").write_text("\n".join(c.lines[start:]) + "\n")
    print(f"wipe -> {m[0]!r} after {wipe_to_prompt:.1f} s")
    assert m[0] == BOOT_UNPROVISIONED, "after the factory reset the board did not ask for its name"
    reset_lines = "\n".join(c.lines[start:])
    for name in MODULES[1:]:
        assert f"{name} module reset" in reset_lines, f"{name} was not reset"

    # 3. re-provision with the tools (they own the port meanwhile); one retry
    port = c.port
    c.close()
    project = xewe.project().root
    attempts = []
    for attempt in (1, 2):
        t1 = time.monotonic()
        rc, out = _provision(project, port, logs / f"{time.strftime('%H%M')}-reprovision-{attempt}.log")
        attempts.append((rc, round(time.monotonic() - t1, 1), "Unable to join" in out))
        print(f"provision attempt {attempt}: exit {rc} in {attempts[-1][1]} s; tail:\n" + "\n".join(out.splitlines()[-3:]))
        if rc == 0:
            break
    assert attempts[-1][0] == 0, f"RE-PROVISIONING FAILED TWICE, BOARD IS UNPROVISIONED: {attempts}"

    # 4. reopen (resets the board) and check the full system is back
    c.open()
    m = wait_for_banner(c, f"{BOOT_READY}|{BOOT_UNPROVISIONED}", BOOT_TIMEOUT, reset=False)
    assert m[0] == BOOT_READY, "board still unprovisioned after xewe provision"
    c.collect(silence=2.0, limit=30)
    if any(l.startswith("rst:") for l in c.lines[-40:]) and not any(BOOT_READY in l for l in c.lines[-5:]):
        wait_for_banner(c, BOOT_READY, BOOT_TIMEOUT, reset=False)
        c.collect(silence=2.0, limit=30)
    out = [l.strip() for l in cli.run("$system status")]
    rows = {m[1]: (m[2], m[3]) for l in out
            if (m := re.fullmatch(r"\|\s*([^|\s][^|]*?)\s*\|\s*(Yes|No)\s*\|\s*(.*?)\s*\|", l))}
    print(f"status after re-provisioning: {rows}")
    assert list(rows) == MODULES, rows
    assert all(v[0] == "Yes" for v in rows.values()), rows
    wifi = "\n".join(cli.run("$wifi status"))
    assert re.search(r"^Connected to ", wifi, re.M), wifi
    (logs / f"{time.strftime('%H%M')}-post-reprovision.log").write_text("\n".join(c.lines[start:]) + "\n")
