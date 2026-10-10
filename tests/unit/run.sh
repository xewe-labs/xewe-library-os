#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
# SPDX-License-Identifier: GPL-3.0-only
# xewe-os-core/tests/unit/run.sh
#
# Host-native checks for the hardware-free parts of XeWeCore: Utils, Serial and Cli; with
# ArduinoJson also FlexData, and at gnu++2b Nvs and Settings against an in-memory NVS shim.
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

# FlexData tests need ArduinoJson (header-only): ARDUINOJSON_SRC, else the copy ./setup.sh put in
# build/libraries, else one a sibling project downloaded (../<project>/build/libraries), else the
# shared toolchain copy.
if [ -z "${ARDUINOJSON_SRC:-}" ]; then
    ARDUINOJSON_SRC="$HERE/../../../../.toolchain/user/libraries/ArduinoJson/src"
    for candidate in "$HERE"/../../build/libraries/ArduinoJson/src "$HERE"/../../../*/build/libraries/ArduinoJson/src; do
        if [ -f "$candidate/ArduinoJson.h" ]; then
            ARDUINOJSON_SRC="$candidate"
            break
        fi
    done
fi
JSON_TESTS=()
if [ -f "$ARDUINOJSON_SRC/ArduinoJson.h" ]; then
    FLAGS+=(-I "$ARDUINOJSON_SRC")
    JSON_TESTS=("$HERE"/test/json/*.cpp)
else
    echo "== ArduinoJson not found (set ARDUINOJSON_SRC): FlexData tests skipped"
fi

for std in c++17 gnu++2b; do
    echo "== $std: probes"
    for p in "$HERE"/probe/*.probe.cpp; do
        "$CXX" -std="$std" "${FLAGS[@]}" -c "$p" -o "$OUT/$(basename "$p").$std.o"
        echo "   ok $(basename "$p")"
    done

    echo "== $std: host-includable headers (standard library only, no Arduino shim)"
    for h in Utils/Color.h Utils/String.h Utils/Pins.h Utils/Listeners.h; do
        printf '#include <XeWeCore/%s>\n' "$h" | "$CXX" -std="$std" -Wall -Wextra -fno-exceptions -I "$SRC" -x c++ -fsyntax-only -
        echo "   ok $h"
    done

    echo "== $std: tests"
    # ${a[@]+"${a[@]}"}: an empty array under `set -u` aborts macOS bash 3.2
    "$CXX" -std="$std" "${FLAGS[@]}" "$HERE"/test/*.cpp ${JSON_TESTS[@]+"${JSON_TESTS[@]}"} "${LIB_SRCS[@]}" -o "$OUT/tests.$std"
    "$OUT/tests.$std"
done
echo "== all unit checks passed"
