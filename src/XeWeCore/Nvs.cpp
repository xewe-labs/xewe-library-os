// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Nvs.cpp

#include "Nvs.h"

#include <esp_log.h>


namespace xewe {

void Nvs::set_error_handler(error_handler_t handler) {
    m_error_handler = std::move(handler);
}

bool Nvs::erase_all() {
    (void)nvs_flash_deinit();
    m_nvs_ready               = false;

    const esp_err_t erase_err = nvs_flash_erase();
    if (erase_err != ESP_OK) return false;
    return ensure_ready();
}

bool Nvs::write_blob(std::string_view ns,
                     std::string_view key,
                     const std::vector<uint8_t>& data) {
    std::string storage_key = sanitize_name(key);
    if (storage_key.empty()) return false;

    ScopedHandle    sh;
    const esp_err_t open_err = open_handle(ns, NVS_READWRITE, sh);
    if (open_err != ESP_OK) return false;

    const esp_err_t write_err = nvs_set_blob(sh, storage_key.c_str(), data.data(), data.size());

    return commit_and_close(sh, write_err);
}

bool Nvs::write_blob(std::string_view ns,
                     std::string_view key,
                     std::span<const uint8_t> data) {
    return write_blob(ns, key, std::vector<uint8_t>(data.begin(), data.end()));
}

std::vector<uint8_t> Nvs::read_blob(std::string_view ns,
                                    std::string_view key) {
    std::vector<uint8_t> out;

    std::string          storage_key = sanitize_name(key);
    if (storage_key.empty()) return out;

    ScopedHandle    sh;
    const esp_err_t open_err = open_handle(ns, NVS_READONLY, sh);
    if (open_err != ESP_OK) return out;

    std::size_t required = 0;
    esp_err_t   read_err = nvs_get_blob(sh, storage_key.c_str(), nullptr, &required);
    if (read_err != ESP_OK || required == 0) return out;

    out.resize(required);
    read_err = nvs_get_blob(sh, storage_key.c_str(), out.data(), &required);
    if (read_err != ESP_OK) return out;

    out.resize(required);
    return out;
}

void Nvs::remove(std::string_view ns,
                 std::string_view key) {
    std::string storage_key = sanitize_name(key);
    if (storage_key.empty()) return;

    ScopedHandle    sh;
    const esp_err_t open_err = open_handle(ns, NVS_READWRITE, sh);
    if (open_err != ESP_OK) return;

    const esp_err_t erase_err = nvs_erase_key(sh, storage_key.c_str());
    if (erase_err == ESP_ERR_NVS_NOT_FOUND) return;

    (void)commit_and_close(sh, erase_err);
}

void Nvs::reset_ns(std::string_view ns) {
    // probe read-only first: a namespace that was never written does not exist,
    // and we must not create it just to erase it (read-only never creates one).
    ScopedHandle    sh;
    const esp_err_t probe_err = open_handle(ns, NVS_READONLY, sh);
    if (probe_err != ESP_OK) return;

    // reopen read-write (open_handle closes the read-only handle first).
    const esp_err_t open_err = open_handle(ns, NVS_READWRITE, sh);
    if (open_err != ESP_OK) return;

    const esp_err_t erase_err = nvs_erase_all(sh);

    if (!commit_and_close(sh, erase_err)) return;
}

bool Nvs::ensure_ready() {
    if (m_nvs_ready) return true;

    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // the standard ESP-IDF recovery wipes every namespace (Wi-Fi credentials included): say so
        report_esp_error("partition unusable, erased all NVS data", {}, err);
        (void)nvs_flash_deinit();
        const esp_err_t erase_err = nvs_flash_erase();
        if (erase_err != ESP_OK) {
            report_esp_error("partition erase failed", {}, erase_err);
            return false;
        }
        err = nvs_flash_init();
    }

    if (err == ESP_OK) {
        m_nvs_ready     = true;
        m_init_reported = false;
        return true;
    }

    if (!m_init_reported) {
        m_init_reported = true;
        report_esp_error("nvs_flash_init failed; all reads return defaults, writes fail", {}, err);
    }
    return false;
}

esp_err_t Nvs::open_handle(std::string_view ns,
                           nvs_open_mode_t mode,
                           ScopedHandle& scoped) {
    scoped.close();

    if (!ensure_ready()) return ESP_FAIL;

    const std::string namespace_name = sanitize_name(ns);
    if (namespace_name.empty()) return ESP_ERR_INVALID_ARG;

    const esp_err_t err = nvs_open(namespace_name.c_str(), mode, &scoped.handle);
    // a read-only open of a namespace that was never written is the normal "missing" case
    if (err != ESP_OK && !(mode == NVS_READONLY && err == ESP_ERR_NVS_NOT_FOUND)) {
        report_esp_error("open namespace failed", namespace_name, err);
    }
    return err;
}

bool Nvs::commit_and_close(ScopedHandle& scoped,
                           esp_err_t op_err) {
    if (op_err != ESP_OK) {
        report_esp_error("write failed", {}, op_err);
        scoped.close();
        return false;
    }

    const esp_err_t commit_err = nvs_commit(scoped);
    scoped.close();

    if (commit_err != ESP_OK) {
        report_esp_error("commit failed", {}, commit_err);
        return false;
    }
    return true;
}

std::string Nvs::sanitize_name(std::string_view name) const {
    if (name.empty()) return {};

    std::string out;
    out.append(name.data(), name.size());

    if (name.find('\0') != std::string_view::npos) {
        report_error("Nvs: ERROR name contains an embedded NUL; rejected");
        return {};
    }

    if (out.size() > MAX_KEY_LEN) {
        char msg[96];
        snprintf(msg,
                 sizeof(msg),
                 "Nvs: ERROR name '%s' too long (%u chars > %u max); rejected",
                 out.c_str(),
                 static_cast<unsigned>(out.size()),
                 static_cast<unsigned>(MAX_KEY_LEN));
        report_error(msg);
        return {};
    }
    return out;
}

void Nvs::report_error(std::string_view message) const {
    if (m_error_handler) {
        m_error_handler(message);
        return;
    }
    ESP_LOGE("XeWeNvs", "%.*s", static_cast<int>(message.size()), message.data());
}

void Nvs::report_esp_error(const char*      what,
                           std::string_view name,
                           esp_err_t        err) const {
    char msg[128];
    if (name.empty()) {
        snprintf(msg, sizeof(msg), "Nvs: ERROR %s (%s)", what, esp_err_to_name(err));
    } else {
        snprintf(msg, sizeof(msg), "Nvs: ERROR %s '%.*s' (%s)", what,
                 static_cast<int>(name.size()), name.data(), esp_err_to_name(err));
    }
    report_error(msg);
}

} // namespace xewe
