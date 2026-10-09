// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/FlexData.h
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>
#include <Arduino.h>
#include <ArduinoJson.h>


namespace xewe {
template <typename Derived>
struct FlexData; // forward decl so the codec / converters can detect it
} // namespace xewe

// ----------------------------------------------------------------------------
// std::vector<T> <-> JSON (works for scalars, strings, AND nested FlexData).
// ----------------------------------------------------------------------------
namespace ArduinoJson {
template <typename T>
struct Converter<std::vector<T>> {
    static void toJson(const std::vector<T>& src, JsonVariant dst) {
        JsonArray a = dst.to<JsonArray>();
        for (const T& x : src) a.add(x);
    }
    static std::vector<T> fromJson(JsonVariantConst src) {
        std::vector<T> dst;
        for (JsonVariantConst x : src.as<JsonArrayConst>()) dst.push_back(x.as<T>());
        return dst;
    }
    static bool checkJson(JsonVariantConst src) { return src.is<JsonArrayConst>(); }
};
} // namespace ArduinoJson

namespace xewe {

// ----------------------------------------------------------------------------
// Binary codec: little-endian, length-prefixed strings/arrays. Nested FlexData
// structs are written field-by-field (no per-element version byte).
// ----------------------------------------------------------------------------
struct BlobWriter {
    std::vector<uint8_t>     bytes;
    void                 raw(const void* p, size_t n) {
        const uint8_t* b = static_cast<const uint8_t*>(p);
        bytes.insert(bytes.end(), b, b + n);
    }
};

struct BlobReader {
    const uint8_t*           p;
    const uint8_t*           end;
    bool                     ok = true;
    bool           take(void* out, size_t n) {
        if (!ok || static_cast<size_t>(end - p) < n) {
            ok = false;
            return false;
        }
        std::memcpy(out, p, n);
        p += n;
        return true;
    }
};

// -- writers --
template <typename T>
void blob_write(BlobWriter& w,
                const T& v) {
    if constexpr (std::is_arithmetic_v<T>) {
        w.raw(&v, sizeof(T));
    } else if constexpr (std::is_base_of_v<FlexData<T>, T>) {
        v.write_fields(w); // nested struct
    } else {
        static_assert(sizeof(T) == 0, "no blob_write overload for this type");
    }
}
inline void blob_write(BlobWriter& w,
                       const std::string& s) {
    uint32_t n = static_cast<uint32_t>(s.size());
    w.raw(&n, sizeof(n));
    w.raw(s.data(), n);
}
template <typename T>
void blob_write(BlobWriter& w,
                const std::vector<T>& v) {
    uint32_t n = static_cast<uint32_t>(v.size());
    w.raw(&n, sizeof(n));
    for (const T& x : v) blob_write(w, x);
}

// -- readers --
template <typename T>
void blob_read(BlobReader& r,
               T& v) {
    if constexpr (std::is_arithmetic_v<T>) {
        r.take(&v, sizeof(T));
    } else if constexpr (std::is_base_of_v<FlexData<T>, T>) {
        v.read_fields(r); // nested struct
    } else {
        static_assert(sizeof(T) == 0, "no blob_read overload for this type");
    }
}
inline void blob_read(BlobReader& r,
                      std::string& s) {
    uint32_t n = 0;
    if (!r.take(&n, sizeof(n))) return;
    if (static_cast<size_t>(r.end - r.p) < n) {
        r.ok = false;
        return;
    }
    s.assign(reinterpret_cast<const char*>(r.p), n);
    r.p += n;
}
template <typename T>
void blob_read(BlobReader& r,
               std::vector<T>& v) {
    uint32_t n = 0;
    if (!r.take(&n, sizeof(n))) return;
    v.clear();
    // n comes from stored bytes: never reserve more elements than bytes remain (a corrupt
    // count would otherwise allocate gigabytes and abort); the loop stops once r.ok drops
    v.reserve(std::min<size_t>(n, static_cast<size_t>(r.end - r.p)));
    for (uint32_t i = 0; i < n && r.ok; ++i) {
        T x{};
        blob_read(r, x);
        v.push_back(std::move(x));
    }
}

// ----------------------------------------------------------------------------
// Field registry helpers.
// ----------------------------------------------------------------------------
template <typename C, typename M>
struct Field {
    const char*              name;
    M C::*                   ptr;
};
template <typename C, typename M>
constexpr Field<C, M> fld(const char* n,
                          M C::* p) { return {n, p}; }

// ----------------------------------------------------------------------------
// JSON type mismatches (a string in a bool field, a number in a string field, ...)
// are rejected field by field and reported here; Os::begin points it at the
// console. Unset (the default) means silent.
// ----------------------------------------------------------------------------
inline std::function<void(std::string_view message)> flex_error_handler;

namespace flex_detail {
template <typename T> struct is_vector : std::false_type {};
template <typename T> struct is_vector<std::vector<T>> : std::true_type {};

// the struct's unqualified name, from the compiler's signature string (no RTTI needed)
template <typename T>
std::string_view type_name() {
    std::string_view s = __PRETTY_FUNCTION__;           // "... [with T = ns::Name; ...]" / "[T = Name]"
    const size_t     b = s.find("T = ") + 4;
    s                  = s.substr(b, s.find_first_of(";]", b) - b);
    const size_t     c = s.rfind("::");
    return c == std::string_view::npos ? s : s.substr(c + 2);
}

template <typename M>
constexpr const char* expected_kind() {
    if constexpr (std::is_same_v<M, bool>) return "bool";
    else if constexpr (std::is_integral_v<M>) return "integer";
    else if constexpr (std::is_floating_point_v<M>) return "number";
    else if constexpr (std::is_same_v<M, std::string>) return "string";
    else if constexpr (is_vector<M>::value) return "array";
    else return "object";
}

inline const char* json_kind(JsonVariantConst x, bool want_integer) {
    if (x.is<bool>()) return "bool";
    if (x.is<long long>() || x.is<unsigned long long>()) return want_integer ? "out-of-range integer" : "integer";
    if (x.is<double>()) return "float";
    if (x.is<const char*>()) return "string";
    if (x.is<JsonArrayConst>()) return "array";
    return "object";
}
} // namespace flex_detail

// ----------------------------------------------------------------------------
// FlexData<Derived>: all generic methods, written once. Derived supplies a
// static constexpr fields() tuple of fld("name", &Derived::member) entries.
// ----------------------------------------------------------------------------
template <typename Derived>
struct FlexData {
    static constexpr uint8_t kBlobVersion = 1;

    // ---- JSON ----
    void                     to_json_object(JsonObject o) const {
        visit(self(), [&](const char* n, const auto& v) { o[n] = v; });
    }
    // false if any present field had the wrong JSON type (that field is left unchanged).
    // Records which fields it assigned: see present() / has().
    bool from_json_object(JsonVariantConst v) {
        bool     ok   = true;
        uint32_t mask = 0;
        uint32_t i    = 0;
        visit(self(), [&](const char* n, auto& ref) {
            JsonVariantConst x = v[n];
            if (!x.isNull()) {
                if (assign(n, ref, x)) mask |= (i < 32 ? uint32_t{1} << i : 0);
                else ok = false;
            }
            ++i;
        });
        present_mask = mask;
        return ok;
    }

    // ---- field presence (the last from_json_object / update / from_json) ----
    // bit i set = the i-th entry of fields() was in the JSON, non-null and accepted. Tells a
    // missing field from one that holds its default. Cleared by the next JSON load; set_field and
    // from_blob do not touch it. At most 32 fields (static_assert).
    uint32_t present() const {
        static_assert(field_count() <= 32, "FlexData presence tracks at most 32 fields");
        return present_mask;
    }
    bool has(std::string_view field) const {
        static_assert(field_count() <= 32, "FlexData presence tracks at most 32 fields");
        bool     found = false;
        uint32_t i     = 0;
        visit(self(), [&](const char* n, const auto&) {
            if (field == n) found = (present_mask >> i) & 1u;
            ++i;
        });
        return found;
    }

    JsonDocument as_json_doc() const {
        JsonDocument doc;
        to_json_object(doc.to<JsonObject>());
        return doc;
    }
    std::string as_json_str() const {
        std::string out;
        serializeJson(as_json_doc(), out);
        return out;
    }

    // partial merge: only keys present in the JSON are overwritten; false on a parse error or
    // when any field was rejected for its type
    bool update(std::string_view json) {
        JsonDocument doc;
        if (deserializeJson(doc, json)) return false;
        return from_json_object(doc.as<JsonVariantConst>());
    }
    static Derived from_json(std::string_view json) {
        Derived d;
        d.update(json);
        return d;
    }

    // ---- one field by name ----
    template <typename V>
    bool set_field(std::string_view name, const V& value) {
        JsonDocument d;
        d.set(value);
        bool done = false;
        bool ok   = false;
        visit(self(), [&](const char* n, auto& ref) {
            if (!done && name == n) {
                done = true;
                ok   = assign(n, ref, d.template as<JsonVariantConst>());
            }
        });
        return done && ok;
    }
    std::string get_field(std::string_view name) const {
        std::string out = "null";
        visit(self(), [&](const char* n, const auto& ref) {
            if (name == n) {
                JsonDocument d;
                d.set(ref);
                out.clear();
                serializeJson(d, out);
            }
        });
        return out;
    }

    // ---- binary blob ----
    void write_fields(BlobWriter& w) const {
        visit(self(), [&](const char*, const auto& ref) { blob_write(w, ref); });
    }
    void read_fields(BlobReader& r) {
        visit(self(), [&](const char*, auto& ref) { blob_read(r, ref); });
    }

    std::vector<uint8_t> to_blob() const {
        BlobWriter w;
        uint8_t    ver = kBlobVersion;
        w.raw(&ver, 1);
        write_fields(w);
        return std::move(w.bytes);
    }
    bool from_blob(const std::vector<uint8_t>& bytes) {
        BlobReader r{bytes.data(), bytes.data() + bytes.size()};
        uint8_t    ver = 0;
        if (!r.take(&ver, 1) || ver != kBlobVersion) return false;
        read_fields(r);
        return r.ok;
    }

private:
    uint32_t       present_mask = 0;   // the only state FlexData adds to a struct: 4 bytes, never stored

    static constexpr size_t field_count() { return std::tuple_size_v<decltype(Derived::fields())>; }
    Derived&       self() { return static_cast<Derived&>(*this); }
    const Derived& self() const { return static_cast<const Derived&>(*this); }

    // type-matched assignment: is<M>() before as<M>() (integers: JSON integer in M's range;
    // float/double: any number; string: string only; nested struct: object; vector: array)
    template <typename M>
    static bool assign(const char* n, M& ref, JsonVariantConst x) {
        if (x.template is<M>()) {
            ref = x.template as<M>();
            return true;
        }
        if (flex_error_handler) {
            const std::string_view t = flex_detail::type_name<Derived>();
            char                   msg[96];
            std::snprintf(msg, sizeof(msg), "! %.*s.%s: expected %s, got %s", int(t.size()), t.data(), n,
                          flex_detail::expected_kind<M>(),
                          flex_detail::json_kind(x, std::is_integral_v<M> && !std::is_same_v<M, bool>));
            flex_error_handler(msg);
        }
        return false;
    }

    template <typename Self, typename Fn>
    static void visit(Self&& s, Fn&& fn) {
        std::apply([&](auto... f) { (fn(f.name, s.*(f.ptr)), ...); }, Derived::fields());
    }
};

} // namespace xewe

// ----------------------------------------------------------------------------
// ArduinoJson converter for any FlexData-derived type. This is what makes a
// FlexData usable as a JSON object AND as a vector element (nested structs).
// ----------------------------------------------------------------------------
namespace ArduinoJson {
template <typename T>
struct Converter<T, typename std::enable_if<std::is_base_of<::xewe::FlexData<T>, T>::value>::type> {
    static void toJson(const T& src, JsonVariant dst) {
        JsonObject o = dst.to<JsonObject>();
        src.to_json_object(o);
    }
    static T fromJson(JsonVariantConst src) {
        T out;
        out.from_json_object(src);
        return out;
    }
    static bool checkJson(JsonVariantConst src) { return src.is<JsonObjectConst>(); }
};
} // namespace ArduinoJson
