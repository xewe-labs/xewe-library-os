"""Wave 3: rapid restart cycles (``System`` ``restart`` command -> ``System::restart`` ->
``ESP.restart()``; ``Os::begin`` prints ``System Setup Complete`` at the end of every boot)."""

import re
import time

from xewe.serialio import BOOT_READY, BOOT_UNPROVISIONED, wait_for_banner

CYCLES = 5
BOOT_TIMEOUT = 90.0


def wait_boot(cli, t0):
    """Wait for the end-of-boot banner; if the port reopen reset the chip once more (native USB:
    the reopen asserts DTR), wait for that boot too. Returns (banner, seconds since t0)."""
    m = wait_for_banner(cli.c, f"{BOOT_READY}|{BOOT_UNPROVISIONED}", BOOT_TIMEOUT, reset=False)
    while m[0] == BOOT_READY:
        tail = cli.c.collect(silence=2.0, limit=30)
        rst = [i for i, l in enumerate(tail) if l.startswith("rst:")]
        if not rst or any(BOOT_READY in l for l in tail[rst[-1]:]):
            break
        m = wait_for_banner(cli.c, f"{BOOT_READY}|{BOOT_UNPROVISIONED}", BOOT_TIMEOUT, reset=False)
    return m[0], time.monotonic() - t0


def test_rapid_restart_cycles(cli, log_dir, csv_writer):
    """5 x `$system restart` back to back: each boot reaches `System Setup Complete` within 90 s,
    never the first-boot prompt, Wi-Fi is connected afterwards, and uptime restarted from ~0."""
    rows = []
    for n in range(1, CYCLES + 1):
        start = len(cli.c.lines)
        t0 = time.monotonic()
        cli.c.send("$system restart")
        cli.c.expect(r"Rebooting", 10)
        banner, boot_s = wait_boot(cli, t0)
        assert banner == BOOT_READY, f"cycle {n}: board came up unprovisioned"
        boot_lines = cli.c.lines[start:]
        roms = sum(1 for l in boot_lines if l.startswith("ESP-ROM:"))
        joined = any(l.startswith("Joined ") for l in boot_lines)
        wifi = "\n".join(cli.run("$wifi status"))
        connected = bool(re.search(r"^Connected to ", wifi, re.M))
        free, total, up = cli.mem()
        rows.append([n, round(boot_s, 2), roms, int(joined), int(connected), up, free, total])
        print(f"cycle {n}: boot {boot_s:.1f} s, ROM banners {roms}, joined {joined}, "
              f"wifi connected {connected}, uptime {up} s, free {free}")
        assert connected, f"cycle {n}: Wi-Fi not connected after boot:\n{wifi}"
        assert up <= boot_s + 10, f"cycle {n}: uptime {up} s; the board did not restart"
    csv_writer("restart-cycles", ["cycle", "boot_s", "rom_banners", "joined", "wifi_connected",
                                  "uptime_s", "free", "total"], rows)
    if log_dir is not None:
        (log_dir / f"{time.strftime('%H%M')}-restart-cycles.log").write_text("\n".join(cli.c.lines) + "\n")
    boots = [r[1] for r in rows]
    print(f"boot times: {boots}; min {min(boots)} max {max(boots)} mean {sum(boots) / len(boots):.1f}")
    frees = [r[6] for r in rows]
    assert max(frees) - min(frees) <= 8192, f"free heap after boot varies by > 8 KB: {frees}"


def test_reboot_alias(cli):
    """`$system reboot` is the same command as restart."""
    cli.c.send("$system reboot")
    cli.c.expect(r"Rebooting", 10)
    banner, _ = wait_boot(cli, time.monotonic())
    assert banner == BOOT_READY
    assert any(re.match(r"uid64 ", l) for l in cli.run("$system uid"))
