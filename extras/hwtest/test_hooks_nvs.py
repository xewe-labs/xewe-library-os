"""NVS through the ``$test nvs*`` hooks (Nvs::write/read/remove/reset_ns on the real flash).

Writes go to namespaces starting with ``xt`` only (the firmware refuses others). Each test
cleans its namespace. Needs firmware built with XEWE_TESTING (see hooks.py).
"""

from __future__ import annotations

import pytest

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # --import-mode=importlib: hooks.py is a sibling
from hooks import hooks, quote, unquote  # noqa: F401,E402  (fixture import)

NS = "xtnvs"


@pytest.fixture
def ns(hooks):
    hooks.call(f"$test nvs_reset {NS}")
    yield NS
    hooks.call(f"$test nvs_reset {NS}")


ROUND_TRIPS = [
    ("bool", "1", "1"),
    ("bool", "false", "0"),
    ("i32", "-2147483648", "-2147483648"),
    ("i32", "2147483647", "2147483647"),
    ("u32", "4294967295", "4294967295"),
    ("i64", "-9223372036854775808", "-9223372036854775808"),
    ("i64", "9223372036854775807", "9223372036854775807"),
    ("float", "3.14159", "3.14159012"),
    ("float", "-0.5", "-0.5"),
    ("double", "2.718281828459045", "2.7182818284590451"),
    ("str", "hello world", "hello world"),
    ("str", "", ""),
    ("str", "héllo \U0001F600", "héllo \U0001F600"),
    ("str", 'quote " and \\ back', 'quote " and \\ back'),
]


@pytest.mark.parametrize("typ,value,expected", ROUND_TRIPS, ids=[f"{t}:{v[:12]}" for t, v, _ in ROUND_TRIPS])
def test_round_trip(hooks, ns, typ, value, expected):
    assert hooks.call(f"$test nvs {ns} k {typ} {quote(value)}") == {"write": "1"}
    r = hooks.call(f"$test nvs_read {ns} k {typ}")
    assert r["found"] == "1"
    assert unquote(r["value"]) == expected


def test_missing_key_and_delete(hooks, ns):
    assert hooks.call(f"$test nvs_read {ns} nothere i32")["found"] == "0"
    hooks.call(f"$test nvs {ns} gone i32 5")
    assert hooks.call(f"$test nvs_read {ns} gone i32")["found"] == "1"
    assert hooks.call(f"$test nvs_del {ns} gone") == {"deleted": "1"}
    assert hooks.call(f"$test nvs_read {ns} gone i32")["found"] == "0"
    # deleting a missing key is silent
    assert hooks.call(f"$test nvs_del {ns} gone") == {"deleted": "1"}


def test_overwrite_same_type(hooks, ns):
    hooks.call(f"$test nvs {ns} k str first")
    hooks.call(f"$test nvs {ns} k str second")
    assert unquote(hooks.call(f"$test nvs_read {ns} k str")["value"]) == "second"


def test_type_mismatch_reads_as_missing(hooks, ns):
    hooks.call(f"$test nvs {ns} k i32 7")
    assert hooks.call(f"$test nvs_read {ns} k str")["found"] == "0"
    assert hooks.call(f"$test nvs_read {ns} k u32")["found"] == "0"
    assert hooks.call(f"$test nvs_read {ns} k i32")["value"] == '"7"'


def test_overwrite_with_other_type(hooks, ns):
    """Same key, new type: the new value reads back. Whether the old typed entry survives is
    ESP-IDF behaviour; it is recorded in the report, not asserted."""
    hooks.call(f"$test nvs {ns} k i32 7")
    assert hooks.call(f"$test nvs {ns} k str seven") == {"write": "1"}
    assert unquote(hooks.call(f"$test nvs_read {ns} k str")["value"]) == "seven"
    old = hooks.call(f"$test nvs_read {ns} k i32")
    print(f"old i32 entry after str overwrite: found={old['found']} value={old['value']}")


def test_key_length_limits(hooks, ns):
    k15, k16 = "k" * 15, "k" * 16
    assert hooks.call(f"$test nvs {ns} {k15} i32 15") == {"write": "1"}
    assert hooks.call(f"$test nvs_read {ns} {k15} i32")["value"] == '"15"'
    reply = hooks.run(f"$test nvs {ns} {k16} i32 16")
    assert any("too long (16 chars > 15 max)" in line for line in reply), reply
    assert hooks.kv(reply)["write"] == "0"
    ns16 = "xt" + "n" * 14
    reply = hooks.run(f"$test nvs {ns16} k i32 1")
    assert any("too long" in line for line in reply), reply
    assert hooks.kv(reply)["write"] == "0"


def test_namespace_isolation(hooks, ns):
    other = "xtnvs2"
    hooks.call(f"$test nvs {ns} same i32 1")
    hooks.call(f"$test nvs {other} same i32 2")
    assert hooks.call(f"$test nvs_read {ns} same i32")["value"] == '"1"'
    assert hooks.call(f"$test nvs_read {other} same i32")["value"] == '"2"'
    hooks.call(f"$test nvs_reset {other}")
    assert hooks.call(f"$test nvs_read {other} same i32")["found"] == "0"
    assert hooks.call(f"$test nvs_read {ns} same i32")["found"] == "1"


@pytest.mark.parametrize("cmd,err", [
    ("$test nvs system k i32 1", "namespace must start with xt"),
    ("$test nvs_del wifi ssid", "namespace must start with xt"),
    ("$test nvs_reset root", "namespace must start with xt"),
    (f"$test nvs {NS} k i32 abc", "bad i32"),
    (f"$test nvs {NS} k i32 99999999999", "bad i32"),
    (f"$test nvs {NS} k u32 -1", "bad u32"),
    (f"$test nvs {NS} k bool maybe", "bad bool"),
    (f"$test nvs {NS} k float 1e999", "bad float"),
    (f"$test nvs {NS} k blob x", "unknown type"),
    ("$test nvs_read xtnvs k blob", "unknown type"),
    ("$test nvs_stress 0", "n must be 1..200"),
    ("$test nvs_stress 201", "n must be 1..200"),
    ("$test nvs_stress abc", "n must be 1..200"),
])
def test_garbage_arguments_report_errors(hooks, cmd, err):
    reply = hooks.run(cmd)
    assert any(f"error={err}" in line for line in reply), reply
    hooks.alive()


def test_reading_foreign_namespaces_is_allowed_and_safe(hooks):
    """Reads are unrestricted: the device name written at provisioning is visible."""
    r = hooks.call("$test nvs_read system device_name str")
    assert r["found"] == "1" and len(unquote(r["value"])) > 0


def test_stress(hooks):
    reply = hooks.run("$test nvs_stress 50", until=r"stress n=50", limit=120)
    fails = [line for line in reply if line.startswith("fail=")]
    kv = hooks.kv([line for line in reply if line.startswith("stress ")])
    assert fails == [], fails
    assert kv["fail"] == "0" and int(kv["pass"]) >= 200
    # the two expected rejections were reported, not crashed on
    assert any("too long" in line for line in reply)
    hooks.alive()


def test_stress_heap_stable(hooks):
    before = hooks.heap()["free"]
    hooks.run("$test nvs_stress 20", until=r"stress n=20", limit=60)
    after = hooks.heap()["free"]
    assert after >= before - 2048, f"free heap {before} -> {after}"
