// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/host/shim/nvs.h
#pragma once

// Host shim: an in-memory NVS with the ESP-IDF semantics XeWeCore relies on:
// typed entries (a read with another type is NOT_FOUND, a write with another type
// replaces the entry), 15-char key/namespace limit, read-only opens never create a
// namespace, strings <= 4000 bytes incl. NUL. host_nvs:: lets tests inject faults
// (init result, full partition, commit failure) and count open handles.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "esp_err.h"

typedef uint32_t nvs_handle_t;
typedef enum { NVS_READONLY, NVS_READWRITE } nvs_open_mode_t;

namespace host_nvs {

enum class Type { U8, I8, U16, I16, U32, I32, U64, I64, STR, BLOB };
struct Entry { Type type; std::vector<uint8_t> bytes; };
using Ns = std::map<std::string, Entry>;
struct Open { std::string ns; nvs_open_mode_t mode; };

struct State {
    bool                              inited       = false;
    std::deque<esp_err_t>             init_results;          // consumed by nvs_flash_init; empty -> ESP_OK
    bool                              full         = false;  // sets / namespace creation -> NOT_ENOUGH_SPACE
    esp_err_t                         commit_error = ESP_OK;
    int                               erase_count  = 0;
    nvs_handle_t                      next_handle  = 1;
    std::map<std::string, Ns>         data;
    std::map<nvs_handle_t, Open>      open;
};

inline State& state() { static State s; return s; }
inline void   reset() { state() = State{}; }

inline bool name_ok(const char* n) { return n && *n && std::strlen(n) <= 15; }

inline esp_err_t set(nvs_handle_t h, const char* key, Type t, const void* p, std::size_t n) {
    auto it = state().open.find(h);
    if (it == state().open.end())                  return ESP_ERR_NVS_INVALID_HANDLE;
    if (it->second.mode != NVS_READWRITE)          return ESP_ERR_NVS_READ_ONLY;
    if (!key || !*key)                             return ESP_ERR_NVS_INVALID_NAME;
    if (std::strlen(key) > 15)                     return ESP_ERR_NVS_KEY_TOO_LONG;
    if (t == Type::STR && n > 4000)                return ESP_ERR_NVS_VALUE_TOO_LONG;
    if (state().full)                              return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    const uint8_t* b = static_cast<const uint8_t*>(p);
    state().data[it->second.ns][key] = Entry{t, std::vector<uint8_t>(b, b + n)};
    return ESP_OK;
}

inline esp_err_t get(nvs_handle_t h, const char* key, Type t, void* out, std::size_t* len, bool var) {
    auto it = state().open.find(h);
    if (it == state().open.end())                  return ESP_ERR_NVS_INVALID_HANDLE;
    if (!key || !*key)                             return ESP_ERR_NVS_INVALID_NAME;
    if (std::strlen(key) > 15)                     return ESP_ERR_NVS_KEY_TOO_LONG;
    const Ns& ns = state().data[it->second.ns];
    auto e = ns.find(key);
    if (e == ns.end() || e->second.type != t)      return ESP_ERR_NVS_NOT_FOUND;
    const std::size_t n = e->second.bytes.size();
    if (var) {
        if (out == nullptr) { *len = n; return ESP_OK; }
        if (*len < n)                              return ESP_ERR_NVS_INVALID_LENGTH;
        *len = n;
    }
    if (n) std::memcpy(out, e->second.bytes.data(), n);
    return ESP_OK;
}

} // namespace host_nvs

inline esp_err_t nvs_flash_init() {
    auto& s = host_nvs::state();
    esp_err_t r = ESP_OK;
    if (!s.init_results.empty()) { r = s.init_results.front(); s.init_results.pop_front(); }
    if (r == ESP_OK) s.inited = true;
    return r;
}
inline esp_err_t nvs_flash_deinit() {
    auto& s = host_nvs::state();
    if (!s.inited) return ESP_ERR_NVS_NOT_INITIALIZED;
    s.inited = false;
    s.open.clear();
    return ESP_OK;
}
inline esp_err_t nvs_flash_erase() {
    auto& s = host_nvs::state();
    if (s.inited) return ESP_FAIL;   // IDF refuses to erase a mounted partition
    s.data.clear();
    ++s.erase_count;
    return ESP_OK;
}

inline esp_err_t nvs_open(const char* name, nvs_open_mode_t mode, nvs_handle_t* out) {
    auto& s = host_nvs::state();
    if (!s.inited)                         return ESP_ERR_NVS_NOT_INITIALIZED;
    if (!host_nvs::name_ok(name))          return ESP_ERR_NVS_INVALID_NAME;
    if (!s.data.count(name)) {
        if (mode == NVS_READONLY)          return ESP_ERR_NVS_NOT_FOUND;
        if (s.full)                        return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
        s.data[name];
    }
    *out = s.next_handle++;
    s.open[*out] = host_nvs::Open{name, mode};
    return ESP_OK;
}
inline void nvs_close(nvs_handle_t h) { host_nvs::state().open.erase(h); }
inline esp_err_t nvs_commit(nvs_handle_t h) {
    if (!host_nvs::state().open.count(h)) return ESP_ERR_NVS_INVALID_HANDLE;
    return host_nvs::state().commit_error;
}
inline esp_err_t nvs_erase_key(nvs_handle_t h, const char* key) {
    auto& s = host_nvs::state();
    auto it = s.open.find(h);
    if (it == s.open.end())                return ESP_ERR_NVS_INVALID_HANDLE;
    if (it->second.mode != NVS_READWRITE)  return ESP_ERR_NVS_READ_ONLY;
    return s.data[it->second.ns].erase(key) ? ESP_OK : ESP_ERR_NVS_NOT_FOUND;
}
inline esp_err_t nvs_erase_all(nvs_handle_t h) {
    auto& s = host_nvs::state();
    auto it = s.open.find(h);
    if (it == s.open.end())                return ESP_ERR_NVS_INVALID_HANDLE;
    if (it->second.mode != NVS_READWRITE)  return ESP_ERR_NVS_READ_ONLY;
    s.data[it->second.ns].clear();
    return ESP_OK;
}

#define HOST_NVS_SCALAR(sfx, T, TY)                                                                   \
    inline esp_err_t nvs_set_##sfx(nvs_handle_t h, const char* k, T v) {                              \
        return host_nvs::set(h, k, host_nvs::Type::TY, &v, sizeof v); }                               \
    inline esp_err_t nvs_get_##sfx(nvs_handle_t h, const char* k, T* v) {                             \
        return host_nvs::get(h, k, host_nvs::Type::TY, v, nullptr, false); }
HOST_NVS_SCALAR(u8,  uint8_t,  U8)
HOST_NVS_SCALAR(i8,  int8_t,   I8)
HOST_NVS_SCALAR(u16, uint16_t, U16)
HOST_NVS_SCALAR(i16, int16_t,  I16)
HOST_NVS_SCALAR(u32, uint32_t, U32)
HOST_NVS_SCALAR(i32, int32_t,  I32)
HOST_NVS_SCALAR(u64, uint64_t, U64)
HOST_NVS_SCALAR(i64, int64_t,  I64)
#undef HOST_NVS_SCALAR

inline esp_err_t nvs_set_str(nvs_handle_t h, const char* k, const char* v) {
    return host_nvs::set(h, k, host_nvs::Type::STR, v, std::strlen(v) + 1);
}
inline esp_err_t nvs_get_str(nvs_handle_t h, const char* k, char* out, std::size_t* len) {
    return host_nvs::get(h, k, host_nvs::Type::STR, out, len, true);
}
inline esp_err_t nvs_set_blob(nvs_handle_t h, const char* k, const void* v, std::size_t n) {
    return host_nvs::set(h, k, host_nvs::Type::BLOB, v, n);
}
inline esp_err_t nvs_get_blob(nvs_handle_t h, const char* k, void* out, std::size_t* len) {
    return host_nvs::get(h, k, host_nvs::Type::BLOB, out, len, true);
}
