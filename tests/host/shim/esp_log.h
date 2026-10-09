// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/host/shim/esp_log.h
#pragma once

// Host shim: ESP_LOGE goes to stderr.

#include <cstdio>

#define ESP_LOGE(tag, fmt, ...) std::fprintf(stderr, "E (%s) " fmt "\n", tag, ##__VA_ARGS__)
