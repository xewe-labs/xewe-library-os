// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/unit/test/json/test_flexdata.cpp
//
// FlexData JSON and blob round trips. Needs ArduinoJson (run.sh adds it when found; see
// ARDUINOJSON_SRC there). The same cases run on the board through `$test flex` / `$test
// flex_bad` in tests/board/test_hooks_flexdata.py.

#include "../test.h"

#include <XeWeCore/FlexData.h>

#include <string>
#include <vector>


namespace {

struct Inner : xewe::FlexData<Inner> {
    int32_t     a = 0;
    std::string s;
    static constexpr auto fields() { return std::make_tuple(xewe::fld("a", &Inner::a), xewe::fld("s", &Inner::s)); }
};

// same layout as the firmware's Probe in src/XeWeCore/Testing.cpp
struct Probe : xewe::FlexData<Probe> {
    bool               b = false;
    int32_t            i = 0;
    uint32_t           u = 0;
    int64_t            l = 0;
    float              f = 0.0f;
    double             d = 0.0;
    std::string        s;
    std::vector<int32_t> v;
    Inner              in;
    std::vector<Inner> vi;
    static constexpr auto fields() {
        using xewe::fld;
        return std::make_tuple(fld("b", &Probe::b), fld("i", &Probe::i), fld("u", &Probe::u), fld("l", &Probe::l),
                               fld("f", &Probe::f), fld("d", &Probe::d), fld("s", &Probe::s), fld("v", &Probe::v),
                               fld("in", &Probe::in), fld("vi", &Probe::vi));
    }
};

const std::string kDefault =
    R"({"b":false,"i":0,"u":0,"l":0,"f":0,"d":0,"s":"","v":[],"in":{"a":0,"s":""},"vi":[]})";

bool blob_round_trip(const Probe& p) {
    Probe q;
    return q.from_blob(p.to_blob()) && q.as_json_str() == p.as_json_str();
}

} // namespace

TEST(flex_default_canonical) {
    CHECK_EQ(Probe{}.as_json_str(), kDefault);
    CHECK_EQ(Probe{}.to_blob().size(), size_t{1 + 1 + 4 + 4 + 8 + 4 + 8 + 4 + 4 + 4 + 4 + 4});
}

TEST(flex_full_round_trip) {
    const char* in = R"({"b":true,"i":-7,"u":4000000000,"l":-9000000000,"f":1.5,"d":0.25,"s":"x y",)"
                     R"("v":[1,-2,3],"in":{"a":9,"s":"n"},"vi":[{"a":1,"s":"p"},{"a":2,"s":"q"}]})";
    const Probe p = Probe::from_json(in);
    CHECK_EQ(p.as_json_str(), std::string(in));
    CHECK(blob_round_trip(p));
    CHECK_EQ(Probe::from_json(p.as_json_str()).as_json_str(), p.as_json_str());   // stable
}

TEST(flex_partial_update_keeps_other_fields) {
    Probe p = Probe::from_json(R"({"i":5,"s":"keep"})");
    p.update(R"({"i":6})");
    CHECK(p.i == 6 && p.s == "keep");
}

TEST(flex_malformed_is_ignored) {
    for (const char* bad : {R"({"i":1,"s":"ab)", R"({"i":1} garbage)", "", "null", "[1,2,3]"}) {
        const Probe p = Probe::from_json(bad);
        // truncated/garbage input: update() bails on the parse error or finds no object
        if (p.as_json_str() != kDefault && std::string(bad) != R"({"i":1} garbage)") {
            host_test::fail(__FILE__, __LINE__, bad);
        }
    }
    // ArduinoJson stops at the end of the first value: trailing garbage is not an error
    CHECK_EQ(Probe::from_json(R"({"i":1} garbage)").i, 1);
}

TEST(flex_wrong_types_and_huge_numbers) {
    const Probe p = Probe::from_json(R"({"b":"yes","i":"abc","v":{"x":1},"in":[1],"s":5})");
    CHECK_EQ(p.as_json_str(), kDefault);                            // every field rejected (type-matched)
    const Probe h = Probe::from_json(R"({"i":99999999999999999999,"u":-1})");
    CHECK(h.i == 0 && h.u == 0);                                    // out of range -> rejected
    CHECK(blob_round_trip(h));
    // documented behaviour: a non-finite float serializes as null, so the JSON is not stable
    const Probe inf = Probe::from_json(R"({"f":1e400})");
    CHECK(inf.as_json_str().find("\"f\":null") != std::string::npos);
    CHECK(Probe::from_json(inf.as_json_str()).as_json_str() != inf.as_json_str());
}

// ---- type-matched assignment ------------------------------------------------

namespace {
// captures what FlexData reports while in scope
struct Capture {
    std::vector<std::string> msgs;
    Capture() { xewe::flex_error_handler = [this](std::string_view m) { msgs.emplace_back(m); }; }
    ~Capture() { xewe::flex_error_handler = nullptr; }
};
} // namespace

TEST(flex_type_string_into_bool_rejected) {
    Capture c;
    Probe   p;
    p.b = true;
    CHECK(!p.update(R"({"b":"false"})"));
    CHECK(p.b == true);                                             // unchanged, not coerced
    CHECK(c.msgs.size() == 1);
    if (!c.msgs.empty()) CHECK_EQ(c.msgs[0], std::string("! Probe.b: expected bool, got string"));
    p.b = false;
    CHECK(!p.update(R"({"b":"yes"})") && p.b == false);
    CHECK(!p.update(R"({"b":1})") && p.b == false);                 // a number is not a bool
}

TEST(flex_type_bool_accepted) {
    Capture c;
    Probe   p;
    CHECK(p.update(R"({"b":true})") && p.b == true);
    CHECK(p.update(R"({"b":false})") && p.b == false);
    CHECK(c.msgs.empty());
}

TEST(flex_type_number_into_string_rejected) {
    Capture c;
    Probe   p;
    p.s = "keep";
    CHECK(!p.update(R"({"s":5})"));
    CHECK_EQ(p.s, std::string("keep"));
    if (!c.msgs.empty()) CHECK_EQ(c.msgs[0], std::string("! Probe.s: expected string, got integer"));
    CHECK(p.update(R"({"s":"5"})") && p.s == "5");
}

TEST(flex_type_numbers) {
    Capture c;
    Probe   p;
    CHECK(p.update(R"({"f":2,"d":-3})") && p.f == 2.0f && p.d == -3.0);   // integer -> float ok
    p.i = 4;
    CHECK(!p.update(R"({"i":1.5})") && p.i == 4);                   // float -> integer rejected
    CHECK(!p.update(R"({"i":"7"})") && p.i == 4);                   // string -> integer rejected
    CHECK(!p.update(R"({"u":-1})") && p.u == 0);                    // range check kept
    CHECK(!p.update(R"({"f":"1.5"})") && p.f == 2.0f);
    CHECK(c.msgs.size() == 4);
    if (c.msgs.size() == 4) {
        CHECK_EQ(c.msgs[0], std::string("! Probe.i: expected integer, got float"));
        CHECK_EQ(c.msgs[2], std::string("! Probe.u: expected integer, got out-of-range integer"));
        CHECK_EQ(c.msgs[3], std::string("! Probe.f: expected number, got string"));
    }
}

TEST(flex_type_mixed_document_partially_applied) {
    Capture c;
    Probe   p;
    p.s = "old";
    CHECK(!p.update(R"({"i":9,"b":"true","s":7,"f":0.5,"in":{"a":3,"s":1},"v":[1,2]})"));
    CHECK(p.i == 9 && p.f == 0.5f && p.v.size() == 2);              // good fields applied
    CHECK(p.b == false && p.s == "old");                            // bad fields unchanged
    CHECK(p.in.a == 3 && p.in.s.empty());                           // nested: same rules, own report
    CHECK(c.msgs.size() == 3);
    if (c.msgs.size() == 3) CHECK_EQ(c.msgs[2], std::string("! Inner.s: expected string, got integer"));
}

TEST(flex_type_set_field) {
    Capture c;
    Probe   p;
    CHECK(!p.set_field("b", "true") && p.b == false);
    CHECK(p.set_field("b", true) && p.b == true);
    CHECK(!p.set_field("s", 3) && p.s.empty());
    CHECK(p.set_field("d", 2) && p.d == 2.0);
    CHECK(!p.set_field("nope", 1));
    CHECK(c.msgs.size() == 2);
}

TEST(flex_type_silent_without_handler) {
    Probe p;
    CHECK(!p.update(R"({"b":"x"})") && p.b == false);               // no handler: still rejected
}

TEST(flex_unicode_and_deep_nesting) {
    const Probe p = Probe::from_json(R"({"s":"é😀"})");
    CHECK_EQ(p.s, std::string("\xC3\xA9\xF0\x9F\x98\x80"));
    CHECK(blob_round_trip(p));
    std::string deep;
    for (int k = 0; k < 200; ++k) deep += "{\"in\":";
    deep += "1";
    for (int k = 0; k < 200; ++k) deep += "}";
    CHECK_EQ(Probe::from_json(deep).as_json_str(), kDefault);      // TooDeep -> ignored
}

TEST(flex_corrupt_blobs_rejected) {
    const std::vector<uint8_t> good = Probe{}.to_blob();
    auto with = [&](std::size_t at, uint8_t byte4) {
        std::vector<uint8_t> b = good;
        for (std::size_t k = 0; k < 4; ++k) b[at + k] = byte4;
        return b;
    };
    Probe p;
    std::vector<uint8_t> v = good;
    v[0] = 0x7F;
    CHECK(!p.from_blob(v));                                         // version
    v = good;
    v.resize(v.size() / 2);
    CHECK(!p.from_blob(v));                                         // truncated
    CHECK(!p.from_blob({}));                                        // empty
    CHECK(!p.from_blob(with(30, 0xFF)));                            // string length 4 GiB
    // vector count 4 G: must not reserve() 16 GiB and abort (bad_alloc)
    CHECK(!p.from_blob(with(34, 0xFF)));
}

// ---- field presence: missing vs default vs present ----

namespace {
struct Versioned : xewe::FlexData<Versioned> {
    uint8_t     schema = 1;
    std::string name   = "pad";
    static constexpr auto fields() {
        return std::make_tuple(xewe::fld("schema", &Versioned::schema), xewe::fld("name", &Versioned::name));
    }
};
} // namespace

TEST(flex_presence_missing_vs_default) {
    Versioned v;
    CHECK(v.present() == 0);
    CHECK(v.update(R"({"name":"x"})"));
    CHECK(!v.has("schema") && v.schema == 1);                      // missing: default value, not present
    CHECK(v.has("name"));
    CHECK(v.present() == 0b10u);
    CHECK(v.update(R"({"schema":1})"));
    CHECK(v.has("schema") && v.schema == 1);                       // present, same value as the default
    CHECK(!v.has("name"));                                         // cleared by the next load
    CHECK(!v.has("nope"));
}

TEST(flex_presence_null_and_rejected_are_not_present) {
    Capture   c;
    Versioned v;
    CHECK(v.update(R"({"schema":null,"name":"y"})"));
    CHECK(!v.has("schema") && v.has("name"));
    CHECK(!v.update(R"({"schema":"2","name":"z"})"));             // wrong type: rejected
    CHECK(!v.has("schema") && v.has("name") && v.schema == 1);
    CHECK(Versioned::from_json(R"({"schema":3})").has("schema"));
    CHECK(!Versioned::from_json("{}").has("schema"));
}

TEST(flex_presence_nested_and_set_by_blob) {
    Probe p;
    CHECK(p.update(R"({"in":{"s":"q"},"vi":[{"a":5}]})"));
    CHECK(p.has("in") && p.has("vi") && !p.has("b"));
    CHECK(p.in.has("s") && !p.in.has("a"));                       // nested structs track their own
    CHECK(p.vi.size() == 1 && p.vi[0].has("a") && !p.vi[0].has("s"));
    Probe src;
    src.vi.resize(1);
    CHECK(p.from_blob(src.to_blob()));
    CHECK(p.present() == 0x3FFu);                                  // a blob holds all 10 fields
    CHECK(p.in.has("a") && p.in.has("s"));                         // nested: every field too
    CHECK(p.vi.size() == 1 && p.vi[0].has("a") && p.vi[0].has("s"));
    CHECK(p.set_field("b", true) && p.has("b"));                  // set_field does not clear it
}

// testing v1 (2026-10-09, board): fan, mlx90614, YourModuleFull and the pad check has("schema")
// after Nvs::read_flex; with presence JSON-only that was always false, so every stored blob was
// treated as foreign ("stored settings have schema 1, this firmware reads schema 1; using the
// default") and fan curve changes were lost at every restart.
TEST(flex_presence_after_blob_load_has_schema) {
    Versioned stored;
    stored.schema = 1;
    const std::vector<uint8_t> blob = stored.to_blob();
    Versioned v;
    CHECK(v.update(R"({"name":"x"})") && !v.has("schema"));      // a JSON load without schema
    CHECK(v.from_blob(blob));
    CHECK(v.has("schema") && v.has("name") && v.schema == 1);     // the blob load replaces the mask
    Versioned fresh;
    CHECK(fresh.from_blob(blob) && fresh.has("schema"));          // what read_flex hands a loader
    std::vector<uint8_t> bad_version = blob;
    bad_version[0] = 99;
    CHECK(!v.from_blob(bad_version) && v.present() == 0);         // a failed load marks nothing
    CHECK(v.from_blob(blob) && v.present() == 0b11u);
    const std::vector<uint8_t> truncated(blob.begin(), blob.begin() + 2);
    CHECK(!v.from_blob(truncated) && v.present() == 0);
}
