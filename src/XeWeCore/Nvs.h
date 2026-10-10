// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Nvs.h
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <Arduino.h>
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>

#include "FlexData.h"


namespace xewe {

class Nvs {
public:
    using error_handler_t        = std::function<void(std::string_view message)>;

    // errors (e.g. an over-long key) go to the ESP log by default; route them elsewhere here
    void                         set_error_handler (error_handler_t handler);

    // atomic types
    template <typename T>
    bool                         write            (std::string_view ns,
                                                   std::string_view key,
                                                   const T&         value);

    template <typename T>
    T                            read             (std::string_view ns,
                                                   std::string_view key,
                                                   T                default_value = T());
    // blobs
    bool                         write_blob       (std::string_view            ns,
                                                   std::string_view            key,
                                                   const std::vector<uint8_t>& data);

    bool                         write_blob       (std::string_view         ns,
                                                   std::string_view         key,
                                                   std::span<const uint8_t> data);

    std::vector<uint8_t>         read_blob        (std::string_view ns,
                                                   std::string_view key);

    // FlexData
    template <typename T>
    bool                         write_flex       (std::string_view ns,
                                                   std::string_view key,
                                                   const T&         obj);

    template <typename T>
    bool                         read_flex        (std::string_view ns,
                                                   std::string_view key,
                                                   T&               out);

    // removal
    void                         remove           (std::string_view ns,
                                                   std::string_view key);
    void                         reset_ns         (std::string_view ns);
    // erase the whole NVS partition (every namespace, including other libraries' data)
    bool                         erase_all        ();

private:
    template <typename>
    struct always_false : std::false_type {};

    static constexpr std::size_t MAX_KEY_LEN      = 15;

    // one typed nvs_set_* / nvs_get_* call; a firmware links only the ones its types use
    using setter_t = esp_err_t (*)(nvs_handle_t handle, const char* key, const void* value, std::size_t size);
    using getter_t = esp_err_t (*)(nvs_handle_t handle, const char* key, void* out, std::size_t size);

    template <typename U>
    static esp_err_t set_typed(nvs_handle_t handle, const char* key, const void* value, std::size_t size);
    template <typename U>
    static esp_err_t get_typed(nvs_handle_t handle, const char* key, void* out, std::size_t size);

    // sanitize the key, open, set, commit; false on a rejected key or any NVS error (reported)
    bool write_value(std::string_view ns, std::string_view key, setter_t set, const void* value, std::size_t size);
    // sanitize the key, open, get; false on a miss, another stored type, a rejected key or an NVS error
    bool read_value(std::string_view ns, std::string_view key, getter_t get, void* out, std::size_t size);

    struct ScopedHandle {
        nvs_handle_t             handle = 0;
        ScopedHandle() = default;
        ScopedHandle(const ScopedHandle&)            = delete;  // a copy would close the handle twice
        ScopedHandle& operator=(const ScopedHandle&) = delete;
        ~ScopedHandle() { close(); }
             operator nvs_handle_t() const { return handle; }
        void close() {
            if (handle != 0) {
                nvs_close(handle);
                handle = 0;
            }
        }
    };

    bool                         m_nvs_ready      = false;
    bool                         m_init_reported  = false;   // report a failing nvs_flash_init once
    error_handler_t              m_error_handler;

    bool                         ensure_ready     ();
    esp_err_t                    open_handle      (std::string_view ns,
                                                   nvs_open_mode_t  mode,
                                                   ScopedHandle&    scoped);
    bool                         commit_and_close (ScopedHandle& scoped,
                                                   esp_err_t     op_err);

    std::string                  sanitize_name    (std::string_view name)          const;
    void                         report_error     (std::string_view message)       const;
    void                         report_esp_error (const char*      what,
                                                   std::string_view name,
                                                   esp_err_t        err)             const;
};

} // namespace xewe

#include "Nvs.tpp"
