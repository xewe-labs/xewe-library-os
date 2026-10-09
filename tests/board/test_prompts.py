"""Black-box tests of interactive prompts (``SerialPort::get_yn``/``get_core``, ``Module::disable``).

``$buttons disable`` asks `OK?` with get_yn(retry_count=2, timeout_ms=15000, default false): two
attempts of 15 s (a typo or a timeout re-prompts once; worst stall 30 s); a second timeout or invalid
answer prints `! No answer: disable cancelled` and aborts (core fix after wave 1; before it the
prompt waited forever and re-prompted on invalid input).
"""

import re
import time

PROMPT = r"\(y/n\) > "


def test_disable_answer_no_aborts(cli):
    """Module::disable + get_yn: 'n' aborts; module stays enabled."""
    cli.c.send("$buttons disable")
    cli.c.expect(r"Disabling Buttons", 5)
    cli.c.expect(r"^OK\?$", 5)
    cli.c.expect(PROMPT, 5)
    cli.c.send("n")
    cli.c.expect(r"^Aborted$", 5)
    assert "Buttons module enabled" in cli.run("$buttons status")


CANCELLED = r"^! No answer: disable cancelled$"


def test_invalid_answer_cancels(cli):
    """get_yn(retry 2): one invalid answer re-prompts once; a second invalid answer cancels."""
    bad_msg = r"^! Please answer 'y' or 'n'\.$"
    cli.c.send("$buttons disable")
    cli.c.expect(PROMPT, 5)
    cli.c.send("maybe")
    cli.c.expect(bad_msg, 5)
    cli.c.expect(PROMPT, 5)  # re-prompted once
    cli.c.send("")  # an empty line is not an answer either
    cli.c.expect(bad_msg, 5)
    cli.c.expect(CANCELLED, 5)
    cli.c.expect(r"^Aborted$", 5)
    late = cli.settle(1.5)
    assert not any("(y/n)" in l for l in late), late  # no third attempt
    assert "Buttons module enabled" in cli.run("$buttons status")
    # a typo followed by a real answer: the answer is taken, no "No answer" line
    cli.c.send("$buttons disable")
    cli.c.expect(PROMPT, 5)
    cli.c.send("maybe")
    cli.c.expect(bad_msg, 5)
    cli.c.expect(PROMPT, 5)
    cli.c.send("NO")  # case-insensitive
    cli.c.expect(r"^Aborted$", 5)
    assert "Buttons module enabled" in cli.run("$buttons status")


def test_disable_prompt_times_out_after_30s(cli):
    """Module::disable: nobody answers → `! Timeout.` at ~15 s, re-prompt, `! Timeout.` at ~30 s,
    cancelled, module stays enabled."""
    cli.c.send("$buttons disable")
    cli.c.expect(PROMPT, 5)
    t0 = time.monotonic()
    cli.c.expect(r"^! Timeout\.$", 25)
    first = time.monotonic() - t0
    cli.c.expect(PROMPT, 5)
    cli.c.expect(r"^! Timeout\.$", 25)
    elapsed = time.monotonic() - t0
    cli.c.expect(CANCELLED, 5)
    cli.c.expect(r"^Aborted$", 5)
    assert 13.5 <= first <= 17.5, first
    assert 28.0 <= elapsed <= 34.0, elapsed
    late = cli.settle(2.0)
    assert not any("Timeout" in l or "(y/n)" in l for l in late), late  # two attempts only
    assert "Buttons module enabled" in cli.run("$buttons status")
    out = [l.strip() for l in cli.run("$system status")]
    assert any(re.match(r"\|\s*Buttons\s*\|\s*Yes\s*\|", l) for l in out), out


def test_type_ahead_is_discarded_by_prompt(cli):
    """get_core calls clear_input(): an answer sent in the same write as the command is dropped.

    Documents current behaviour: scripted `cmd\\nanswer\\n` pastes do not answer prompts.
    """
    cli.write(b"$buttons disable\nn\n")
    cli.c.expect(PROMPT, 5)
    late = cli.settle(1.5)
    assert not any("Aborted" in l for l in late), "type-ahead answered the prompt"
    cli.c.send("n")
    cli.c.expect(r"^Aborted$", 5)


def test_disable_yes_then_enable(cli):
    """Module::disable('y') → reset + restart → disabled after boot; Module::enable restores (restart)."""
    from xewe.serialio import BOOT_READY, wait_for_banner

    def reboot_wait():
        cli.c.expect(r"Rebooting", 10)
        wait_for_banner(cli.c, BOOT_READY, 90, reset=False)
        cli.settle(2.0)

    cli.c.send("$buttons disable")
    cli.c.expect(PROMPT, 5)
    cli.c.send("y")
    try:
        cli.c.expect(r"Buttons module disabled", 5)
        cli.c.expect(r"Buttons module reset", 5)
        reboot_wait()  # Module::disable restarts the board itself (do_restart = true)
        assert "Buttons module disabled" in cli.run("$buttons status")
        # module commands stay registered while disabled; Buttons' add is silently ignored
        out = cli.run('$buttons add 4 "$system status" pullup on_press 50')
        assert not any("Successfully added" in l for l in out), out
        out = [l.strip() for l in cli.run("$system status")]
        assert any(re.match(r"\|\s*Buttons\s*\|\s*No\s*\|", l) for l in out), out
    finally:
        cli.settle()
        cli.c.send("$buttons enable")
        m = cli.c.expect(r"Buttons module (already )?enabled", 5)
        if not m[1]:
            reboot_wait()
    assert "Buttons module enabled" in cli.run("$buttons status")
    out = cli.run("$buttons enable")
    assert any("Buttons module already enabled" in l for l in out), out


def test_system_cannot_be_disabled(cli):
    """Module::register_generic_commands: System has no enable/disable commands."""
    assert "Error: Unknown command 'disable' in command group 'system'" in cli.run("$system disable")
