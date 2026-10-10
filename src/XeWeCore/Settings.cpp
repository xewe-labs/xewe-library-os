// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Settings.cpp

#include "Settings.h"

#include <cstdio>

#include "Nvs.h"
#include "Utils/String.h"


namespace xewe {

namespace {

constexpr char kMask[] = "********";

bool is_int(SettingType t) { return t != SettingType::BOOL && t != SettingType::F32 && t != SettingType::STRING; }

// numeric view of a BOOL / integer / F32 member
double get_number(SettingType t, const void* p) {
    switch (t) {
        case SettingType::BOOL: return *static_cast<const bool*>(p) ? 1.0 : 0.0;
        case SettingType::U8:   return *static_cast<const uint8_t*>(p);
        case SettingType::I8:   return *static_cast<const int8_t*>(p);
        case SettingType::U16:  return *static_cast<const uint16_t*>(p);
        case SettingType::I16:  return *static_cast<const int16_t*>(p);
        case SettingType::U32:  return *static_cast<const uint32_t*>(p);
        case SettingType::I32:  return *static_cast<const int32_t*>(p);
        case SettingType::F32:  return *static_cast<const float*>(p);
        default:                return 0.0;
    }
}

// v is already range-checked against the row, so every cast is exact
void put_number(SettingType t, void* p, double v) {
    switch (t) {
        case SettingType::BOOL: *static_cast<bool*>(p)     = v != 0.0;                  break;
        case SettingType::U8:   *static_cast<uint8_t*>(p)  = static_cast<uint8_t>(v);   break;
        case SettingType::I8:   *static_cast<int8_t*>(p)   = static_cast<int8_t>(v);    break;
        case SettingType::U16:  *static_cast<uint16_t*>(p) = static_cast<uint16_t>(v);  break;
        case SettingType::I16:  *static_cast<int16_t*>(p)  = static_cast<int16_t>(v);   break;
        case SettingType::U32:  *static_cast<uint32_t*>(p) = static_cast<uint32_t>(v);  break;
        case SettingType::I32:  *static_cast<int32_t*>(p)  = static_cast<int32_t>(v);   break;
        case SettingType::F32:  *static_cast<float*>(p)    = static_cast<float>(v);     break;
        default:                                                                        break;
    }
}

std::string number_text(SettingType t, double v) {
    if (t == SettingType::BOOL) return v != 0.0 ? "true" : "false";
    char buf[24];
    if (t == SettingType::F32) std::snprintf(buf, sizeof(buf), "%.7g", v);
    else                       std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
    return buf;
}

bool in_range(const SettingDef& d, const void* p) {
    if (d.type == SettingType::STRING) {
        const double n = static_cast<double>(static_cast<const std::string*>(p)->size());
        return n >= d.min && n <= d.max;
    }
    const double v = get_number(d.type, p);
    return v >= d.min && v <= d.max;   // false for NaN
}

bool parse_bool(std::string_view s, bool& out) {
    const std::string l = str::lower(std::string(s));
    if (l == "1" || l == "true" || l == "on" || l == "yes")  { out = true;  return true; }
    if (l == "0" || l == "false" || l == "off" || l == "no") { out = false; return true; }
    return false;
}

void set_default(const SettingDef& d, void* p) {
    if (d.type == SettingType::STRING) *static_cast<std::string*>(p) = d.def_str ? d.def_str : "";
    else                               put_number(d.type, p, d.def);
}

// typed NVS read into the member, which holds the default: a missing key or another stored type
// leaves it untouched (Nvs::read's default path)
void read_stored(const SettingDef& d, void* p, Nvs& nvs, std::string_view ns) {
    switch (d.type) {
        case SettingType::BOOL:   { auto& m = *static_cast<bool*>(p);        m = nvs.read<bool>(ns, d.key, m);        break; }
        case SettingType::U8:     { auto& m = *static_cast<uint8_t*>(p);     m = nvs.read<uint8_t>(ns, d.key, m);     break; }
        case SettingType::I8:     { auto& m = *static_cast<int8_t*>(p);      m = nvs.read<int8_t>(ns, d.key, m);      break; }
        case SettingType::U16:    { auto& m = *static_cast<uint16_t*>(p);    m = nvs.read<uint16_t>(ns, d.key, m);    break; }
        case SettingType::I16:    { auto& m = *static_cast<int16_t*>(p);     m = nvs.read<int16_t>(ns, d.key, m);     break; }
        case SettingType::U32:    { auto& m = *static_cast<uint32_t*>(p);    m = nvs.read<uint32_t>(ns, d.key, m);    break; }
        case SettingType::I32:    { auto& m = *static_cast<int32_t*>(p);     m = nvs.read<int32_t>(ns, d.key, m);     break; }
        case SettingType::F32:    { auto& m = *static_cast<float*>(p);       m = nvs.read<float>(ns, d.key, m);       break; }
        case SettingType::STRING: { auto& m = *static_cast<std::string*>(p); m = nvs.read<std::string>(ns, d.key, m); break; }
    }
}

bool write_stored(const SettingDef& d, const void* p, Nvs& nvs, std::string_view ns) {
    switch (d.type) {
        case SettingType::BOOL:   return nvs.write<bool>(ns, d.key, *static_cast<const bool*>(p));
        case SettingType::U8:     return nvs.write<uint8_t>(ns, d.key, *static_cast<const uint8_t*>(p));
        case SettingType::I8:     return nvs.write<int8_t>(ns, d.key, *static_cast<const int8_t*>(p));
        case SettingType::U16:    return nvs.write<uint16_t>(ns, d.key, *static_cast<const uint16_t*>(p));
        case SettingType::I16:    return nvs.write<int16_t>(ns, d.key, *static_cast<const int16_t*>(p));
        case SettingType::U32:    return nvs.write<uint32_t>(ns, d.key, *static_cast<const uint32_t*>(p));
        case SettingType::I32:    return nvs.write<int32_t>(ns, d.key, *static_cast<const int32_t*>(p));
        case SettingType::F32:    return nvs.write<float>(ns, d.key, *static_cast<const float*>(p));
        case SettingType::STRING: return nvs.write<std::string>(ns, d.key, *static_cast<const std::string*>(p));
    }
    return false;
}

// JSON string literal: str::escape_json plus \u00XX for control characters
std::string quoted(std::string_view s) {
    std::string out = "\"";
    for (char c : str::escape_json(s)) {
        if (static_cast<unsigned char>(c) < 0x20) {
            char esc[8];
            std::snprintf(esc, sizeof(esc), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
            out += esc;
        } else {
            out += c;
        }
    }
    return out + '"';
}

// a secret string is "set" when non-empty; a secret number when it differs from the default
bool secret_is_set(const SettingDef& d, const void* p) {
    if (d.type == SettingType::STRING) return !static_cast<const std::string*>(p)->empty();
    return get_number(d.type, p) != d.def;
}

const char* type_name(SettingType t) {
    switch (t) {
        case SettingType::BOOL:   return "bool";
        case SettingType::U8:     return "u8";
        case SettingType::I8:     return "i8";
        case SettingType::U16:    return "u16";
        case SettingType::I16:    return "i16";
        case SettingType::U32:    return "u32";
        case SettingType::I32:    return "i32";
        case SettingType::F32:    return "f32";
        case SettingType::STRING: return "str";
    }
    return "?";
}

// [+-]digits[.digits][(e|E)[+-]digits], nothing else (no nan/inf/hex, no spaces): a small parser
// instead of str::parse_float, whose strtod costs ~6 KB of flash in every firmware with a table
bool parse_decimal(std::string_view s, double& out) {
    std::size_t i   = 0;
    const bool  neg = i < s.size() && (s[i] == '-' || s[i] == '+') ? s[i++] == '-' : false;
    uint64_t    mant = 0;
    int         exp10 = 0, digits = 0, kept = 0;
    for (bool frac = false; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '.' && !frac) { frac = true; continue; }
        if (c < '0' || c > '9') break;
        ++digits;
        if (mant == 0 && c == '0') { if (frac) --exp10; continue; }   // leading zeros: not significant
        if (kept < 18) { mant = mant * 10 + uint64_t(c - '0'); ++kept; if (frac) --exp10; }
        else if (!frac) ++exp10;                      // beyond 18 digits: keep the magnitude only
    }
    if (digits == 0) return false;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        const bool eneg = i < s.size() && (s[i] == '-' || s[i] == '+') ? s[i++] == '-' : false;
        int e = 0, edigits = 0;
        for (; i < s.size() && s[i] >= '0' && s[i] <= '9'; ++i, ++edigits) if (e < 1000) e = e * 10 + (s[i] - '0');
        if (edigits == 0) return false;
        exp10 += eneg ? -e : e;
    }
    if (i != s.size()) return false;
    double v = double(mant), scale = 1.0;
    for (int n = exp10 < 0 ? -exp10 : exp10; n > 0 && scale < 1e300; --n) scale *= 10.0;   // exact up to 1e22
    v = exp10 < 0 ? v / scale : v * scale;
    out = neg ? -v : v;
    return true;
}

} // namespace

// ---- the engine: reached only through settings_engine (Module.cpp) ------------------------

namespace detail {

std::string settings_expected(const SettingDef& d) {
    const std::string t = type_name(d.type);
    switch (d.type) {
        case SettingType::BOOL:   return "bool (true/false, on/off, 1/0)";
        case SettingType::STRING: return t + " of " + number_text(SettingType::U32, d.min) + "-" +
                                         number_text(SettingType::U32, d.max) + " chars";
        default:                  return t + " in [" + number_text(d.type, d.min) + ", " + number_text(d.type, d.max) + "]";
    }
}

void settings_load(const Settings& t, Nvs& nvs, std::string_view ns, SerialPort* report) {
    for (const SettingDef& d : t.rows()) {
        void* p = d.ref(t.object());
        set_default(d, p);
        read_stored(d, p, nvs, ns);
        if (!in_range(d, p)) {
            if (report) {
                report->printf("! %.*s/%s: stored value outside %s, using the default",
                               int(ns.size()), ns.data(), d.key, settings_expected(d).c_str());
            }
            set_default(d, p);
        }
    }
}

SettingError settings_set(const Settings& t, Nvs& nvs, std::string_view ns, const SettingDef& d, std::string_view text) {
    void* p = d.ref(t.object());
    if (d.type == SettingType::STRING) {
        const double n = static_cast<double>(text.size());
        if (n < d.min || n > d.max) return SettingError::BAD_VALUE;
        *static_cast<std::string*>(p) = std::string(text);
    } else {
        double v = 0.0;
        if (d.type == SettingType::BOOL) {
            bool b = false;
            if (!parse_bool(text, b)) return SettingError::BAD_VALUE;
            v = b ? 1.0 : 0.0;
        } else if (is_int(d.type)) {
            long long n = 0;
            if (!str::parse_int(text, n)) return SettingError::BAD_VALUE;
            v = static_cast<double>(n);
        } else if (!parse_decimal(text, v)) {
            return SettingError::BAD_VALUE;
        }
        if (!(v >= d.min && v <= d.max)) return SettingError::BAD_VALUE;
        put_number(d.type, p, v);
    }
    return write_stored(d, p, nvs, ns) ? SettingError::NONE : SettingError::NOT_SAVED;
}

std::string settings_value(const Settings& t, const SettingDef& d) {
    const void* p = d.ref(t.object());
    if (d.flags & SettingDef::SECRET) return secret_is_set(d, p) ? kMask : "";
    if (d.type == SettingType::STRING) return *static_cast<const std::string*>(p);
    return number_text(d.type, get_number(d.type, p));
}

std::string settings_schema(const Settings& t, const SettingDef& d) {
    const void*       p          = d.ref(t.object());
    const bool        secret     = d.flags & SettingDef::SECRET;
    const bool        text       = d.type == SettingType::STRING;
    const SettingType bound_type = text ? SettingType::U32 : d.type;   // string bounds are lengths

    std::string s = "\"key\":" + quoted(d.key) + ",\"type\":\"" + type_name(d.type) + '"';
    if (d.type != SettingType::BOOL) {
        s += ",\"min\":" + number_text(bound_type, d.min);
        s += ",\"max\":" + number_text(bound_type, d.max);
    }
    if (!secret) {
        s += ",\"default\":";
        s += text ? quoted(d.def_str ? d.def_str : "") : number_text(d.type, d.def);
        s += ",\"value\":";
        s += text ? quoted(*static_cast<const std::string*>(p)) : number_text(d.type, get_number(d.type, p));
    } else {
        s += ",\"value\":\"";
        s += kMask;
        s += "\",\"secret\":true,\"set\":";
        s += secret_is_set(d, p) ? "true" : "false";
    }
    if (d.doc && *d.doc) s += ",\"doc\":" + quoted(d.doc);
    if (d.flags & SettingDef::RESTART) s += ",\"restart\":true";
    return s;
}

} // namespace detail

void SchemaOut::row(std::string_view fields) {
    std::string line = "{";
    if (!module_id.empty()) {
        line += "\"module\":";
        line += quoted(module_id);
        line += ',';
    }
    line += fields;
    line += '}';
    serial.print(line);   // one line: control characters are escaped, width 0 never wraps
    ++rows;
}

std::string Settings::header(std::string_view core_version, std::string_view device,
                             xewe::span<const std::string_view> module_ids) {
    std::string s = "{\"schema\":1,\"core\":" + quoted(core_version) + ",\"device\":" + quoted(device) + ",\"modules\":[";
    for (std::size_t i = 0; i < module_ids.size(); ++i) {
        if (i) s += ',';
        s += quoted(module_ids[i]);
    }
    return s + "]}";
}

} // namespace xewe
