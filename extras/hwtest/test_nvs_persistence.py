"""Black-box NVS persistence across `$system restart` (``Nvs`` via Buttons/Scheduler/Module state)."""

import re


def test_button_and_schedule_survive_restart(cli):
    """Nvs: a button mapping and a schedule written before a restart are listed after it."""
    cli.clear_buttons()
    cli.clear_schedules()
    assert "Successfully added button mapping." in cli.run('$buttons add 4 "$pins status" pullup on_release 75')
    assert "Scheduler: schedule saved" in cli.run('$schedule add 600 660 3 0000FF "$time status"')
    cli.restart()
    rows = [l.strip() for l in cli.run("$buttons status")]
    assert any(re.match(r"^\|\s*0\s*\|\s*4\s*\|\s*\$pins status\s*\|\s*75\s*\|\s*pullup\s*\|\s*on_release\s*\|", l)
               for l in rows), rows
    out = "\n".join(cli.run("$schedule status"))
    assert '"start_time":600' in out and '"end_time":660' in out and '"day":3' in out, out
    assert '"displayed_color":"0000FF"' in out and '"commands":["$time status"]' in out, out
    cli.clear_buttons()
    cli.clear_schedules()


def test_removal_survives_restart(cli):
    """Nvs: removals are persisted too (empty tables after restart)."""
    cli.restart()
    assert cli.button_ids() == []
    assert cli.schedule_ids() == []


def test_enable_state_survives_restart(cli):
    """Module enable flags (Nvs 'is_enabled') read back after restart: every module still Yes."""
    cli.restart()
    out = [l.strip() for l in cli.run("$system status")]
    rows = [m for l in out if (m := re.fullmatch(r"\|\s*([^|\s][^|]*?)\s*\|\s*(Yes|No)\s*\|.*", l))]
    assert len(rows) == 7 and all(m[2] == "Yes" for m in rows), out
