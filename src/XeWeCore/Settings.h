// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Settings.h
//
// Settings table (core 2.1): a module declares its plain persistent settings as a constexpr table of
// SettingDef rows, and the core provides `$<id> set|get|schema`, the status lines, `$system schema`
// and the load at begin (table default, then the NVS value). Full documentation: doc/os/settings.md.
//
//   xewe::Settings settings() const override {
//       static constexpr xewe::SettingDef table[] = {
//           xewe::setting<&MyModule::number>("number", 0, 1000, 0, "Remembered number"),
//           xewe::setting<&MyModule::beat>  ("beat", true, "Print the heartbeat"),
//           xewe::setting<&MyModule::label> ("label", 16, "my", "Heartbeat prefix"),
//       };
//       return {table, this};
//   }
//
// The key is the NVS key (namespace = module id) and the CLI name: 1-15 characters, no whitespace,
// not a core key (is_enabled, not_first_boot, init_complete). A bad key, a default outside
// [min, max] or bounds outside the member's type stop the build (the table must be constexpr).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

#include "Utils/Span.h"
#include "Serial.h"


namespace xewe {

class Nvs;

enum class SettingType : uint8_t { BOOL, U8, I8, U16, I16, U32, I32, F32, STRING };

struct SettingDef {
    enum Flag : uint8_t {
        SECRET  = 1u << 0,   // value never printed: schema/get/status show "********" (+ "set":bool)
        RESTART = 1u << 1,   // applied and saved at once, takes effect after a restart
    };

    const char*  key;                 // NVS key and CLI name, <= 15 chars
    SettingType  type;                // deduced from the member's type by setting<>()
    uint8_t      flags;               // Flag bits
    void*        (*ref)(void* self);  // the member inside the module object
    double       min;                 // numbers; STRING: length bounds
    double       max;
    double       def;                 // numbers and BOOL (0/1)
    const char*  def_str;             // STRING default
    const char*  doc;                 // optional one-line help, nullptr for none
};

namespace detail {

template <typename P> struct member_of;
template <typename C, typename T> struct member_of<T C::*> { using cls = C; using type = T; };

template <typename T>
constexpr SettingType setting_type_of() {
    if constexpr (std::is_same_v<T, bool>)        return SettingType::BOOL;
    else if constexpr (std::is_same_v<T, float>)  return SettingType::F32;
    else if constexpr (std::is_same_v<T, std::string>) return SettingType::STRING;
    else if constexpr (std::is_integral_v<T> && sizeof(T) == 1) return std::is_signed_v<T> ? SettingType::I8  : SettingType::U8;
    else if constexpr (std::is_integral_v<T> && sizeof(T) == 2) return std::is_signed_v<T> ? SettingType::I16 : SettingType::U16;
    else if constexpr (std::is_integral_v<T> && sizeof(T) == 4) return std::is_signed_v<T> ? SettingType::I32 : SettingType::U32;
    else {
        static_assert(!sizeof(T), "setting<>: member must be bool, a 8/16/32-bit integer, float or std::string");
        return SettingType::BOOL;
    }
}

template <auto M>
void* member_ref(void* self) {
    using C = typename member_of<decltype(M)>::cls;
    return &(static_cast<C*>(self)->*M);
}

constexpr bool same(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

constexpr bool key_ok(const char* key) {
    if (key == nullptr || *key == '\0') return false;
    std::size_t n = 0;
    for (const char* p = key; *p; ++p, ++n) {
        if (*p == ' ' || *p == '\t' || *p == '"' || *p == '\\') return false;
    }
    return n <= 15 && !same(key, "is_enabled") && !same(key, "not_first_boot") && !same(key, "init_complete");
}

constexpr double type_min(SettingType t) {
    switch (t) {
        case SettingType::I8:  return -128.0;
        case SettingType::I16: return -32768.0;
        case SettingType::I32: return -2147483648.0;
        case SettingType::F32: return -3.402823466e38;
        default:               return 0.0;
    }
}

constexpr double type_max(SettingType t) {
    switch (t) {
        case SettingType::BOOL:   return 1.0;
        case SettingType::U8:     return 255.0;
        case SettingType::I8:     return 127.0;
        case SettingType::U16:    return 65535.0;
        case SettingType::I16:    return 32767.0;
        case SettingType::U32:    return 4294967295.0;
        case SettingType::I32:    return 2147483647.0;
        case SettingType::F32:    return 3.402823466e38;
        case SettingType::STRING: return 4000.0;   // NVS string limit (incl. NUL)
    }
    return 0.0;
}

// Never defined. A constexpr table whose row reaches one of these does not compile, and the
// error names the problem ("call to non-'constexpr' function 'setting_key_invalid'").
void setting_key_invalid();     // empty, > 15 chars, whitespace/quote/backslash, or a core key
void setting_range_invalid();   // min > def, def > max, or bounds outside the member's type

constexpr SettingDef make_setting(const char* key, SettingType type, void* (*ref)(void*), double min, double max,
                                  double def, const char* def_str, const char* doc, uint8_t flags) {
    if (!key_ok(key)) setting_key_invalid();
    if (!(min <= def && def <= max && min >= type_min(type) && max <= type_max(type))) setting_range_invalid();
    return SettingDef{key, type, flags, ref, min, max, def, def_str, doc};
}

constexpr std::size_t length(const char* s) {
    std::size_t n = 0;
    while (s != nullptr && s[n]) ++n;
    return n;
}

template <auto M> using member_t = typename member_of<decltype(M)>::type;

} // namespace detail

// numbers: bool excluded, integers and float; min <= def <= max, all inside the member's type
template <auto M, std::enable_if_t<!std::is_same_v<detail::member_t<M>, bool> &&
                                   !std::is_same_v<detail::member_t<M>, std::string>, int> = 0>
constexpr SettingDef setting(const char* key, double min, double max, double def,
                             const char* doc = nullptr, uint8_t flags = 0) {
    return detail::make_setting(key, detail::setting_type_of<detail::member_t<M>>(), &detail::member_ref<M>,
                                min, max, def, nullptr, doc, flags);
}

// bool
template <auto M, std::enable_if_t<std::is_same_v<detail::member_t<M>, bool>, int> = 0>
constexpr SettingDef setting(const char* key, bool def, const char* doc = nullptr, uint8_t flags = 0) {
    return detail::make_setting(key, SettingType::BOOL, &detail::member_ref<M>, 0, 1, def ? 1 : 0, nullptr, doc, flags);
}

// std::string: up to max_len characters (<= 4000), default def
template <auto M, std::enable_if_t<std::is_same_v<detail::member_t<M>, std::string>, int> = 0>
constexpr SettingDef setting(const char* key, std::size_t max_len, const char* def,
                             const char* doc = nullptr, uint8_t flags = 0) {
    return detail::make_setting(key, SettingType::STRING, &detail::member_ref<M>, 0, double(max_len),
                                double(detail::length(def)), def, doc, flags);
}

// Writes schema rows as JSON Lines: row("\"key\":\"x\",...") prints {"module":"<id>","key":"x",...}
// when made for $system schema (module id given) and {"key":"x",...} for $<id> schema.
class SchemaOut {
public:
                SchemaOut (SerialPort& serial, std::string_view module_id = {})
        : serial(serial), module_id(module_id) {}

    void        row       (std::string_view fields);
    void        set_module(std::string_view id) { module_id = id; }
    std::size_t count     () const { return rows; }

private:
    SerialPort&      serial;
    std::string_view module_id;
    std::size_t      rows = 0;
};

class Settings;
class Module;

enum class SettingError : uint8_t { NONE, UNKNOWN_KEY, BAD_VALUE, NOT_SAVED };

// The engine, reached only through this table of function pointers, which only the Settings
// constructor below names: a firmware in which no module returns a table references none of it,
// and the linker drops the engine and the `$<id> set|get|schema` glue. Defined in Module.cpp.
struct SettingsEngine {
    void         (*load)    (const Settings&, Nvs&, std::string_view ns, SerialPort* report);
    SettingError (*set)     (const Settings&, Nvs&, std::string_view ns, const SettingDef& row, std::string_view text);
    std::string  (*value)   (const Settings&, const SettingDef& row);
    std::string  (*schema)  (const Settings&, const SettingDef& row);
    std::string  (*expected)(const SettingDef& row);
    void         (*attach)  (Module& module);   // Module::begin: load, then register the commands
};
extern const SettingsEngine settings_engine;

namespace detail {   // the engine functions (Settings.cpp); call them through Settings
void         settings_load    (const Settings&, Nvs&, std::string_view ns, SerialPort* report);
SettingError settings_set     (const Settings&, Nvs&, std::string_view ns, const SettingDef& row, std::string_view text);
std::string  settings_value   (const Settings&, const SettingDef& row);
std::string  settings_schema  (const Settings&, const SettingDef& row);
std::string  settings_expected(const SettingDef& row);
} // namespace detail

// What Module::settings() returns: the table and the object its rows point into (`this`).
// Rows must name members of that same class. Empty by default: the module opts out at no cost.
class Settings {
public:
    using SetError = SettingError;

    constexpr   Settings  () = default;
    template <typename C, std::size_t N>
    constexpr   Settings  (const SettingDef (&table)[N], const C* owner)
        : defs(table, N), self(const_cast<C*>(owner)), engine(&settings_engine) {}
                Settings  (xewe::span<const SettingDef> table, void* owner)
        : defs(table), self(owner), engine(&settings_engine) {}

    bool                          empty    () const { return defs.empty() || self == nullptr || engine == nullptr; }
    xewe::span<const SettingDef>  rows     () const { return defs; }
    void*                         object   () const { return self; }
    const SettingDef*             find     (std::string_view key) const {
        for (const SettingDef& d : defs) if (key == d.key) return &d;
        return nullptr;
    }

    // every row: table default, then the stored NVS value when it has the row's type and is in
    // range; a stored value out of range keeps the default and prints one `!` line to `report`
    void        load      (Nvs& nvs, std::string_view ns, SerialPort* report = nullptr) const {
        if (!empty()) engine->load(*this, nvs, ns, report);
    }
    // parse + validate `text`, assign, persist under ns/key; NOT_SAVED: applied but the NVS write failed
    SetError    set       (Nvs& nvs, std::string_view ns, std::string_view key, std::string_view text,
                           const SettingDef** row = nullptr) const {
        const SettingDef* d = empty() ? nullptr : find(key);
        if (row) *row = d;
        return d ? engine->set(*this, nvs, ns, *d, text) : SetError::UNKNOWN_KEY;
    }
    // current value as text (strings raw); masked "********" for a set SECRET ("" when unset)
    std::string value     (const SettingDef& def) const { return empty() ? std::string() : engine->value(*this, def); }
    // schema fields for SchemaOut::row: key, type, min, max, default, value, secret/set, doc, restart
    std::string schema    (const SettingDef& def) const { return empty() ? std::string() : engine->schema(*this, def); }
    // what `set` accepts, for error messages: "u16 in [0, 1000]", "bool (true/false, on/off, 1/0)", ...
    std::string expected  (const SettingDef& def) const { return empty() ? std::string() : engine->expected(def); }

    // Module::begin only: load, then `$<id> set|get|schema` (the ones the module has not taken)
    void        attach    (Module& module) const { if (!empty() && engine->attach) engine->attach(module); }

    // `$system schema` header line: {"schema":1,"core":"<ver>","device":"<name>","modules":[...]}
    static std::string header(std::string_view core_version, std::string_view device,
                              xewe::span<const std::string_view> module_ids);

private:
    xewe::span<const SettingDef> defs;
    void*                        self   = nullptr;
    const SettingsEngine*        engine = nullptr;
};

} // namespace xewe
