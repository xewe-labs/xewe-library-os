// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Testing.cpp
//
// The `$test` CLI group (see Testing.h). Everything below is compiled only with XEWE_TESTING.
// Output is one `key=value` record per line so tests/board can parse it; every command
// validates its arguments and answers `error=<reason>` instead of crashing on garbage.
// NVS writes and deletes are restricted to namespaces starting with "xt" so a test can never
// touch provisioning or module data.

#include "Testing.h"

#ifdef XEWE_TESTING

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "XeWeOs.h"

namespace xewe::testing {
namespace {

using args_t = xewe::span<const std::string>;

constexpr const char* kGroup   = "test";
constexpr const char* kNsAllow = "xt";

// printable form of an arbitrary byte string: \xNN for control/non-ASCII, \" and \\ escaped
std::string quote(std::string_view s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c < 0x20 || c >= 0x7F) {
            out += str::format("\\x%02X", c);
        } else {
            out += static_cast<char>(c);
        }
    }
    return out + "\"";
}

std::string hex(std::string_view s) {
    return str::to_hex(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

bool parse_u32(const std::string& s, uint32_t lo, uint32_t hi, uint32_t& out) {
    return str::parse_int(s, out) && out >= lo && out <= hi;
}

// ---- FlexData probe types ------------------------------------------------

struct ProbeInner : FlexData<ProbeInner> {
    int32_t     a = 0;
    std::string s;
    static constexpr auto fields() {
        return std::make_tuple(fld("a", &ProbeInner::a), fld("s", &ProbeInner::s));
    }
};

struct Probe : FlexData<Probe> {
    bool                    b = false;
    int32_t                 i = 0;
    uint32_t                u = 0;
    int64_t                 l = 0;
    float                   f = 0.0f;
    double                  d = 0.0;
    std::string             s;
    std::vector<int32_t>    v;
    ProbeInner              in;
    std::vector<ProbeInner> vi;
    static constexpr auto fields() {
        return std::make_tuple(fld("b", &Probe::b), fld("i", &Probe::i), fld("u", &Probe::u),
                               fld("l", &Probe::l), fld("f", &Probe::f), fld("d", &Probe::d),
                               fld("s", &Probe::s), fld("v", &Probe::v), fld("in", &Probe::in),
                               fld("vi", &Probe::vi));
    }
};

const char* json_type(JsonVariantConst v) {
    if (v.isNull())                 return "null";
    if (v.is<bool>())               return "bool";
    if (v.is<long long>())          return "int";
    if (v.is<unsigned long long>()) return "uint";
    if (v.is<double>())             return "float";
    if (v.is<const char*>())        return "string";
    if (v.is<JsonArrayConst>())     return "array";
    if (v.is<JsonObjectConst>())    return "object";
    return "unknown";
}

// parse `json` into a Probe and report: parse error, input types, canonical JSON, blob round trip
void flex_report(SerialPort& out, std::string_view json) {
    JsonDocument               doc;
    const DeserializationError err = deserializeJson(doc, json.data(), json.size());
    out.print(std::string("parse=") + err.c_str());

    std::string types;
    if (!err && doc.is<JsonObjectConst>()) {
        for (JsonPairConst kv : doc.as<JsonObjectConst>()) {
            if (!types.empty()) types += ',';
            types += kv.key().c_str();
            types += ':';
            types += json_type(kv.value());
        }
    } else if (!err) {
        types = std::string("<top>:") + json_type(doc.as<JsonVariantConst>());
    }
    out.print("types=" + types);

    const Probe       p     = Probe::from_json(json);
    const std::string canon = p.as_json_str();
    out.print("json=" + canon);

    // idempotence: the canonical output parses back to itself
    const std::string again = Probe::from_json(canon).as_json_str();
    out.print(str::format("stable=%d", again == canon ? 1 : 0));

    const std::vector<uint8_t> blob = p.to_blob();
    Probe                      q;
    const bool                 ok   = q.from_blob(blob);
    out.print(str::format("blob_len=%u blob_ok=%d blob_equal=%d", static_cast<unsigned>(blob.size()),
                          ok ? 1 : 0, (ok && q.as_json_str() == canon) ? 1 : 0));
}

// built-in malformed inputs (some are longer than a CLI line can carry)
void flex_bad(SerialPort& out, const std::string& name) {
    if (name == "truncated")  return flex_report(out, R"({"i":1,"s":"ab)");
    if (name == "trailing")   return flex_report(out, R"({"i":1} garbage)");
    if (name == "empty")      return flex_report(out, "");
    if (name == "null")       return flex_report(out, "null");
    if (name == "array")      return flex_report(out, "[1,2,3]");
    if (name == "wrongtypes") return flex_report(out, R"({"b":"yes","i":"abc","v":{"x":1},"in":[1],"s":5})");
    if (name == "hugenum")    return flex_report(out, R"({"i":99999999999999999999,"u":-1,"l":1e30,"f":1e400,"d":-1e400})");
    if (name == "unicode")    return flex_report(out, R"({"s":"é😀\u0000x"})");
    // Q4: rejected with "! Probe.b: expected bool, got string"; b stays false, i is applied
    if (name == "boolstr")    return flex_report(out, R"({"b":"false","i":3})");
    if (name == "deep") {
        std::string j;
        for (int k = 0; k < 200; ++k) j += "{\"in\":";
        j += "1";
        for (int k = 0; k < 200; ++k) j += "}";
        return flex_report(out, j);
    }
    if (name == "longstr") return flex_report(out, "{\"s\":\"" + std::string(3000, 'x') + "\"}");

    // corrupted blobs: must be rejected, never crash
    std::vector<uint8_t> blob = Probe{}.to_blob();
    if (name == "blob_version") {
        blob[0] = 0x7F;
    } else if (name == "blob_short") {
        blob.resize(blob.size() / 2);
    } else if (name == "blob_strlen") {
        // Probe::s length prefix sits after b(1) i(4) u(4) l(8) f(4) d(8) behind the version byte
        const std::size_t at = 1 + 1 + 4 + 4 + 8 + 4 + 8;
        blob[at] = blob[at + 1] = blob[at + 2] = blob[at + 3] = 0xFF;
    } else if (name == "blob_veccount") {
        // Probe::v element count right after the empty string's 4-byte length
        const std::size_t at = 1 + 1 + 4 + 4 + 8 + 4 + 8 + 4;
        blob[at] = blob[at + 1] = blob[at + 2] = blob[at + 3] = 0xFF;
    } else if (name == "blob_empty") {
        blob.clear();
    } else {
        out.print("error=unknown case");
        return;
    }
    Probe      p;
    const bool ok = p.from_blob(blob);
    out.print(str::format("blob_ok=%d", ok ? 1 : 0));
}

// ---- NVS -----------------------------------------------------------------

bool ns_allowed(const std::string& ns) { return ns.rfind(kNsAllow, 0) == 0; }

bool nvs_write(Nvs& nvs, const std::string& ns, const std::string& key, const std::string& type,
               const std::string& value, std::string& err) {
    if (type == "str") return nvs.write<std::string>(ns, key, value);
    if (type == "bool") {
        const std::string v = str::to_lower(value);
        if (v != "0" && v != "1" && v != "true" && v != "false") return (err = "bad bool", false);
        return nvs.write<bool>(ns, key, v == "1" || v == "true");
    }
    if (type == "i32") {
        int32_t x = 0;
        if (!str::parse_int(value, x)) return (err = "bad i32", false);
        return nvs.write<int32_t>(ns, key, x);
    }
    if (type == "u32") {
        uint32_t x = 0;
        if (!str::parse_int(value, x)) return (err = "bad u32", false);
        return nvs.write<uint32_t>(ns, key, x);
    }
    if (type == "i64") {
        int64_t x = 0;
        if (!str::parse_int(value, x)) return (err = "bad i64", false);
        return nvs.write<int64_t>(ns, key, x);
    }
    if (type == "float") {
        float x = 0;
        if (!str::parse_float(value, x)) return (err = "bad float", false);
        return nvs.write<float>(ns, key, x);
    }
    if (type == "double") {
        double x = 0;
        if (!str::parse_float(value, x)) return (err = "bad double", false);
        return nvs.write<double>(ns, key, x);
    }
    err = "unknown type";
    return false;
}

// read twice with different defaults: a value that follows the default is missing
bool nvs_read(Nvs& nvs, const std::string& ns, const std::string& key, const std::string& type,
              std::string& value, bool& found, std::string& err) {
    if (type == "str") {
        const std::string a = nvs.read<std::string>(ns, key, "\x01");
        const std::string b = nvs.read<std::string>(ns, key, "\x02");
        found = a == b;
        value = found ? a : "";
        return true;
    }
    if (type == "bool") {
        const bool a = nvs.read<bool>(ns, key, false), b = nvs.read<bool>(ns, key, true);
        found = a == b;
        value = a ? "1" : "0";
        return true;
    }
    if (type == "i32") {
        const int32_t a = nvs.read<int32_t>(ns, key, 1), b = nvs.read<int32_t>(ns, key, 2);
        found = a == b;
        value = std::to_string(a);
        return true;
    }
    if (type == "u32") {
        const uint32_t a = nvs.read<uint32_t>(ns, key, 1), b = nvs.read<uint32_t>(ns, key, 2);
        found = a == b;
        value = std::to_string(a);
        return true;
    }
    if (type == "i64") {
        const int64_t a = nvs.read<int64_t>(ns, key, 1), b = nvs.read<int64_t>(ns, key, 2);
        found = a == b;
        value = std::to_string(a);
        return true;
    }
    if (type == "float") {
        const float a = nvs.read<float>(ns, key, 1.0f), b = nvs.read<float>(ns, key, 2.0f);
        found = a == b;
        value = str::format("%.9g", static_cast<double>(a));
        return true;
    }
    if (type == "double") {
        const double a = nvs.read<double>(ns, key, 1.0), b = nvs.read<double>(ns, key, 2.0);
        found = a == b;
        value = str::format("%.17g", a);
        return true;
    }
    err = "unknown type";
    return false;
}

// n write/read/verify cycles plus the edge cases; prints one `fail=` line per failed check
void nvs_stress(SerialPort& out, Nvs& nvs, uint32_t n) {
    const char* ns  = "xtstress";
    const char* ns2 = "xtstress2";
    uint32_t    pass = 0, fail = 0;
    auto        check = [&](bool ok, const std::string& what) {
        if (ok) {
            ++pass;
        } else {
            ++fail;
            out.print("fail=" + what);
        }
    };

    nvs.reset_ns(ns);
    nvs.reset_ns(ns2);

    for (uint32_t k = 0; k < n; ++k) {
        const bool        b = (k % 2) == 0;
        const int32_t     i = static_cast<int32_t>(k * 7919u) - 1000000;
        const float       f = static_cast<float>(k) * 0.5f - 3.25f;
        const std::string s = "v" + std::to_string(k) + std::string(k % 40, 'z');
        check(nvs.write<bool>(ns, "b", b) && nvs.read<bool>(ns, "b", !b) == b, str::format("bool k=%u", k));
        check(nvs.write<int32_t>(ns, "i", i) && nvs.read<int32_t>(ns, "i", i + 1) == i, str::format("i32 k=%u", k));
        check(nvs.write<float>(ns, "f", f) && nvs.read<float>(ns, "f", f + 1) == f, str::format("float k=%u", k));
        check(nvs.write<std::string>(ns, "s", s) && nvs.read<std::string>(ns, "s") == s, str::format("str k=%u", k));
    }

    // overwrite with a different type under the same key: the old typed read must not see it
    check(nvs.write<std::string>(ns, "ow", "first") && nvs.write<std::string>(ns, "ow", "second") &&
          nvs.read<std::string>(ns, "ow") == "second", "overwrite str");
    check(nvs.write<int32_t>(ns, "ow2", 5) && nvs.write<int32_t>(ns, "ow2", -5) &&
          nvs.read<int32_t>(ns, "ow2") == -5, "overwrite i32");

    // key length: 15 is the ESP NVS maximum, 16 must be rejected (and reported, not crash)
    const std::string k15(15, 'k'), k16(16, 'k');
    check(nvs.write<int32_t>(ns, k15, 15) && nvs.read<int32_t>(ns, k15) == 15, "key15");
    check(!nvs.write<int32_t>(ns, k16, 16), "key16 rejected");
    check(nvs.read<int32_t>(ns, k16, -7) == -7, "key16 read default");
    check(!nvs.write<int32_t>(std::string(16, 'x'), "k", 1), "ns16 rejected");
    check(!nvs.write<int32_t>(ns, "", 1), "empty key rejected");

    // string values: a long one fits, one past the ESP NVS limit (4000 bytes with NUL) fails
    const std::string big(1984, 'L');
    check(nvs.write<std::string>(ns, "big", big) && nvs.read<std::string>(ns, "big") == big, "str1984");
    nvs.remove(ns, "big");
    check(!nvs.write<std::string>(ns, "huge", std::string(4001, 'H')), "str4001 rejected");
    check(nvs.read<std::string>(ns, "huge", "none") == "none", "str4001 absent");

    // namespace isolation
    check(nvs.write<int32_t>(ns, "iso", 1) && nvs.write<int32_t>(ns2, "iso", 2) &&
          nvs.read<int32_t>(ns, "iso") == 1 && nvs.read<int32_t>(ns2, "iso") == 2, "isolation");
    nvs.reset_ns(ns2);
    check(nvs.read<int32_t>(ns, "iso") == 1 && nvs.read<int32_t>(ns2, "iso", -1) == -1, "reset_ns scope");

    // remove
    nvs.remove(ns, "iso");
    check(nvs.read<int32_t>(ns, "iso", -1) == -1, "remove");

    nvs.reset_ns(ns);
    check(nvs.read<int32_t>(ns, k15, -1) == -1, "reset_ns");
    out.print(str::format("stress n=%u pass=%u fail=%u", n, pass, fail));
}

// ---- utils ---------------------------------------------------------------

void run_validate(SerialPort& out, const std::string& kind, const std::string& v) {
    auto report = [&](bool ok, const std::string& value) {
        out.print(str::format("ok=%d value=", ok ? 1 : 0) + quote(value));
    };
    if (kind == "parse_int") {
        long long x = 0;
        const bool ok = str::parse_int(v, x);
        return report(ok, ok ? std::to_string(x) : "");
    }
    if (kind == "parse_u8") {
        uint8_t x = 0;
        const bool ok = str::parse_int(v, x);
        return report(ok, ok ? std::to_string(x) : "");
    }
    if (kind == "parse_u32") {
        uint32_t x = 0;
        const bool ok = str::parse_int(v, x);
        return report(ok, ok ? std::to_string(x) : "");
    }
    if (kind == "parse_i64") {
        int64_t x = 0;
        const bool ok = str::parse_int(v, x);
        return report(ok, ok ? std::to_string(x) : "");
    }
    if (kind == "parse_float") {
        double x = 0;
        const bool ok = str::parse_float(v, x);
        return report(ok, ok ? str::format("%.17g", x) : "");
    }
    if (kind == "gmt") {
        std::string g;
        const bool ok = str::parse_gmt_offset(v, g);
        return report(ok, ok ? g : "");
    }
    if (kind == "day") {
        uint8_t d = 0;
        const bool ok = str::parse_day(v, d);
        return report(ok, ok ? std::to_string(d) : "");
    }
    if (kind == "time") {
        uint16_t m = 0;
        const bool ok = str::parse_time(v, m);
        return report(ok, ok ? std::to_string(m) : "");
    }
    if (kind == "extract") {
        const auto cmds = str::extract_commands(v);
        std::string all;
        for (const auto& c : cmds) all += (all.empty() ? "" : "|") + c;
        return report(true, std::to_string(cmds.size()) + ":" + all);
    }
    if (kind == "escape_json") return report(true, str::escape_json(v));
    if (kind == "capitalize")  return report(true, str::capitalize(v));
    if (kind == "lower")       return report(true, str::lower(v));
    if (kind == "upper")       return report(true, str::upper(v));
    out.print("error=unknown kind");
}

// validate<T>(value, lo, hi) for each supported T
void run_validate_range(SerialPort& out, const std::string& kind, const std::string& lo_s,
                    const std::string& hi_s, const std::string& v) {
    auto report = [&](bool ok, const std::string& value) {
        out.print(str::format("ok=%d value=", ok ? 1 : 0) + quote(value));
    };
    if (kind == "str") {
        uint32_t lo = 0, hi = 0;
        if (!str::parse_int(lo_s, lo) || !str::parse_int(hi_s, hi)) return out.print("error=bad bounds");
        const auto r = xewe::validate<std::string>(v, lo, hi);
        return report(r.has_value(), r.value_or(""));
    }
    if (kind == "int" || kind == "i8" || kind == "i64") {
        long long lo = 0, hi = 0;
        if (!str::parse_int(lo_s, lo) || !str::parse_int(hi_s, hi)) return out.print("error=bad bounds");
        if (kind == "i8") {
            const auto r = xewe::validate<int8_t>(v, lo, hi);
            return report(r.has_value(), r ? std::to_string(*r) : "");
        }
        if (kind == "i64") {
            const auto r = xewe::validate<int64_t>(v, lo, hi);
            return report(r.has_value(), r ? std::to_string(*r) : "");
        }
        const auto r = xewe::validate<int>(v, lo, hi);
        return report(r.has_value(), r ? std::to_string(*r) : "");
    }
    if (kind == "u8" || kind == "u32") {
        unsigned long long lo = 0, hi = 0;
        if (!str::parse_int(lo_s, lo) || !str::parse_int(hi_s, hi)) return out.print("error=bad bounds");
        if (kind == "u8") {
            const auto r = xewe::validate<uint8_t>(v, lo, hi);
            return report(r.has_value(), r ? std::to_string(*r) : "");
        }
        const auto r = xewe::validate<uint32_t>(v, lo, hi);
        return report(r.has_value(), r ? std::to_string(*r) : "");
    }
    if (kind == "float") {
        double lo = 0, hi = 0;
        if (!str::parse_float(lo_s, lo) || !str::parse_float(hi_s, hi)) return out.print("error=bad bounds");
        const auto r = xewe::validate<float>(v, lo, hi);
        return report(r.has_value(), r ? str::format("%.9g", static_cast<double>(*r)) : "");
    }
    out.print("error=unknown kind");
}

// ---- registration ----------------------------------------------------------

void add(Os& os, const char* name, const char* usage, std::size_t argc,
         std::function<void(args_t)> fn) {
    os.cli.add_command(kGroup, Command{name, "test hook", std::string("$test ") + usage, argc, std::move(fn)});
}

void add_echo(Os& os, const char* name, std::size_t argc) {
    add(os, name, name, argc, [&os](args_t args) {
        os.serial.print(str::format("argc=%u", static_cast<unsigned>(args.size())));
        for (std::size_t k = 0; k < args.size(); ++k) {
            os.serial.print(str::format("arg%u len=%u hex=%s text=", static_cast<unsigned>(k),
                                        static_cast<unsigned>(args[k].size()), hex(args[k]).c_str()) +
                            quote(args[k]));
        }
    });
}

} // namespace

void register_commands(Os& os) {
    os.cli.add_group(kGroup, "Test hooks (XEWE_TESTING)");
    SerialPort& out = os.serial;
    Nvs&        nvs = os.nvs;

    add(os, "nvs", "nvs <ns> <key> <type> <value>", 4, [&out, &nvs](args_t a) {
        if (!ns_allowed(a[0])) return out.print("error=namespace must start with xt");
        std::string err;
        const bool  ok = nvs_write(nvs, a[0], a[1], a[2], a[3], err);
        out.print(err.empty() ? str::format("write=%d", ok ? 1 : 0) : "error=" + err);
    });
    add(os, "nvs_read", "nvs_read <ns> <key> <type>", 3, [&out, &nvs](args_t a) {
        std::string value, err;
        bool        found = false;
        if (!nvs_read(nvs, a[0], a[1], a[2], value, found, err)) return out.print("error=" + err);
        out.print(str::format("found=%d value=", found ? 1 : 0) + quote(value));
    });
    add(os, "nvs_del", "nvs_del <ns> <key>", 2, [&out, &nvs](args_t a) {
        if (!ns_allowed(a[0])) return out.print("error=namespace must start with xt");
        nvs.remove(a[0], a[1]);
        out.print("deleted=1");
    });
    add(os, "nvs_reset", "nvs_reset <ns>", 1, [&out, &nvs](args_t a) {
        if (!ns_allowed(a[0])) return out.print("error=namespace must start with xt");
        nvs.reset_ns(a[0]);
        out.print("reset=1");
    });
    add(os, "nvs_stress", "nvs_stress <n 1..200>", 1, [&out, &nvs](args_t a) {
        uint32_t n = 0;
        if (!parse_u32(a[0], 1, 200, n)) return out.print("error=n must be 1..200");
        nvs_stress(out, nvs, n);
    });

    add(os, "flex", "flex <json>", 1, [&out](args_t a) { flex_report(out, a[0]); });
    add(os, "flex_bad", "flex_bad <case>", 1, [&out](args_t a) { flex_bad(out, a[0]); });

    add(os, "validate", "validate <kind> <value>", 2, [&out](args_t a) { run_validate(out, a[0], a[1]); });
    add(os, "validate_range", "validate_range <kind> <lo> <hi> <value>", 4,
        [&out](args_t a) { run_validate_range(out, a[0], a[1], a[2], a[3]); });

    // prompts: timeout 1..60000 ms (never 0: that would block until input), attempts 0..10
    add(os, "prompt_yn", "prompt_yn <timeout_ms> <retries>", 2, [&out](args_t a) {
        uint32_t t = 0, r = 0;
        if (!parse_u32(a[0], 1, 60000, t) || !parse_u32(a[1], 0, 10, r)) return out.print("error=bad args");
        bool       ok = false;
        const bool v  = out.get_yn("test yn?", r, t, false, ok);
        out.print(str::format("result=%d success=%d", v ? 1 : 0, ok ? 1 : 0));
    });
    add(os, "prompt_str", "prompt_str <min> <max> <timeout_ms>", 3, [&out](args_t a) {
        uint32_t lo = 0, hi = 0, t = 0;
        if (!parse_u32(a[0], 0, 254, lo) || !parse_u32(a[1], 0, 254, hi) || !parse_u32(a[2], 1, 60000, t)) {
            return out.print("error=bad args");
        }
        bool              ok = false;
        const std::string v  = out.get_string("test str?", lo, hi, 1, t, "<default>", ok);
        out.print("result=" + quote(v) + str::format(" success=%d", ok ? 1 : 0));
    });
    add(os, "prompt_int", "prompt_int <lo> <hi> <timeout_ms>", 3, [&out](args_t a) {
        int32_t  lo = 0, hi = 0;
        uint32_t t  = 0;
        if (!str::parse_int(a[0], lo) || !str::parse_int(a[1], hi) || !parse_u32(a[2], 1, 60000, t)) {
            return out.print("error=bad args");
        }
        bool      ok = false;
        const int v  = out.get_int("test int?", lo, hi, 2, t, -12345, ok);
        out.print(str::format("result=%d success=%d", v, ok ? 1 : 0));
    });

    add(os, "heap", "heap", 0, [&out](args_t) {
        out.print(str::format("free=%u min_free=%u largest=%u", static_cast<unsigned>(ESP.getFreeHeap()),
                              static_cast<unsigned>(ESP.getMinFreeHeap()),
                              static_cast<unsigned>(ESP.getMaxAllocHeap())));
    });
    add(os, "uptime", "uptime", 0, [&out](args_t) {
        out.print(str::format("uptime_ms=%lu", static_cast<unsigned long>(millis())));
    });
    add(os, "sleep_ms", "sleep_ms <0..10000>", 1, [&out](args_t a) {
        uint32_t n = 0;
        if (!parse_u32(a[0], 0, 10000, n)) return out.print("error=n must be 0..10000");
        delay(n);
        out.print(str::format("slept_ms=%u", n));
    });

    // the CLI matches arity exactly, so the tokenizer probe comes in fixed arities
    add_echo(os, "echo0", 0);
    add_echo(os, "echo", 1);
    add_echo(os, "echo2", 2);
    add_echo(os, "echo3", 3);
    add_echo(os, "echo5", 5);
}

} // namespace xewe::testing

#endif // XEWE_TESTING
