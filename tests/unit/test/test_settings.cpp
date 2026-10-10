// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/unit/test/test_settings.cpp
//
// The settings table engine (Settings.h/.cpp) against the in-memory NVS shim: type deduction,
// compile-time key checks, load (defaults, NVS overrides, type mismatch, out of range), set
// (parse/range per type incl. f32), secret masking, restart flag, schema line format, the
// $system schema header. Module glue (commands, begin) needs the ESP32 core: board-side only.
// Nvs.h needs <span> (C++20) and FlexData.h needs ArduinoJson, so this file is empty otherwise.
// Nvs.cpp itself is compiled into test_nvs.cpp's translation unit; this one links against it.

#if __cplusplus >= 202002L && __has_include(<ArduinoJson.h>)

#include "test.h"

class String;  // Nvs.tpp names Arduino's String; the host shim has none (never instantiated here)

#include <XeWeCore/Nvs.h>
#include <XeWeCore/Settings.cpp>   // not in run.sh's LIB_SRCS: compiled into this TU only

#include <string>
#include <vector>


// Module.cpp owns the real table (its `attach` registers the commands); the host test wires the
// engine functions the same way, without attach
const xewe::SettingsEngine xewe::settings_engine = {
    xewe::detail::settings_load, xewe::detail::settings_set, xewe::detail::settings_value,
    xewe::detail::settings_schema, xewe::detail::settings_expected, nullptr,
};

namespace {

using xewe::SettingDef;
using xewe::SettingType;
using Err = xewe::Settings::SetError;

struct Fake {
    bool        on    = false;
    uint8_t     level = 0;
    int8_t      trim  = 0;
    uint16_t    count = 0;
    int16_t     temp  = 0;
    uint32_t    big   = 0;
    int         delta = 0;     // int: deduced as i32
    float       gain  = 0.f;
    std::string name;
    std::string pass;

    xewe::Settings settings() const {
        static constexpr SettingDef table[] = {
            xewe::setting<&Fake::on>   ("on", true, "Power on boot"),
            xewe::setting<&Fake::level>("level", 1, 200, 100, "Level"),
            xewe::setting<&Fake::trim> ("trim", -10, 10, -2),
            xewe::setting<&Fake::count>("count", 0, 60000, 60, nullptr, SettingDef::RESTART),
            xewe::setting<&Fake::temp> ("temp", -400, 1250, 250),
            xewe::setting<&Fake::big>  ("big", 0, 4294967295.0, 4000000000.0),
            xewe::setting<&Fake::delta>("delta", -2147483648.0, 2147483647.0, -5),
            xewe::setting<&Fake::gain> ("gain", -1.5, 2.5, 0.25, "Gain \"x\""),
            xewe::setting<&Fake::name> ("name", 12, "kitchen", "Device label"),
            xewe::setting<&Fake::pass> ("pass", 63, "", "Password", SettingDef::SECRET),
        };
        return {table, this};
    }
};

struct Fx {
    xewe::SerialPort serial;
    xewe::Nvs        nvs;
    Fake             fake;
    xewe::Settings   table;
    std::vector<std::string> errors;   // Nvs error handler (the full-partition test)

    Fx() : table(fake.settings()) {
        host_nvs::reset();
        nvs.set_error_handler([this](std::string_view m) { errors.emplace_back(m); });
        Serial.take();
        xewe::SerialPortConfig cfg;
        cfg.startup_delay_ms = 0;
        cfg.echo             = false;
        serial.begin(cfg);
        Serial.take();
    }
    Err         set(const char* key, const char* text) { return table.set(nvs, "fk", key, text); }
    std::string row(const char* key)                   { return table.schema(*table.find(key)); }
};

} // namespace

// ---- declaration (compile time) ---------------------------------------------------------------

static_assert(xewe::detail::setting_type_of<bool>()        == SettingType::BOOL);
static_assert(xewe::detail::setting_type_of<uint8_t>()     == SettingType::U8);
static_assert(xewe::detail::setting_type_of<int8_t>()      == SettingType::I8);
static_assert(xewe::detail::setting_type_of<uint16_t>()    == SettingType::U16);
static_assert(xewe::detail::setting_type_of<int16_t>()     == SettingType::I16);
static_assert(xewe::detail::setting_type_of<uint32_t>()    == SettingType::U32);
static_assert(xewe::detail::setting_type_of<int>()         == SettingType::I32);
static_assert(xewe::detail::setting_type_of<float>()       == SettingType::F32);
static_assert(xewe::detail::setting_type_of<std::string>() == SettingType::STRING);
// the build-time key check: <= 15 chars (the NVS limit), no whitespace/quotes, no core key
static_assert( xewe::detail::key_ok("fifteen_chars_x"));
static_assert(!xewe::detail::key_ok("sixteen_chars_xx"));
static_assert(!xewe::detail::key_ok(""));
static_assert(!xewe::detail::key_ok("two words"));
static_assert(!xewe::detail::key_ok("is_enabled"));
static_assert(!xewe::detail::key_ok("not_first_boot"));
static_assert(!xewe::detail::key_ok("init_complete"));

TEST(settings_table_is_constexpr_and_typed) {
    Fake f;
    const xewe::Settings t = f.settings();
    CHECK(!t.empty());
    CHECK_EQ(t.rows().size(), std::size_t(10));
    CHECK(t.find("gain")->type == SettingType::F32);
    CHECK(t.find("delta")->type == SettingType::I32);
    CHECK(t.find("pass")->flags & SettingDef::SECRET);
    CHECK(t.find("nope") == nullptr);
    // what Module::settings() returns by default: every call is a no-op, no NVS access
    const xewe::Settings none;
    CHECK(none.empty());
    host_nvs::reset();
    xewe::Nvs nvs;
    none.load(nvs, "fk");
    CHECK(none.set(nvs, "fk", "level", "1") == Err::UNKNOWN_KEY);
    CHECK(none.find("level") == nullptr);
    CHECK(host_nvs::state().data.empty());
}

TEST(settings_load_defaults_then_nvs_overrides) {
    Fx f;
    f.fake.level = 7;                     // the member initializer loses to the table default
    f.table.load(f.nvs, "fk", &f.serial);
    CHECK_EQ(f.fake.on, true);
    CHECK_EQ(f.fake.level, uint8_t(100));
    CHECK_EQ(f.fake.trim, int8_t(-2));
    CHECK_EQ(f.fake.big, 4000000000u);
    CHECK_EQ(f.fake.delta, -5);
    CHECK_EQ(f.fake.gain, 0.25f);
    CHECK_EQ(f.fake.name, std::string("kitchen"));
    CHECK(f.fake.pass.empty());
    CHECK(Serial.take().empty());         // read misses are silent

    f.nvs.write<uint8_t>("fk", "level", 150);
    f.nvs.write<float>("fk", "gain", -1.25f);
    f.nvs.write<std::string>("fk", "name", "porch");
    f.nvs.write<int32_t>("fk", "temp", 99);   // stored as i32, row is i16: type mismatch = default
    f.table.load(f.nvs, "fk", &f.serial);
    CHECK_EQ(f.fake.level, uint8_t(150));
    CHECK_EQ(f.fake.gain, -1.25f);
    CHECK_EQ(f.fake.name, std::string("porch"));
    CHECK_EQ(f.fake.temp, int16_t(250));
    CHECK(Serial.take().empty());
}

TEST(settings_load_out_of_range_keeps_default_and_reports) {
    Fx f;
    f.nvs.write<uint8_t>("fk", "level", 250);         // max 200
    f.nvs.write<std::string>("fk", "name", "a name far too long");
    f.table.load(f.nvs, "fk", &f.serial);
    CHECK_EQ(f.fake.level, uint8_t(100));
    CHECK_EQ(f.fake.name, std::string("kitchen"));
    const std::string out = Serial.take();
    CHECK(out.find("! fk/level: stored value outside u8 in [1, 200], using the default") != std::string::npos);
    CHECK(out.find("! fk/name: stored value outside str of 0-12 chars, using the default") != std::string::npos);
}

TEST(settings_set_validates_each_type) {
    Fx f;
    f.table.load(f.nvs, "fk", nullptr);
    // bool
    CHECK(f.set("on", "off") == Err::NONE);   CHECK_EQ(f.fake.on, false);
    CHECK(f.set("on", "TRUE") == Err::NONE);  CHECK_EQ(f.fake.on, true);
    CHECK(f.set("on", "0") == Err::NONE);     CHECK_EQ(f.fake.on, false);
    CHECK(f.set("on", "2") == Err::BAD_VALUE);
    CHECK(f.set("on", "") == Err::BAD_VALUE);
    // u8 [1, 200]
    CHECK(f.set("level", "1") == Err::NONE);  CHECK_EQ(f.fake.level, uint8_t(1));
    CHECK(f.set("level", "200") == Err::NONE);
    CHECK(f.set("level", "0") == Err::BAD_VALUE);
    CHECK(f.set("level", "201") == Err::BAD_VALUE);
    CHECK(f.set("level", "-1") == Err::BAD_VALUE);
    CHECK(f.set("level", "12x") == Err::BAD_VALUE);
    CHECK(f.set("level", "1.5") == Err::BAD_VALUE);
    CHECK_EQ(f.fake.level, uint8_t(200));     // a refused value changes nothing
    // i8 [-10, 10]
    CHECK(f.set("trim", "-10") == Err::NONE); CHECK_EQ(f.fake.trim, int8_t(-10));
    CHECK(f.set("trim", "-11") == Err::BAD_VALUE);
    // u16 / i16
    CHECK(f.set("count", "60000") == Err::NONE);
    CHECK(f.set("count", "60001") == Err::BAD_VALUE);
    CHECK(f.set("temp", "-400") == Err::NONE);  CHECK_EQ(f.fake.temp, int16_t(-400));
    CHECK(f.set("temp", "1251") == Err::BAD_VALUE);
    // u32 / i32 at the type edges
    CHECK(f.set("big", "4294967295") == Err::NONE);  CHECK_EQ(f.fake.big, 4294967295u);
    CHECK(f.set("big", "4294967296") == Err::BAD_VALUE);
    CHECK(f.set("delta", "-2147483648") == Err::NONE); CHECK_EQ(f.fake.delta, -2147483647 - 1);
    CHECK(f.set("delta", "2147483648") == Err::BAD_VALUE);
    // f32 [-1.5, 2.5]
    CHECK(f.set("gain", "2.5") == Err::NONE);   CHECK_EQ(f.fake.gain, 2.5f);
    CHECK(f.set("gain", "-1.5") == Err::NONE);
    CHECK(f.set("gain", "0.1") == Err::NONE);   CHECK_EQ(f.fake.gain, 0.1f);
    CHECK(f.set("gain", "2.51") == Err::BAD_VALUE);
    CHECK(f.set("gain", "nan") == Err::BAD_VALUE);
    CHECK(f.set("gain", "inf") == Err::BAD_VALUE);
    CHECK(f.set("gain", "abc") == Err::BAD_VALUE);
    CHECK(f.set("gain", "-.5") == Err::NONE);   CHECK_EQ(f.fake.gain, -0.5f);
    CHECK(f.set("gain", "2.") == Err::NONE);    CHECK_EQ(f.fake.gain, 2.0f);
    CHECK(f.set("gain", "+1e0") == Err::NONE);  CHECK_EQ(f.fake.gain, 1.0f);
    CHECK(f.set("gain", "125E-2") == Err::NONE); CHECK_EQ(f.fake.gain, 1.25f);
    CHECK(f.set("gain", "0.000000000000000000001e21") == Err::NONE); CHECK_EQ(f.fake.gain, 1.0f);
    CHECK(f.set("gain", "1e") == Err::BAD_VALUE);
    CHECK(f.set("gain", ".") == Err::BAD_VALUE);
    CHECK(f.set("gain", "1.2.3") == Err::BAD_VALUE);
    CHECK(f.set("gain", " 1") == Err::BAD_VALUE);
    CHECK(f.set("gain", "0x1") == Err::BAD_VALUE);
    CHECK(f.set("gain", "1e999") == Err::BAD_VALUE);
    CHECK(f.set("gain", "") == Err::BAD_VALUE);
    CHECK(f.set("gain", "0.1") == Err::NONE);
    // str 0-12 chars
    CHECK(f.set("name", "") == Err::NONE);      CHECK(f.fake.name.empty());
    CHECK(f.set("name", "twelve chars") == Err::NONE);
    CHECK(f.set("name", "thirteen char") == Err::BAD_VALUE);
    CHECK_EQ(f.fake.name, std::string("twelve chars"));
    // unknown
    const SettingDef* row = &*f.table.find("on");
    CHECK(f.table.set(f.nvs, "fk", "nope", "1", &row) == Err::UNKNOWN_KEY);
    CHECK(row == nullptr);
    CHECK(Serial.take().empty());               // the engine prints nothing; Module does
}

TEST(settings_set_persists_typed_and_round_trips) {
    Fx f;
    f.table.load(f.nvs, "fk", nullptr);
    CHECK(f.set("on", "false") == Err::NONE);
    CHECK(f.set("level", "42") == Err::NONE);
    CHECK(f.set("trim", "-7") == Err::NONE);
    CHECK(f.set("big", "123456789") == Err::NONE);
    CHECK(f.set("gain", "1.75") == Err::NONE);
    CHECK(f.set("name", "porch light") == Err::NONE);
    CHECK(f.set("pass", "hunter2") == Err::NONE);

    const auto& ns = host_nvs::state().data["fk"];
    CHECK(ns.at("on").type    == host_nvs::Type::U8);
    CHECK(ns.at("level").type == host_nvs::Type::U8);
    CHECK(ns.at("trim").type  == host_nvs::Type::I8);
    CHECK(ns.at("big").type   == host_nvs::Type::U32);
    CHECK(ns.at("gain").type  == host_nvs::Type::BLOB);   // f32: Nvs's float blob path
    CHECK(ns.at("name").type  == host_nvs::Type::STR);
    CHECK_EQ(f.nvs.read<float>("fk", "gain"), 1.75f);
    CHECK_EQ(host_nvs::state().open.size(), std::size_t(0));

    Fake           g;                               // a fresh object: the next boot
    xewe::Settings t = g.settings();
    t.load(f.nvs, "fk", nullptr);
    CHECK_EQ(g.on, false);
    CHECK_EQ(g.level, uint8_t(42));
    CHECK_EQ(g.trim, int8_t(-7));
    CHECK_EQ(g.big, 123456789u);
    CHECK_EQ(g.gain, 1.75f);
    CHECK_EQ(g.name, std::string("porch light"));
    CHECK_EQ(g.pass, std::string("hunter2"));
    CHECK_EQ(g.count, uint16_t(60));                // never set: default
}

TEST(settings_set_not_saved_still_applies) {
    Fx f;
    f.table.load(f.nvs, "fk", nullptr);
    f.nvs.write<bool>("fk", "x", true);             // namespace exists
    host_nvs::state().full = true;
    CHECK(f.set("level", "33") == Err::NOT_SAVED);
    CHECK_EQ(f.fake.level, uint8_t(33));
    CHECK_EQ(f.errors.size(), std::size_t(1));
}

TEST(settings_schema_line_format) {
    Fx f;
    f.table.load(f.nvs, "fk", nullptr);
    f.set("level", "42");
    CHECK_EQ(f.row("level"), std::string(R"("key":"level","type":"u8","min":1,"max":200,"default":100,"value":42,"doc":"Level")"));
    CHECK_EQ(f.row("on"),    std::string(R"("key":"on","type":"bool","default":true,"value":true,"doc":"Power on boot")"));
    CHECK_EQ(f.row("trim"),  std::string(R"("key":"trim","type":"i8","min":-10,"max":10,"default":-2,"value":-2)"));
    CHECK_EQ(f.row("gain"),  std::string(R"("key":"gain","type":"f32","min":-1.5,"max":2.5,"default":0.25,"value":0.25,"doc":"Gain \"x\"")"));
    CHECK_EQ(f.row("big"),   std::string(R"("key":"big","type":"u32","min":0,"max":4294967295,"default":4000000000,"value":4000000000)"));
    CHECK_EQ(f.row("name"),  std::string(R"("key":"name","type":"str","min":0,"max":12,"default":"kitchen","value":"kitchen","doc":"Device label")"));
    // restart flag
    CHECK_EQ(f.row("count"), std::string(R"("key":"count","type":"u16","min":0,"max":60000,"default":60,"value":60,"restart":true)"));
    f.set("name", "a\"b\\c");
    CHECK(f.row("name").find(R"("value":"a\"b\\c")") != std::string::npos);
}

TEST(settings_secret_is_never_printed) {
    Fx f;
    f.table.load(f.nvs, "fk", nullptr);
    const SettingDef& pass = *f.table.find("pass");
    CHECK_EQ(f.row("pass"), std::string(R"("key":"pass","type":"str","min":0,"max":63,"value":"********","secret":true,"set":false,"doc":"Password")"));
    CHECK_EQ(f.table.value(pass), std::string(""));
    CHECK(f.set("pass", "hunter2") == Err::NONE);
    CHECK_EQ(f.row("pass"), std::string(R"("key":"pass","type":"str","min":0,"max":63,"value":"********","secret":true,"set":true,"doc":"Password")"));
    CHECK_EQ(f.table.value(pass), std::string("********"));
    CHECK(f.row("pass").find("hunter2") == std::string::npos);
}

TEST(settings_schema_out_rows_and_module_prefix) {
    Fx f;
    f.table.load(f.nvs, "fk", nullptr);
    xewe::SchemaOut plain(f.serial);
    plain.row(f.row("trim"));
    CHECK_EQ(Serial.take(), std::string("{\"key\":\"trim\",\"type\":\"i8\",\"min\":-10,\"max\":10,\"default\":-2,\"value\":-2}\r\n"));
    xewe::SchemaOut all(f.serial, "fk");
    all.row(R"("key":"speed","group":"mode:rainbow","set":"$led mode param 5 speed <v>")");   // a schema_extra row
    all.set_module("other");
    all.row(f.row("on"));
    CHECK_EQ(all.count(), std::size_t(2));
    CHECK_EQ(Serial.take(), std::string(
        "{\"module\":\"fk\",\"key\":\"speed\",\"group\":\"mode:rainbow\",\"set\":\"$led mode param 5 speed <v>\"}\r\n"
        "{\"module\":\"other\",\"key\":\"on\",\"type\":\"bool\",\"default\":true,\"value\":true,\"doc\":\"Power on boot\"}\r\n"));
}

TEST(settings_system_schema_header) {
    const std::vector<std::string_view> ids = {"system", "wifi", "led"};
    CHECK_EQ(xewe::Settings::header("2.1.0", "Kitchen \"Lights\"", ids),
             std::string(R"({"schema":1,"core":"2.1.0","device":"Kitchen \"Lights\"","modules":["system","wifi","led"]})"));
    CHECK_EQ(xewe::Settings::header("2.1.0", "", {}), std::string(R"({"schema":1,"core":"2.1.0","device":"","modules":[]})"));
}

TEST(settings_expected_messages) {
    Fake f;
    const xewe::Settings t = f.settings();
    CHECK_EQ(t.expected(*t.find("level")), std::string("u8 in [1, 200]"));
    CHECK_EQ(t.expected(*t.find("gain")),  std::string("f32 in [-1.5, 2.5]"));
    CHECK_EQ(t.expected(*t.find("on")),    std::string("bool (true/false, on/off, 1/0)"));
    CHECK_EQ(t.expected(*t.find("name")),  std::string("str of 0-12 chars"));
}

#endif
