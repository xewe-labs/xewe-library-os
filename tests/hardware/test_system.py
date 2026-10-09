"""Black-box tests of the System module (``System::status``, ``System::restart``, generic commands).

``$system status`` prints a table of modules only; heap and uptime come from
``$web_interface status`` (``Memory Usage`` / ``Uptime``), the only commands that print them.
"""

import re
import time

MODULES = ["System", "Buttons", "Pins", "Wifi", "Time", "Scheduler", "Web Interface"]


def test_status_table(cli):
    """System::status: header, column titles, one row per registered module with Yes/No and status."""
    out = [l.strip() for l in cli.run("$system status")]
    assert any(re.fullmatch(r"\|\s+System Status\s+\|", l) for l in out), out
    assert any(re.fullmatch(r"\|\s*Module Name\s*\|\s*Enabled\s*\|\s*Status\s*\|", l) for l in out)
    rows = {m[1]: (m[2], m[3]) for l in out
            if (m := re.fullmatch(r"\|\s*([^|\s][^|]*?)\s*\|\s*(Yes|No)\s*\|\s*(.*?)\s*\|", l))}
    assert list(rows) == MODULES, rows
    assert rows["System"] == ("Yes", "System OK")
    for name in MODULES[1:]:
        assert rows[name] == ("Yes", f"{name} module enabled"), (name, rows[name])
    assert any(re.search(r"Connected to ", l) for l in out), "Wi-Fi not connected"


def test_info_uid_mac(cli):
    """System commands info/uid/mac: shape of each reply."""
    out = cli.run("$system info")
    assert any(re.match(r"Model \d+\s+Cores \d+\s+Rev \d+", l) for l in out), out
    assert any(l.startswith("IDF ") for l in out)
    assert any(re.match(r"Flash \d+ bytes @ \d+ Hz", l) for l in out)
    assert any(re.match(r"MAC ([0-9A-F]{2}:){5}[0-9A-F]{2}$", l) for l in out)
    out = cli.run("$system uid")
    assert any(re.match(r"base_mac [0-9a-fA-F]{12}$", l) for l in out), out
    assert any(re.match(r"uid64 [0-9a-fA-F]{16}$", l) for l in out), out
    out = cli.run("$system mac")
    assert any(re.match(r"wifi_sta ([0-9A-F]{2}:){5}[0-9A-F]{2}$", l) for l in out), out


def test_uptime_increases(cli):
    """Uptime (web interface status) advances with wall time."""
    _, _, u1 = cli.mem()
    time.sleep(3)
    _, _, u2 = cli.mem()
    assert 2 <= u2 - u1 <= 6, (u1, u2)


def test_heap_stable_over_50_status_calls(cli, csv_writer):
    """System::status: 50 calls leave free heap within 2 KB."""
    f0, total, up0 = cli.mem()
    rows = [[0, f0, total, up0]]
    for i in range(1, 51):
        out = cli.run("$system status", quiet=0.3)
        assert any("Web Interface" in l for l in out), (i, out)
        if i % 10 == 0:
            f, _, up = cli.mem()
            rows.append([i, f, total, up])
    f1 = rows[-1][1]
    csv_writer("heap-50-status", ["calls", "free", "total", "uptime_s"], rows)
    print(f"heap free before {f0} after {f1} delta {f1 - f0}; samples {[r[1] for r in rows]}")
    assert abs(f1 - f0) <= 2048, rows


def test_restart_keeps_provisioning(cli):
    """System::restart: `$system restart` → Rebooting → banner within 90 s, Wi-Fi reconnects, uptime resets."""
    _, _, up_before = cli.mem()
    cli.restart()
    print(f"restart: banner after {cli.boot_seconds:.1f} s")
    out = cli.run("$wifi status", quiet=1.0)
    assert "Wifi module enabled" in out and any(l.startswith("Connected to ") for l in out), out
    _, _, up_after = cli.mem()
    assert up_after < 90, up_after
    assert any("System Status" in l for l in cli.run("$system status"))
