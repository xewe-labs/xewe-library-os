"""Utils parsers and validators on the board through ``$test validate`` / ``validate_range``.

Same cases as extras/host/test/test_parsers.cpp (keep in sync): the board runs the same
header-only code under the ESP32 toolchain (newlib strtoll/strtod, 32-bit long).
"""

from __future__ import annotations

import pytest

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # --import-mode=importlib: hooks.py is a sibling
from hooks import hooks, quote, unquote  # noqa: F401,E402

# (kind, input, ok, value)
CASES = [
    ("parse_int", "42", True, "42"),
    ("parse_int", " -17 ", True, "-17"),
    ("parse_int", "+5", True, "5"),
    ("parse_int", "12x", False, ""),
    ("parse_int", "0x10", False, ""),
    ("parse_int", "1e3", False, ""),
    ("parse_int", "", False, ""),
    ("parse_int", "9223372036854775807", True, "9223372036854775807"),
    ("parse_int", "9223372036854775808", False, ""),          # fixed: was LLONG_MAX
    ("parse_int", "-9223372036854775809", False, ""),         # fixed: was LLONG_MIN
    ("parse_i64", "-9223372036854775808", True, "-9223372036854775808"),
    ("parse_u8", "255", True, "255"),
    ("parse_u8", "256", False, ""),
    ("parse_u8", "-0", False, ""),                            # fixed: was 0
    ("parse_u32", "4294967295", True, "4294967295"),
    ("parse_u32", "4294967296", False, ""),
    ("parse_u32", "-1", False, ""),
    ("parse_float", " 1.5 ", True, "1.5"),
    ("parse_float", "1e400", False, ""),
    ("parse_float", "abc", False, ""),
    ("parse_float", "1.5x", False, ""),
    ("parse_float", "0x10", True, "16"),                      # strtod leniency (documented)
    ("gmt", "GMT", True, "GMT+00:00"),
    ("gmt", "utc", True, "GMT+00:00"),
    ("gmt", "GMT+5", True, "GMT+05:00"),
    ("gmt", "GMT+0530", True, "GMT+05:30"),
    ("gmt", "GMT-14", True, "GMT-14:00"),
    ("gmt", "GMT+5:30", True, "GMT+05:30"),
    ("gmt", "GMT+14:01", False, ""),
    ("gmt", "GMT+5:60", False, ""),
    ("gmt", "GMT+", False, ""),
    ("gmt", "GMT+123456", False, ""),
    ("gmt", "EST", False, ""),
    ("gmt", "GMT*5", False, ""),
    ("day", "mo", True, "0"),
    ("day", "SU", True, "6"),
    ("day", "MON", False, ""),
    ("time", "24:00", True, "1440"),
    ("time", "24:01", False, ""),
    ("time", "7:5", True, "425"),
    ("time", "7", False, ""),
    ("time", "12:60", False, ""),
    ("escape_json", 'a"b\\c', True, 'a\\"b\\\\c'),
    ("capitalize", "kitchen lights", True, "Kitchen Lights"),
    ("upper", "abc", True, "ABC"),
]

# lenient inputs recorded in the report (behaviour, not asserted as right or wrong)
LENIENT = [("gmt", "GMT+5:30abc"), ("gmt", "GMT+ 5"), ("time", "12:30abc"), ("time", "-0:30"),
           ("parse_float", "nan"), ("parse_float", "inf"), ("parse_float", "1e-400")]


@pytest.mark.parametrize("kind,value,ok,expected", CASES, ids=[f"{k}:{v}" for k, v, _, _ in CASES])
def test_validate(hooks, kind, value, ok, expected):
    r = hooks.call(f"$test validate {kind} {quote(value)}")
    assert r["ok"] == ("1" if ok else "0"), r
    assert unquote(r["value"]) == expected


def test_extract_commands(hooks):
    r = hooks.call("$test validate extract " + quote('"$a b" "$c \\"d\\""'))
    assert unquote(r["value"]) == '2:$a b|$c "d"'


@pytest.mark.parametrize("kind,lo,hi,value,ok,expected", [
    ("int", "-10", "10", "-5", True, "-5"),
    ("int", "-10", "10", "-11", False, ""),
    ("i8", "0", "1000", "300", False, ""),          # fixed: was 44 (truncated)
    ("i8", "-1000", "1000", "-128", True, "-128"),
    ("u8", "0", "1000", "256", False, ""),          # fixed: was 0
    ("u8", "0", "255", "128", True, "128"),
    ("u32", "0", "4294967295", "4294967295", True, "4294967295"),
    ("u32", "0", "4294967295", "-1", False, ""),
    ("i64", "-10", "10", "-5", True, "-5"),
    ("float", "0", "2", "1.5", True, "1.5"),
    ("float", "0", "1", "nan", False, ""),
    ("str", "1", "4", "abc", True, "abc"),
    ("str", "1", "4", "abcde", False, ""),
    ("str", "0", "0", "", True, ""),
])
def test_validate_range(hooks, kind, lo, hi, value, ok, expected):
    r = hooks.call(f"$test validate_range {kind} {lo} {hi} {quote(value)}")
    assert r["ok"] == ("1" if ok else "0"), r
    assert unquote(r["value"]) == expected


def test_lenient_inputs_recorded(hooks):
    for kind, value in LENIENT:
        r = hooks.call(f"$test validate {kind} {quote(value)}")
        print(f"lenient {kind} {value!r}: ok={r.get('ok')} value={r.get('value')}")


@pytest.mark.parametrize("cmd", ["$test validate nope 1", "$test validate_range nope 1 2 3",
                                 "$test validate_range int x 2 3", "$test validate_range float 1 y 3"])
def test_bad_kinds_and_bounds(hooks, cmd):
    assert any(line.startswith("error=") for line in hooks.run(cmd))
    hooks.alive()
