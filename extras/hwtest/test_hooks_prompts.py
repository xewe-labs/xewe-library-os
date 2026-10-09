"""Prompt timeouts, retries and answers (SerialPort::get_core) through ``$test prompt_*``.

prompt_yn <timeout_ms> <retries>          -> get_yn("test yn?", retries, timeout, default false)
prompt_str <min> <max> <timeout_ms>       -> get_string(..., retries 1, default "<default>")
prompt_int <lo> <hi> <timeout_ms>         -> get_int(..., retries 2, default -12345)
Each prints ``result=<v> success=<0|1>``. Answers are sent only after the iteration prompt
appears: every prompt starts with clear_input(), which drops type-ahead.
"""

from __future__ import annotations

import time

import pytest

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # --import-mode=importlib: hooks.py is a sibling
from hooks import hooks  # noqa: F401,E402

YN = r"\(y/n\) > "
GT = r"^> $|^> "
RESULT = r"result="


def start(hooks, cmd: str, marker: str) -> None:
    hooks.c.send(cmd)
    hooks.c.expect(marker, 5)


def answer(hooks, text: str, marker: str | None = None) -> None:
    hooks.c.send(text)
    if marker:
        hooks.c.expect(marker, 5)


def result(hooks, timeout: float = 15) -> dict[str, str]:
    m = hooks.c.expect(RESULT, timeout)
    line = hooks.c.lines[hooks.c._cursor - 1]
    assert m
    return hooks.kv([line])


def test_yn_answer_yes(hooks):
    start(hooks, "$test prompt_yn 5000 1", YN)
    answer(hooks, "y")
    assert result(hooks) == {"result": "1", "success": "1"}


@pytest.mark.parametrize("text,value", [("YES", "1"), ("true", "1"), ("1", "1"), ("No", "0"), ("false", "0"), ("0", "0")])
def test_yn_accepted_spellings(hooks, text, value):
    start(hooks, "$test prompt_yn 5000 1", YN)
    answer(hooks, text)
    assert result(hooks) == {"result": value, "success": "1"}


def test_yn_single_attempt_timeout(hooks):
    t0 = time.monotonic()
    start(hooks, "$test prompt_yn 800 1", YN)
    hooks.c.expect(r"^! Timeout\.$", 5)
    r = result(hooks)
    dt = time.monotonic() - t0
    assert r == {"result": "0", "success": "0"}
    assert 0.7 <= dt <= 3.0, dt


def test_yn_retries_exhausted_by_timeouts(hooks):
    t0 = time.monotonic()
    hooks.c.send("$test prompt_yn 500 3")
    r = result(hooks, 10)
    dt = time.monotonic() - t0
    timeouts = sum(1 for line in hooks.c.lines if line.strip() == "! Timeout.")
    assert r == {"result": "0", "success": "0"}
    assert timeouts == 3, hooks.c.lines
    assert 1.4 <= dt <= 4.0, dt


def test_yn_invalid_then_valid(hooks):
    start(hooks, "$test prompt_yn 5000 2", YN)
    answer(hooks, "maybe", r"^! Please answer 'y' or 'n'\.$")
    hooks.c.expect(YN, 5)
    answer(hooks, "n")
    assert result(hooks) == {"result": "0", "success": "1"}


def test_yn_invalid_exhausts_attempts(hooks):
    start(hooks, "$test prompt_yn 5000 2", YN)
    answer(hooks, "maybe", r"Please answer")
    hooks.c.expect(YN, 5)
    answer(hooks, "perhaps", r"Please answer")
    assert result(hooks) == {"result": "0", "success": "0"}


def test_yn_infinite_retries_with_timeout(hooks):
    """retries 0 = infinite: timeouts re-prompt until a valid answer arrives."""
    start(hooks, "$test prompt_yn 400 0", YN)
    for _ in range(3):
        hooks.c.expect(r"^! Timeout\.$", 5)
    hooks.c.expect(YN, 5)
    answer(hooks, "y")
    assert result(hooks) == {"result": "1", "success": "1"}


def test_yn_type_ahead_dropped(hooks):
    """The answer in the same write as the command is discarded by clear_input()."""
    hooks.c._ser.write(b"$test prompt_yn 1500 1\ny\n")
    hooks.c._ser.flush()
    hooks.c.mark()
    r = result(hooks)
    assert r == {"result": "0", "success": "0"}, "type-ahead answered the prompt"


@pytest.mark.parametrize("cmd", ["$test prompt_yn 0 1", "$test prompt_yn 60001 1", "$test prompt_yn 100 11",
                                 "$test prompt_yn x y", "$test prompt_str 5 300 100", "$test prompt_int a 1 100"])
def test_prompt_bad_args(hooks, cmd):
    assert any("error=bad args" in line for line in hooks.run(cmd))


def test_str_valid(hooks):
    start(hooks, "$test prompt_str 2 5 5000", GT)
    answer(hooks, "abc")
    assert result(hooks) == {"result": '"abc"', "success": "1"}


def test_str_not_trimmed_and_unicode(hooks):
    start(hooks, "$test prompt_str 0 20 5000", GT)
    answer(hooks, " é ")
    assert result(hooks)["result"] == '" \\xC3\\xA9 "'


def test_str_too_short_gives_default(hooks):
    start(hooks, "$test prompt_str 2 5 5000", GT)
    answer(hooks, "a", r"! Length must be in \[2\.\.5\] chars\.")
    assert result(hooks) == {"result": '"<default>"', "success": "0"}


def test_str_empty_allowed_when_min_zero(hooks):
    start(hooks, "$test prompt_str 0 5 5000", GT)
    answer(hooks, "")
    assert result(hooks) == {"result": '""', "success": "1"}


def test_str_max_zero_means_254(hooks):
    start(hooks, "$test prompt_str 0 0 5000", GT)
    answer(hooks, "z" * 254)
    assert result(hooks)["success"] == "1"


def test_str_timeout(hooks):
    hooks.c.send("$test prompt_str 1 5 600")
    assert result(hooks) == {"result": '"<default>"', "success": "0"}


def test_int_valid_and_bounds(hooks):
    start(hooks, "$test prompt_int -5 5 5000", GT)
    answer(hooks, "-5")
    assert result(hooks) == {"result": "-5", "success": "1"}


def test_int_out_of_range_then_invalid_exhausts(hooks):
    start(hooks, "$test prompt_int -5 5 5000", GT)
    answer(hooks, "9", r"! Out of range \[-5\.\.5\]\.")
    hooks.c.expect(GT, 5)
    answer(hooks, "x", r"! Invalid number")
    assert result(hooks) == {"result": "-12345", "success": "0"}


def test_int_swapped_bounds(hooks):
    """lo > hi: get_integral swaps them."""
    start(hooks, "$test prompt_int 5 -5 5000", GT)
    answer(hooks, "0")
    assert result(hooks) == {"result": "0", "success": "1"}


def test_int_timeout_then_answer(hooks):
    start(hooks, "$test prompt_int 0 9 700", GT)
    hooks.c.expect(r"^! Timeout\.$", 5)
    hooks.c.expect(GT, 5)
    answer(hooks, "7")
    assert result(hooks) == {"result": "7", "success": "1"}
