// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/unit/test/test_nvs.cpp
//
// Nvs against the in-memory NVS shim (shim/nvs.h). Nvs.h needs <span> (C++20) and
// FlexData.h needs ArduinoJson, so this file is empty at -std=c++17 or without it.

#if __cplusplus >= 202002L && __has_include(<ArduinoJson.h>)

#include "test.h"

class String;  // Nvs.tpp names Arduino's String; the host shim has none (never instantiated here)

#include <XeWeCore/Nvs.cpp>   // Nvs.cpp is not in run.sh's LIB_SRCS: compiled into this TU only

#include <string>
#include <vector>


namespace {

struct Fx {
    xewe::Nvs                nvs;
    std::vector<std::string> errors;

    Fx() {
        host_nvs::reset();
        nvs.set_error_handler([this](std::string_view m) { errors.emplace_back(m); });
    }
    std::size_t open_handles() const { return host_nvs::state().open.size(); }
    bool        logged(const char* needle) const {
        for (const auto& e : errors) if (e.find(needle) != std::string::npos) return true;
        return false;
    }
};

} // namespace

TEST(nvs_round_trip_all_types) {
    Fx f;
    CHECK(f.nvs.write<bool>("ns", "b", true));
    CHECK(f.nvs.write<int8_t>("ns", "i8", -8));
    CHECK(f.nvs.write<uint16_t>("ns", "u16", 65535));
    CHECK(f.nvs.write<int32_t>("ns", "i32", -123456));
    CHECK(f.nvs.write<uint64_t>("ns", "u64", UINT64_MAX));
    CHECK(f.nvs.write<int64_t>("ns", "i64", INT64_MIN));
    CHECK(f.nvs.write<float>("ns", "f", 1.5f));
    CHECK(f.nvs.write<double>("ns", "d", -2.25));
    CHECK(f.nvs.write<std::string>("ns", "s", "hello"));
    CHECK(f.nvs.write<std::string>("ns", "empty", ""));
    CHECK(f.nvs.write<const char*>("ns", "cstr", "lit"));
    CHECK(f.nvs.write("ns", "lit", "bare"));                 // T = char[5]: no -Waddress
    CHECK(f.nvs.write<const char*>("ns", "null", nullptr));
    CHECK(f.nvs.write<std::string_view>("ns", "sv", std::string_view("view")));

    CHECK_EQ(f.nvs.read<bool>("ns", "b"), true);
    CHECK_EQ(f.nvs.read<int8_t>("ns", "i8"), int8_t(-8));
    CHECK_EQ(f.nvs.read<uint16_t>("ns", "u16"), uint16_t(65535));
    CHECK_EQ(f.nvs.read<int32_t>("ns", "i32"), int32_t(-123456));
    CHECK_EQ(f.nvs.read<uint64_t>("ns", "u64"), UINT64_MAX);
    CHECK_EQ(f.nvs.read<int64_t>("ns", "i64"), INT64_MIN);
    CHECK_EQ(f.nvs.read<float>("ns", "f"), 1.5f);
    CHECK_EQ(f.nvs.read<double>("ns", "d"), -2.25);
    CHECK_EQ(f.nvs.read<std::string>("ns", "s"), std::string("hello"));
    CHECK_EQ(f.nvs.read<std::string>("ns", "empty", "dflt"), std::string(""));  // empty != missing
    CHECK_EQ(f.nvs.read<std::string>("ns", "cstr"), std::string("lit"));
    CHECK_EQ(f.nvs.read<std::string>("ns", "lit"), std::string("bare"));
    CHECK_EQ(f.nvs.read<std::string>("ns", "null", "x"), std::string(""));
    CHECK_EQ(f.nvs.read<std::string>("ns", "sv"), std::string("view"));
    CHECK_EQ(f.open_handles(), std::size_t(0));
    CHECK(f.errors.empty());
}

TEST(nvs_key_length_rejects_not_truncates) {
    Fx f;
    const std::string k15(15, 'k');
    CHECK(f.nvs.write<int32_t>("ns", k15, 15));
    CHECK_EQ(f.nvs.read<int32_t>("ns", k15), 15);

    // two 16-char keys sharing a 15-char prefix: both rejected, nothing written, no collision
    const std::string a = k15 + "A", b = k15 + "B";
    CHECK(!f.nvs.write<int32_t>("ns", a, 1));
    CHECK(!f.nvs.write<int32_t>("ns", b, 2));
    CHECK_EQ(f.nvs.read<int32_t>("ns", k15), 15);           // the 15-char key is untouched
    CHECK_EQ(f.nvs.read<int32_t>("ns", a, -1), -1);
    CHECK_EQ(host_nvs::state().data["ns"].size(), std::size_t(1));
    CHECK(f.logged("too long"));

    // namespaces follow the same rule
    CHECK(f.nvs.write<int32_t>(std::string(15, 'n'), "k", 1));
    CHECK(!f.nvs.write<int32_t>(std::string(16, 'n'), "k", 1));
    // empty and embedded-NUL names are rejected
    CHECK(!f.nvs.write<int32_t>("ns", "", 1));
    CHECK(!f.nvs.write<int32_t>("", "k", 1));
    CHECK(!f.nvs.write<int32_t>("ns", std::string_view("a\0b", 3), 1));
    CHECK(f.logged("embedded NUL"));
    CHECK_EQ(f.open_handles(), std::size_t(0));
}

TEST(nvs_type_confusion_reads_default) {
    Fx f;
    CHECK(f.nvs.write<int32_t>("ns", "k", -1));
    CHECK_EQ(f.nvs.read<uint32_t>("ns", "k", 7u), 7u);      // signedness is part of the type
    CHECK_EQ(f.nvs.read<int64_t>("ns", "k", 7), int64_t(7)); // width too
    CHECK_EQ(f.nvs.read<std::string>("ns", "k", "d"), std::string("d"));

    // bool is stored as u8: the two alias each other
    CHECK(f.nvs.write<uint8_t>("ns", "u8", 5));
    CHECK_EQ(f.nvs.read<bool>("ns", "u8", false), true);
    CHECK(f.nvs.write<bool>("ns", "bool", true));
    CHECK_EQ(f.nvs.read<uint8_t>("ns", "bool"), uint8_t(1));

    // float and double are blobs of different size
    CHECK(f.nvs.write<float>("ns", "fl", 1.0f));
    CHECK_EQ(f.nvs.read<double>("ns", "fl", 9.0), 9.0);

    // a write with another type replaces the old entry
    CHECK(f.nvs.write<std::string>("ns", "k", "now a string"));
    CHECK_EQ(f.nvs.read<int32_t>("ns", "k", 3), 3);
    CHECK_EQ(f.nvs.read<std::string>("ns", "k"), std::string("now a string"));
}

TEST(nvs_read_cannot_tell_missing_from_wrong_type_or_error) {
    // read<T> returns default_value for "missing", "wrong type",
    // "namespace never written", "over-long key" and "NVS not initialised" alike.
    Fx f;
    CHECK(f.nvs.write<int32_t>("ns", "wrong", 5));
    const int32_t missing = f.nvs.read<int32_t>("ns", "missing", -9);
    const int32_t wrong   = f.nvs.read<uint8_t>("ns", "wrong", 247);
    const int32_t no_ns   = f.nvs.read<int32_t>("nons", "k", -9);
    CHECK_EQ(missing, -9);
    CHECK_EQ(wrong, 247);
    CHECK_EQ(no_ns, -9);
}

TEST(nvs_string_size_limit) {
    Fx f;
    const std::string s3999(3999, 'x');   // 3999 + NUL = 4000 bytes: the IDF maximum
    CHECK(f.nvs.write<std::string>("ns", "s", s3999));
    CHECK_EQ(f.nvs.read<std::string>("ns", "s"), s3999);
    CHECK(!f.nvs.write<std::string>("ns", "s", std::string(4000, 'y')));
    CHECK_EQ(f.nvs.read<std::string>("ns", "s"), s3999);    // failed write leaves the old value
    CHECK(f.logged("ESP_ERR_NVS_VALUE_TOO_LONG"));
    CHECK_EQ(f.open_handles(), std::size_t(0));
}

TEST(nvs_blobs) {
    Fx f;
    const std::vector<uint8_t> v{1, 2, 0, 255};
    CHECK(f.nvs.write_blob("ns", "b", v));
    CHECK(f.nvs.read_blob("ns", "b") == v);
    const uint8_t raw[] = {9, 8};
    CHECK(f.nvs.write_blob("ns", "s", std::span<const uint8_t>(raw, 2)));
    CHECK_EQ(f.nvs.read_blob("ns", "s").size(), std::size_t(2));
    CHECK(f.nvs.read_blob("ns", "missing").empty());
    // an empty blob reads back the same as a missing one
    CHECK(f.nvs.write_blob("ns", "e", std::vector<uint8_t>{}));
    CHECK(f.nvs.read_blob("ns", "e").empty());
    CHECK_EQ(f.open_handles(), std::size_t(0));
}

TEST(nvs_remove_and_reset_ns) {
    Fx f;
    f.nvs.remove("ns", "nothing");                       // missing key: silent, no handle leak
    f.nvs.reset_ns("never");                            // never-written namespace is not created
    CHECK(host_nvs::state().data.count("never") == 0);
    CHECK(f.nvs.write<int32_t>("a", "k", 1));
    CHECK(f.nvs.write<int32_t>("b", "k", 2));
    f.nvs.remove("a", "k");
    CHECK_EQ(f.nvs.read<int32_t>("a", "k", -1), -1);
    f.nvs.reset_ns("b");
    CHECK_EQ(f.nvs.read<int32_t>("b", "k", -1), -1);
    CHECK_EQ(f.open_handles(), std::size_t(0));
    CHECK(f.errors.empty());
}

TEST(nvs_init_erases_on_no_free_pages_and_reports_it) {
    Fx f;
    host_nvs::state().data["old"]["k"] = host_nvs::Entry{host_nvs::Type::U8, {1}};
    host_nvs::state().init_results = {ESP_ERR_NVS_NO_FREE_PAGES, ESP_OK};
    CHECK(f.nvs.write<int32_t>("ns", "k", 1));
    CHECK_EQ(host_nvs::state().erase_count, 1);
    CHECK(host_nvs::state().data.count("old") == 0);
    CHECK(f.logged("erased"));
}

TEST(nvs_init_failure_fails_every_call_and_reports_once) {
    Fx f;
    host_nvs::state().init_results = {ESP_ERR_NOT_FOUND, ESP_ERR_NOT_FOUND, ESP_ERR_NOT_FOUND};
    CHECK(!f.nvs.write<int32_t>("ns", "k", 1));
    CHECK_EQ(f.nvs.read<int32_t>("ns", "k", 4), 4);
    CHECK(!f.nvs.write<bool>("root", "init_setup_flag", true));   // Os::begin must not restart on this
    CHECK(f.logged("ESP_ERR_NOT_FOUND"));
    CHECK_EQ(f.errors.size(), std::size_t(1));
    // the partition comes back: the next call initialises normally
    CHECK(f.nvs.write<int32_t>("ns", "k", 1));
    CHECK_EQ(f.nvs.read<int32_t>("ns", "k"), 1);
}

TEST(nvs_full_partition_is_reported) {
    Fx f;
    CHECK(f.nvs.write<int32_t>("ns", "k", 1));
    host_nvs::state().full = true;
    CHECK(!f.nvs.write<int32_t>("ns", "k2", 2));            // set fails
    CHECK(f.logged("ESP_ERR_NVS_NOT_ENOUGH_SPACE"));
    f.errors.clear();
    CHECK(!f.nvs.write<int32_t>("newns", "k", 2));          // namespace creation fails
    CHECK(f.logged("newns"));
    CHECK_EQ(f.nvs.read<int32_t>("ns", "k"), 1);            // reads still work
    CHECK_EQ(f.open_handles(), std::size_t(0));
}

TEST(nvs_commit_failure_is_reported) {
    Fx f;
    host_nvs::state().commit_error = ESP_FAIL;
    CHECK(!f.nvs.write<int32_t>("ns", "k", 1));
    CHECK(f.logged("ESP_FAIL"));
    CHECK_EQ(f.open_handles(), std::size_t(0));
}

TEST(nvs_erase_all_wipes_and_stays_usable) {
    Fx f;
    CHECK(f.nvs.write<int32_t>("a", "k", 1));
    CHECK(f.nvs.write<int32_t>("b", "k", 2));
    CHECK(f.nvs.erase_all());
    CHECK_EQ(f.nvs.read<int32_t>("a", "k", -1), -1);
    CHECK(f.nvs.write<int32_t>("a", "k", 3));
    CHECK_EQ(f.nvs.read<int32_t>("a", "k"), 3);
    CHECK_EQ(f.open_handles(), std::size_t(0));
}

TEST(nvs_first_boot_flag_pattern) {
    // Os::begin and Module::begin: "flag missing" == first boot. A namespace that was
    // never written reads as first boot, and an erased partition does too.
    Fx f;
    CHECK(!f.nvs.read<bool>("root", "init_setup_flag"));
    CHECK(f.nvs.write<bool>("root", "init_setup_flag", true));
    CHECK(f.nvs.read<bool>("root", "init_setup_flag"));
    CHECK(f.nvs.erase_all());
    CHECK(!f.nvs.read<bool>("root", "init_setup_flag"));
}

#endif
