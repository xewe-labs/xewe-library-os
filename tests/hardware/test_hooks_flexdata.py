"""FlexData JSON/blob round trips on the board through ``$test flex`` / ``$test flex_bad``.

Same cases as tests/host/test/json/test_flexdata.cpp. The firmware's Probe struct has fields
b(bool) i(int32) u(uint32) l(int64) f(float) d(double) s(string) v(int32[]) in{a,s} vi[{a,s}].
"""

from __future__ import annotations

import pytest

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # --import-mode=importlib: hooks.py is a sibling
from hooks import hooks, quote  # noqa: F401,E402

DEFAULT = '{"b":false,"i":0,"u":0,"l":0,"f":0,"d":0,"s":"","v":[],"in":{"a":0,"s":""},"vi":[]}'
FULL = ('{"b":true,"i":-7,"u":4000000000,"l":-9000000000,"f":1.5,"d":0.25,"s":"x y",'
        '"v":[1,-2,3],"in":{"a":9,"s":"n"},"vi":[{"a":1,"s":"p"},{"a":2,"s":"q"}]}')


def flex(hooks, json: str) -> dict[str, str]:
    lines = hooks.run(f"$test flex {quote(json)}", until=r"^blob_len=")
    out = {}
    for line in lines:
        key, sep, value = line.partition("=")
        if sep and key in ("parse", "types", "json", "stable"):
            out[key] = value
    out.update(hooks.kv([line for line in lines if line.startswith("blob_len=")]))
    return out


def test_default_object(hooks):
    r = flex(hooks, "{}")
    assert r["parse"] == "Ok" and r["json"] == DEFAULT and r["stable"] == "1"
    assert r["blob_len"] == "50" and r["blob_ok"] == "1" and r["blob_equal"] == "1"


def test_full_round_trip(hooks):
    r = flex(hooks, FULL)
    assert r["parse"] == "Ok"
    assert r["json"] == FULL
    assert r["stable"] == "1" and r["blob_ok"] == "1" and r["blob_equal"] == "1"
    assert r["types"] == "b:bool,i:int,u:int,l:int,f:float,d:float,s:string,v:array,in:object,vi:array"


def test_partial_input_fills_defaults(hooks):
    r = flex(hooks, '{"i":5,"unknown":1}')
    assert r["json"] == DEFAULT.replace('"i":0', '"i":5')


@pytest.mark.parametrize("case,parse,json", [
    ("truncated", "IncompleteInput", DEFAULT),
    ("trailing", "Ok", DEFAULT.replace('"i":0', '"i":1')),
    ("empty", "EmptyInput", DEFAULT),
    ("null", "Ok", DEFAULT),
    ("array", "Ok", DEFAULT),
    ("deep", "TooDeep", DEFAULT),
    # FINDING (not fixed, see core-test-report wave 2): as<bool>() turns ANY string into true,
    # so {"b":"yes"} (and {"b":"false"}) set b=true; numbers become strings
    ("wrongtypes", "Ok", DEFAULT.replace('"s":""', '"s":"5"', 1).replace('"b":false', '"b":true')),
])
def test_malformed_json(hooks, case, parse, json):
    lines = hooks.run(f"$test flex_bad {case}", until=r"^blob_len=")
    got = {k: v for k, _, v in (line.partition("=") for line in lines) if k in ("parse", "json", "stable")}
    assert got["parse"] == parse
    assert got["json"] == json
    assert got["stable"] == "1"
    hooks.alive()


def test_huge_numbers(hooks):
    """Out-of-range numbers: no crash, blob round trip intact. Values recorded, not asserted
    beyond the integer fields (ArduinoJson converts out-of-range integers to 0)."""
    lines = hooks.run("$test flex_bad hugenum", until=r"^blob_len=")
    got = {k: v for k, _, v in (line.partition("=") for line in lines) if k in ("parse", "json", "stable")}
    print("hugenum:", got)
    assert got["parse"] == "Ok"
    assert '"i":0' in got["json"] and '"u":0' in got["json"]
    # FINDING: 1e400 parses as inf, serializes as null; the canonical JSON is then not stable
    assert '"f":null' in got["json"] and got["stable"] == "0"
    assert "blob_ok=1" in "\n".join(lines)
    hooks.alive()


def test_unicode(hooks):
    lines = hooks.run("$test flex_bad unicode", until=r"^blob_len=")
    got = {k: v for k, _, v in (line.partition("=") for line in lines) if k in ("parse", "json")}
    print("unicode:", got)
    assert got["parse"] == "Ok"
    assert got["json"].startswith('{"b":false') and "é\U0001F600" in got["json"]
    assert "blob_equal=1" in "\n".join(lines)


def test_unicode_via_cli(hooks):
    r = flex(hooks, '{"s":"héllo \U0001F600"}')
    assert '"s":"héllo \U0001F600"' in r["json"] and r["blob_equal"] == "1"


def test_long_string(hooks):
    lines = hooks.run("$test flex_bad longstr", until=r"^blob_len=", limit=30)
    js = next(line for line in lines if line.startswith("json="))
    assert '"s":"' + "x" * 3000 + '"' in js
    assert "blob_equal=1" in "\n".join(lines)


@pytest.mark.parametrize("case", ["blob_version", "blob_short", "blob_empty", "blob_strlen", "blob_veccount"])
def test_corrupt_blob_rejected(hooks, case):
    """blob_veccount: a 4 G element count used to make reserve() abort (fixed 2026-10-08)."""
    assert hooks.call(f"$test flex_bad {case}") == {"blob_ok": "0"}
    hooks.alive()


def test_string_coerced_to_bool_true(hooks):
    """Documents current behaviour: the string "false" stored in a bool field reads as true."""
    assert '"b":true' in flex(hooks, '{"b":"false"}')["json"]


def test_unknown_case(hooks):
    assert any("error=unknown case" in line for line in hooks.run("$test flex_bad nope"))
