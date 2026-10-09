// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Nvs.tpp
#pragma once



namespace xewe {

template <typename T>
bool Nvs::write(std::string_view ns,
                std::string_view key,
                const T& value) {
    using U = typename std::decay<T>::type;

    const std::string storage_key = sanitize_name(key);
    if (storage_key.empty()) return false;

    ScopedHandle sh;
    const esp_err_t open_err = open_handle(ns, NVS_READWRITE, sh);
    if (open_err != ESP_OK) return false;

    esp_err_t write_err = ESP_ERR_INVALID_ARG;

    if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, String>) {
        write_err = nvs_set_str(sh, storage_key.c_str(), value.c_str());
    } else if constexpr (std::is_same_v<U, std::string_view>) {
        write_err = nvs_set_str(sh, storage_key.c_str(), std::string(value).c_str());
    } else if constexpr (std::is_convertible_v<U, const char*>) {
        const char* str = value;  // arrays (string literals) decay here; avoids -Waddress
        write_err = nvs_set_str(sh, storage_key.c_str(), str ? str : "");
    } else if constexpr (std::is_same_v<U, bool>) {
        write_err = nvs_set_u8(sh, storage_key.c_str(), value ? 1u : 0u);
    } else if constexpr (std::is_integral_v<U> && std::is_signed_v<U>) {
        if constexpr (sizeof(U) == 1)      write_err = nvs_set_i8(sh, storage_key.c_str(), static_cast<int8_t>(value));
        else if constexpr (sizeof(U) == 2) write_err = nvs_set_i16(sh, storage_key.c_str(), static_cast<int16_t>(value));
        else if constexpr (sizeof(U) == 4) write_err = nvs_set_i32(sh, storage_key.c_str(), static_cast<int32_t>(value));
        else if constexpr (sizeof(U) == 8) write_err = nvs_set_i64(sh, storage_key.c_str(), static_cast<int64_t>(value));
    } else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U>) {
        if constexpr (sizeof(U) == 1)      write_err = nvs_set_u8(sh, storage_key.c_str(), static_cast<uint8_t>(value));
        else if constexpr (sizeof(U) == 2) write_err = nvs_set_u16(sh, storage_key.c_str(), static_cast<uint16_t>(value));
        else if constexpr (sizeof(U) == 4) write_err = nvs_set_u32(sh, storage_key.c_str(), static_cast<uint32_t>(value));
        else if constexpr (sizeof(U) == 8) write_err = nvs_set_u64(sh, storage_key.c_str(), static_cast<uint64_t>(value));
    } else if constexpr (std::is_floating_point_v<U>) {
        write_err = nvs_set_blob(sh, storage_key.c_str(), &value, sizeof(value));
    } else {
        static_assert(always_false<U>::value, "Unsupported Nvs::write<T>() type.");
    }

    return commit_and_close(sh, write_err);
}

template <typename T>
T Nvs::read(std::string_view ns,
            std::string_view key,
            T default_value) {
    using U = typename std::decay<T>::type;

    const std::string storage_key = sanitize_name(key);
    if (storage_key.empty()) return default_value;

    ScopedHandle sh;
    const esp_err_t open_err = open_handle(ns, NVS_READONLY, sh);
    if (open_err != ESP_OK) return default_value;

    if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, String>) {
        std::size_t required = 0;
        esp_err_t read_err = nvs_get_str(sh, storage_key.c_str(), nullptr, &required);
        if (read_err != ESP_OK || required == 0) return default_value;

        std::string result(required, '\0');
        read_err = nvs_get_str(sh, storage_key.c_str(), &result[0], &required);

        if (read_err != ESP_OK) return default_value;

        if (!result.empty() && result.back() == '\0') {
            result.pop_back();
        }

        return U(result.c_str());

    } else if constexpr (std::is_same_v<U, bool>) {
        uint8_t raw = default_value ? 1u : 0u;
        if (nvs_get_u8(sh, storage_key.c_str(), &raw) == ESP_OK) {
            return raw != 0;
        }
    } else if constexpr (std::is_integral_v<U> && std::is_signed_v<U>) {
        if constexpr (sizeof(U) == 1) {
            int8_t r = static_cast<int8_t>(default_value);
            if (nvs_get_i8(sh, storage_key.c_str(), &r) == ESP_OK) return static_cast<T>(r);
        } else if constexpr (sizeof(U) == 2) {
            int16_t r = static_cast<int16_t>(default_value);
            if (nvs_get_i16(sh, storage_key.c_str(), &r) == ESP_OK) return static_cast<T>(r);
        } else if constexpr (sizeof(U) == 4) {
            int32_t r = static_cast<int32_t>(default_value);
            if (nvs_get_i32(sh, storage_key.c_str(), &r) == ESP_OK) return static_cast<T>(r);
        } else if constexpr (sizeof(U) == 8) {
            int64_t r = static_cast<int64_t>(default_value);
            if (nvs_get_i64(sh, storage_key.c_str(), &r) == ESP_OK) return static_cast<T>(r);
        }
    } else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U>) {
        if constexpr (sizeof(U) == 1) {
            uint8_t r = static_cast<uint8_t>(default_value);
            if (nvs_get_u8(sh, storage_key.c_str(), &r) == ESP_OK) return static_cast<T>(r);
        } else if constexpr (sizeof(U) == 2) {
            uint16_t r = static_cast<uint16_t>(default_value);
            if (nvs_get_u16(sh, storage_key.c_str(), &r) == ESP_OK) return static_cast<T>(r);
        } else if constexpr (sizeof(U) == 4) {
            uint32_t r = static_cast<uint32_t>(default_value);
            if (nvs_get_u32(sh, storage_key.c_str(), &r) == ESP_OK) return static_cast<T>(r);
        } else if constexpr (sizeof(U) == 8) {
            uint64_t r = static_cast<uint64_t>(default_value);
            if (nvs_get_u64(sh, storage_key.c_str(), &r) == ESP_OK) return static_cast<T>(r);
        }
    } else if constexpr (std::is_floating_point_v<U>) {
        U result = default_value;
        std::size_t size = sizeof(result);
        if (nvs_get_blob(sh, storage_key.c_str(), &result, &size) == ESP_OK && size == sizeof(result)) {
            return result;
        }
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
