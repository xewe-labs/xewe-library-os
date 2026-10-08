#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
# SPDX-License-Identifier: GPL-3.0-only
# xewe-os-core/extras/host/run.sh
#
# Host-native checks for the hardware-free parts of XeWeCore (Utils, Serial, Cli).
# Builds the compile probes and the unit tests with the host g++ against the
# Arduino/FreeRTOS shim, once at -std=c++17 (exercises the xewe::span fallback)
# and once at -std=gnu++2b (what the ESP32 core uses), and runs the tests.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$HERE/../../src"
CXX="${CXX:-g++}"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

FLAGS=(-Wall -Wextra -fno-exceptions -I "$HERE/shim" -I "$SRC")
LIB_SRCS=("$SRC/XeWeCore/Serial.cpp" "$SRC/XeWeCore/Cli.cpp")

for std in c++17 gnu++2b; do
    echo "== $std: probes"
    for p in "$HERE"/probe/*.probe.cpp; do
        "$CXX" -std="$std" "${FLAGS[@]}" -c "$p" -o "$OUT/$(basename "$p").$std.o"
        echo "   ok $(basename "$p")"
    done

    echo "== $std: tests"
    "$CXX" -std="$std" "${FLAGS[@]}" "$HERE"/test/*.cpp "${LIB_SRCS[@]}" -o "$OUT/tests.$std"
    "$OUT/tests.$std"
done
echo "== all host checks passed"
