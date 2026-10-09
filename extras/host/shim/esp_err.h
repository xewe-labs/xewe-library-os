// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/extras/host/shim/esp_err.h
#pragma once

// Host shim: the ESP-IDF error codes Nvs.cpp uses (values as in ESP-IDF 5.x).

#include <cstdint>

typedef int esp_err_t;

#define ESP_OK                          0
#define ESP_FAIL                        -1
#define ESP_ERR_INVALID_ARG             0x102
#define ESP_ERR_NOT_FOUND               0x105
#define ESP_ERR_NVS_BASE                0x1100
#define ESP_ERR_NVS_NOT_INITIALIZED     (ESP_ERR_NVS_BASE + 0x01)
#define ESP_ERR_NVS_NOT_FOUND           (ESP_ERR_NVS_BASE + 0x02)
#define ESP_ERR_NVS_TYPE_MISMATCH       (ESP_ERR_NVS_BASE + 0x03)
#define ESP_ERR_NVS_READ_ONLY           (ESP_ERR_NVS_BASE + 0x04)
#define ESP_ERR_NVS_NOT_ENOUGH_SPACE    (ESP_ERR_NVS_BASE + 0x05)
#define ESP_ERR_NVS_INVALID_NAME        (ESP_ERR_NVS_BASE + 0x06)
#define ESP_ERR_NVS_INVALID_HANDLE      (ESP_ERR_NVS_BASE + 0x07)
#define ESP_ERR_NVS_KEY_TOO_LONG        (ESP_ERR_NVS_BASE + 0x09)
#define ESP_ERR_NVS_INVALID_LENGTH      (ESP_ERR_NVS_BASE + 0x0c)
#define ESP_ERR_NVS_NO_FREE_PAGES       (ESP_ERR_NVS_BASE + 0x0d)
#define ESP_ERR_NVS_VALUE_TOO_LONG      (ESP_ERR_NVS_BASE + 0x0e)
#define ESP_ERR_NVS_NEW_VERSION_FOUND   (ESP_ERR_NVS_BASE + 0x10)

inline const char* esp_err_to_name(esp_err_t err) {
    switch (err) {
        case ESP_OK:                       return "ESP_OK";
        case ESP_FAIL:                     return "ESP_FAIL";
        case ESP_ERR_INVALID_ARG:          return "ESP_ERR_INVALID_ARG";
        case ESP_ERR_NOT_FOUND:            return "ESP_ERR_NOT_FOUND";
        case ESP_ERR_NVS_NOT_INITIALIZED:  return "ESP_ERR_NVS_NOT_INITIALIZED";
        case ESP_ERR_NVS_NOT_FOUND:        return "ESP_ERR_NVS_NOT_FOUND";
        case ESP_ERR_NVS_TYPE_MISMATCH:    return "ESP_ERR_NVS_TYPE_MISMATCH";
        case ESP_ERR_NVS_READ_ONLY:        return "ESP_ERR_NVS_READ_ONLY";
        case ESP_ERR_NVS_NOT_ENOUGH_SPACE: return "ESP_ERR_NVS_NOT_ENOUGH_SPACE";
        case ESP_ERR_NVS_INVALID_HANDLE:   return "ESP_ERR_NVS_INVALID_HANDLE";
        case ESP_ERR_NVS_KEY_TOO_LONG:     return "ESP_ERR_NVS_KEY_TOO_LONG";
        case ESP_ERR_NVS_INVALID_LENGTH:   return "ESP_ERR_NVS_INVALID_LENGTH";
        case ESP_ERR_NVS_NO_FREE_PAGES:    return "ESP_ERR_NVS_NO_FREE_PAGES";
        case ESP_ERR_NVS_VALUE_TOO_LONG:   return "ESP_ERR_NVS_VALUE_TOO_LONG";
        case ESP_ERR_NVS_NEW_VERSION_FOUND:return "ESP_ERR_NVS_NEW_VERSION_FOUND";
        default:                           return "UNKNOWN ERROR";
    }
}
