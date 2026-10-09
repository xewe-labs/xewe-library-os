// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/host/test/json/test_flexdata.cpp
//
// FlexData JSON and blob round trips. Needs ArduinoJson (run.sh adds it when found; see
// ARDUINOJSON_SRC there). The same cases run on the board through `$test flex` / `$test
// flex_bad` in tests/hardware/test_hooks_flexdata.py.

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
    CHECK(p.i == 0 && p.v.empty() && p.s == "5");
    // documented finding (2026-10-08, not fixed): ArduinoJson's as<bool>() turns any string into
    // true, so FlexData reads {"b":"false"} as b == true
    CHECK(p.b == true);
    CHECK(Probe::from_json(R"({"b":"false"})").b == true);
    const Probe h = Probe::from_json(R"({"i":99999999999999999999,"u":-1})");
    CHECK(h.i == 0 && h.u == 0);                                    // out of range -> 0
    CHECK(blob_round_trip(h));
    // documented finding: a non-finite float serializes as null, so the JSON is not stable
    const Probe inf = Probe::from_json(R"({"f":1e400})");
    CHECK(inf.as_json_str().find("\"f\":null") != std::string::npos);
    CHECK(Probe::from_json(inf.as_json_str()).as_json_str() != inf.as_json_str());
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
    // vector count 4 G: reserve() used to allocate 16 GiB and abort (bad_alloc)
    CHECK(!p.from_blob(with(34, 0xFF)));
}
