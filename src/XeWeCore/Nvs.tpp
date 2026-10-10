// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Nvs.tpp
#pragma once


namespace xewe {

template <typename U>
esp_err_t Nvs::set_typed(nvs_handle_t handle, const char* key, const void* value, std::size_t size) {
    if constexpr (std::is_same_v<U, const char*>) {
        return nvs_set_str(handle, key, static_cast<const char*>(value));
    } else if constexpr (std::is_integral_v<U>) {
        const U v = *static_cast<const U*>(value);
        if constexpr (std::is_signed_v<U>) {
            if constexpr (sizeof(U) == 1) return nvs_set_i8(handle, key, static_cast<int8_t>(v));
            else if constexpr (sizeof(U) == 2) return nvs_set_i16(handle, key, static_cast<int16_t>(v));
            else if constexpr (sizeof(U) == 4) return nvs_set_i32(handle, key, static_cast<int32_t>(v));
            else return nvs_set_i64(handle, key, static_cast<int64_t>(v));
        } else {
            if constexpr (sizeof(U) == 1) return nvs_set_u8(handle, key, static_cast<uint8_t>(v));
            else if constexpr (sizeof(U) == 2) return nvs_set_u16(handle, key, static_cast<uint16_t>(v));
            else if constexpr (sizeof(U) == 4) return nvs_set_u32(handle, key, static_cast<uint32_t>(v));
            else return nvs_set_u64(handle, key, static_cast<uint64_t>(v));
        }
    } else {
        return nvs_set_blob(handle, key, value, size);
    }
}

template <typename U>
esp_err_t Nvs::get_typed(nvs_handle_t handle, const char* key, void* out, std::size_t size) {
    if constexpr (std::is_same_v<U, std::string>) {
        std::size_t required = 0;
        esp_err_t   err      = nvs_get_str(handle, key, nullptr, &required);
        if (err != ESP_OK) return err;
        if (required == 0) return ESP_ERR_NVS_NOT_FOUND;
        std::string result(required, '\0');
        err = nvs_get_str(handle, key, &result[0], &required);
        if (err != ESP_OK) return err;
        if (!result.empty() && result.back() == '\0') result.pop_back();
        *static_cast<std::string*>(out) = std::move(result);
        return ESP_OK;
    } else if constexpr (std::is_integral_v<U>) {
        if constexpr (std::is_signed_v<U>) {
            if constexpr (sizeof(U) == 1) return nvs_get_i8(handle, key, static_cast<int8_t*>(out));
            else if constexpr (sizeof(U) == 2) return nvs_get_i16(handle, key, static_cast<int16_t*>(out));
            else if constexpr (sizeof(U) == 4) return nvs_get_i32(handle, key, static_cast<int32_t*>(out));
            else return nvs_get_i64(handle, key, static_cast<int64_t*>(out));
        } else {
            if constexpr (sizeof(U) == 1) return nvs_get_u8(handle, key, static_cast<uint8_t*>(out));
            else if constexpr (sizeof(U) == 2) return nvs_get_u16(handle, key, static_cast<uint16_t*>(out));
            else if constexpr (sizeof(U) == 4) return nvs_get_u32(handle, key, static_cast<uint32_t*>(out));
            else return nvs_get_u64(handle, key, static_cast<uint64_t*>(out));
        }
    } else {
        std::size_t length = size;
        const esp_err_t err = nvs_get_blob(handle, key, out, &length);
        return (err == ESP_OK && length != size) ? ESP_ERR_NVS_INVALID_LENGTH : err;
    }
}

template <typename T>
bool Nvs::write(std::string_view ns,
                std::string_view key,
                const T& value) {
    using U = typename std::decay<T>::type;

    if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, String>) {
        return write_value(ns, key, &set_typed<const char*>, value.c_str(), 0);
    } else if constexpr (std::is_same_v<U, std::string_view>) {
        const std::string text(value);
        return write_value(ns, key, &set_typed<const char*>, text.c_str(), 0);
    } else if constexpr (std::is_convertible_v<U, const char*>) {
        const char* str = value;  // arrays (string literals) decay here; avoids -Waddress
        return write_value(ns, key, &set_typed<const char*>, str ? str : "", 0);
    } else if constexpr (std::is_same_v<U, bool>) {
        const uint8_t raw = value ? 1u : 0u;
        return write_value(ns, key, &set_typed<uint8_t>, &raw, 1);
    } else if constexpr (std::is_integral_v<U> && sizeof(U) <= 8) {
        const U raw = value;
        return write_value(ns, key, &set_typed<U>, &raw, sizeof(U));
    } else if constexpr (std::is_floating_point_v<U>) {
        return write_value(ns, key, &set_typed<U>, &value, sizeof(value));
    } else {
        static_assert(always_false<U>::value, "Unsupported Nvs::write<T>() type.");
        return false;
    }
}

template <typename T>
T Nvs::read(std::string_view ns,
            std::string_view key,
            T default_value) {
    using U = typename std::decay<T>::type;

    if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, String>) {
        std::string result;
        if (!read_value(ns, key, &get_typed<std::string>, &result, 0)) return default_value;
        return U(result.c_str());
    } else if constexpr (std::is_same_v<U, bool>) {
        uint8_t raw = 0;
        if (read_value(ns, key, &get_typed<uint8_t>, &raw, 1)) return raw != 0;
    } else if constexpr (std::is_integral_v<U> && sizeof(U) <= 8) {
        U raw{};
        if (read_value(ns, key, &get_typed<U>, &raw, sizeof(U))) return static_cast<T>(raw);
    } else if constexpr (std::is_floating_point_v<U>) {
        U raw{};
        if (read_value(ns, key, &get_typed<U>, &raw, sizeof(U))) return raw;
    } else {
        static_assert(always_false<U>::value, "Unsupported Nvs::read<T>() type.");
    }
    return default_value;
}

template <typename T>
bool Nvs::write_flex(std::string_view ns, std::string_view key, const T& obj) {
    static_assert(std::is_base_of_v<FlexData<T>, T>, "Nvs::save<T>() requires T : FlexData<T>.");
    return write_blob(ns, key, obj.to_blob());
}

template <typename T>
bool Nvs::read_flex(std::string_view ns, std::string_view key, T& out) {
    static_assert(std::is_base_of_v<FlexData<T>, T>, "Nvs::load<T>() requires T : FlexData<T>.");
    const std::vector<uint8_t> bytes = read_blob(ns, key);
    if (bytes.empty()) return false;
    return out.from_blob(bytes);
}

} // namespace xewe
