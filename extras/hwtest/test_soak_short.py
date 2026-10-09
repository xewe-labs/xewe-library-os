"""Short soak: 200 mixed commands with heap sampled every 20 (Cli, SerialPort, Nvs, modules)."""

CYCLE = [
    ("$system status", r"\|\s*Web Interface\s*\|"),
    ("$wifi status", r"^Wifi module enabled"),
    ("$time status", r"^Time module enabled"),
    ("$pins status", r"^Pins module enabled"),
    ("$buttons add 4 \"$system status\" pullup on_press 50", r"^Successfully added button mapping\."),
    ("$buttons status", r"^\|\s*0\s*\|\s*4\s*\|"),
    ("$buttons remove 0", r"^Successfully removed button mapping\."),
    ("$schedule add 1439 1439 6 00FF00 \"$system status\"", r"^Scheduler: schedule saved"),
    ("$schedule status", r'"id":0'),
    ("$schedule remove 0", r"^Scheduler: schedule removed"),
    ("$help pins", r"Pins Commands \[pins\]"),
    ("$web_interface status", r"Memory Usage"),
    ("$nope", r"^Error: Unknown command group 'nope'"),
]


def test_soak_200_commands(cli, csv_writer):
    """200 commands, every one answered; free heap end >= start - 4 KB."""
    cli.clear_buttons()
    cli.clear_schedules()
    samples = []
    f, total, up = cli.mem()
    samples.append([0, f, total, up])
    for n in range(1, 201):
        cmd, rx = CYCLE[(n - 1) % len(CYCLE)]
        cli.c.send(cmd)
        cli.c.expect(rx, 15)
        cli.settle(0.25)
        if n % 20 == 0:
            f, _, up = cli.mem()
            samples.append([n, f, total, up])
    cli.clear_buttons()
    cli.clear_schedules()
    csv_writer("heap-soak-200", ["commands", "free", "total", "uptime_s"], samples)
    frees = [s[1] for s in samples]
    print(f"soak heap free min {min(frees)} max {max(frees)} start {frees[0]} end {frees[-1]}; {frees}")
    monotonic_drop = all(b <= a for a, b in zip(frees, frees[1:]))
    assert frees[-1] >= frees[0] - 4096, frees
    assert not (monotonic_drop and frees[0] - frees[-1] > 4096), frees
