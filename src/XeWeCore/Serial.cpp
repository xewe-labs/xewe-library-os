// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Serial.cpp

#include "Serial.h"


namespace xewe {

void SerialPort::begin(const SerialPortConfig& cfg) {
    echo = cfg.echo;
    // ESP32 only: other cores size their UART buffers at build time and simply
    // use their own defaults. The config fields are ignored there.
#if defined(ARDUINO_ARCH_ESP32)
    Serial.setTxBufferSize(cfg.tx_buffer_size);
    Serial.setRxBufferSize(cfg.rx_buffer_size);
#endif
    Serial.begin(cfg.baud_rate);
    delay(cfg.startup_delay_ms);
}

void SerialPort::loop() {
    while (Serial.available()) {
        char c = static_cast<char>(Serial.read());
        yield();

        if (echo) Serial.write(static_cast<uint8_t>(c));

        if (c == '\r') continue;
        if (c == '\n' || input_buffer_pos >= INPUT_BUFFER_SIZE - 1) {
            input_buffer[input_buffer_pos] = '\0';
            line_length                    = input_buffer_pos;
            input_buffer_pos               = 0;
            line_ready                     = true;
        } else {
            input_buffer[input_buffer_pos++] = c;
        }
    }
}

// printers
void SerialPort::print(std::string_view message,
                       std::string_view end,
                       std::string_view edge_character,
                       const char text_align,
                       const char wrap_mode,
                       const uint16_t message_width,
                       const uint16_t margin_l,
                       const uint16_t margin_r) {
    auto       lines_sv = xewe::str::split_lines_sv(message, '\n');
    const bool use_wrap = (message_width > 0);

    for (std::size_t i = 0; i < lines_sv.size(); ++i) {
        std::string base_line(lines_sv[i]);
        xewe::str::rtrim_cr(base_line);

        std::vector<std::string> chunks = use_wrap
                                              ? ((wrap_mode == 'c' || wrap_mode == 'C')
                                                        ? xewe::str::wrap_fixed(base_line, message_width)
                                                        : xewe::str::wrap_words(base_line, message_width)
                                              )
                                              : std::vector<std::string>{base_line};

        for (std::size_t j = 0; j < chunks.size(); ++j) {
            const bool  is_last = (i + 1 == lines_sv.size()) && (j + 1 == chunks.size());
            std::string out     = xewe::str::compose_box_line(chunks[j], edge_character,
                message_width, margin_l, margin_r, text_align
            );
            Serial.write(reinterpret_cast<const uint8_t*>(out.data()), out.size());
            if (is_last) {
                if (!end.empty())
                    Serial.write(reinterpret_cast<const uint8_t*>(end.data()), end.size());
            } else {
                Serial.write(reinterpret_cast<const uint8_t*>(xewe::str::kCRLF), 2);
            }
        }
    }
}

void SerialPort::printf_fmt(std::string_view end,
                            std::string_view edge_character,
                            const char text_align,
                            const char wrap_mode,
                            const uint16_t message_width,
                            const uint16_t margin_l,
                            const uint16_t margin_r,
                            const char* fmt,
                            ...) {
    if (!fmt) {
        print("", end, edge_character, text_align, wrap_mode, message_width, margin_l, margin_r);
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    const int needed = vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);

    std::string msg;
    if (needed > 0) {
        std::vector<char> buf(static_cast<std::size_t>(needed) + 1u);
        vsnprintf(buf.data(), buf.size(), fmt, ap2);
        msg.assign(buf.data(), static_cast<std::size_t>(needed));
    }
    va_end(ap2);

    print(msg, end, edge_character, text_align, wrap_mode, message_width, margin_l, margin_r);
}

void SerialPort::printf(const char* fmt,
                        ...) {
    if (!fmt) {
        print();
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    const int needed = vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);

    std::string msg;
    if (needed > 0) {
        std::vector<char> buf(static_cast<std::size_t>(needed) + 1u);
        vsnprintf(buf.data(), buf.size(), fmt, ap2);
        msg.assign(buf.data(), static_cast<std::size_t>(needed));
    }
    va_end(ap2);

    print(msg);
}

void SerialPort::print_separator(const uint16_t total_width,
                                 std::string_view fill,
                                 std::string_view edge_character) {
    std::string line;
    if (total_width == 0) {
        line.clear();
    } else if (edge_character.empty()) {
        // Full-width fill pattern
        line = xewe::str::repeat_pattern(fill, total_width);
    } else {
        const std::size_t e = edge_character.size();
        if (total_width <= e) {
            line.assign(edge_character.substr(0, total_width));
        } else if (total_width <= 2 * e) {
            // Not enough room for interior
            line.assign(edge_character.substr(0, total_width));
        } else {
            const uint16_t inner = static_cast<uint16_t>(total_width - 2 * e);
            line.reserve(total_width);
            line.append(edge_character);
            line += xewe::str::repeat_pattern(fill, inner);
            line.append(edge_character);
        }
    }
    write_line_crlf(line);
}

void SerialPort::print_spacer(const uint16_t total_width,
                              std::string_view edge_character) {
    std::string line;
    if (total_width == 0) {
        line.clear();
    } else if (edge_character.empty()) {
        line.assign(static_cast<std::size_t>(total_width), ' ');
    } else {
        const std::size_t e = edge_character.size();
        if (total_width <= e) {
            line.assign(edge_character.substr(0, total_width));
        } else if (total_width <= 2 * e) {
            line.assign(edge_character.substr(0, total_width));
        } else {
            const uint16_t inner = static_cast<uint16_t>(total_width - 2 * e);
            line.reserve(total_width);
            line.append(edge_character);
            line.append(inner, ' ');
            line.append(edge_character);
        }
    }
    write_line_crlf(line);
}

void SerialPort::print_header(std::string_view message,
                              const uint16_t total_width,
                              std::string_view edge_character,
                              std::string_view cross_edge_character,
                              std::string_view sep_fill) {
    print_separator(total_width, sep_fill, cross_edge_character);

    auto           parts  = xewe::str::split_by_token(message, "\\sep");
    const uint16_t edge_w = static_cast<uint16_t>(edge_character.size() * 2) + 2;
    const uint16_t content_width =
        (!edge_character.empty() && total_width > edge_w)
            ? static_cast<uint16_t>(total_width - edge_w)
            : total_width;

    for (auto& p : parts) {
        print(p, xewe::str::kCRLF, edge_character, 'c', 'w', content_width, 1, 1);
        print_separator(total_width, sep_fill, cross_edge_character);
    }
}

void SerialPort::print_table(const std::vector<std::vector<std::string_view>>& table,
                             std::string_view header_content,
                             const uint16_t max_col_width,
                             std::string_view edge_character,
                             std::string_view cross_edge_character,
                             std::string_view sep_fill) {
    print_raw(render_table(table,
        header_content,
        max_col_width,
        edge_character,
        cross_edge_character,
        sep_fill
    ));
}

std::string SerialPort::render_table(const std::vector<std::vector<std::string_view>>& table,
                                     std::string_view header_content,
                                     const uint16_t max_col_width,
                                     std::string_view edge_character,
                                     std::string_view cross_edge_character,
                                     std::string_view sep_fill) const {
    if (table.empty()) return {};

    std::string output;

    auto        append_line_crlf = [&](std::string_view line) {
        output.append(line.data(), line.size());
        output.append(xewe::str::kCRLF);
    };

    auto append_separator = [&](const uint16_t   total_width,
                                std::string_view fill,
                                std::string_view separator_edge) {
        std::string line;
        if (total_width == 0) {
            line.clear();
        } else if (separator_edge.empty()) {
            line = xewe::str::repeat_pattern(fill, total_width);
        } else {
            const std::size_t e = separator_edge.size();
            if (total_width <= e) {
                line.assign(separator_edge.substr(0, total_width));
            } else if (total_width <= 2 * e) {
                line.assign(separator_edge.substr(0, total_width));
            } else {
                const uint16_t inner = static_cast<uint16_t>(total_width - 2 * e);
                line.reserve(total_width);
                line.append(separator_edge);
                line += xewe::str::repeat_pattern(fill, inner);
                line.append(separator_edge);
            }
        }
        append_line_crlf(line);
    };

    auto append_formatted = [&](std::string_view message,
                                std::string_view end,
                                std::string_view line_edge,
                                const char       text_align,
                                const char       wrap_mode,
                                const uint16_t   message_width,
                                const uint16_t   margin_l,
                                const uint16_t   margin_r) {
        auto       lines_sv = xewe::str::split_lines_sv(message, '\n');
        const bool use_wrap = (message_width > 0);

        for (std::size_t i = 0; i < lines_sv.size(); ++i) {
            std::string base_line(lines_sv[i]);
            xewe::str::rtrim_cr(base_line);

            std::vector<std::string> chunks = use_wrap
                                                  ? ((wrap_mode == 'c' || wrap_mode == 'C')
                                                            ? xewe::str::wrap_fixed(base_line, message_width)
                                                            : xewe::str::wrap_words(base_line, message_width)
                                                  )
                                                  : std::vector<std::string>{base_line};

            for (std::size_t j = 0; j < chunks.size(); ++j) {
                const bool is_last =
                    (i + 1 == lines_sv.size()) && (j + 1 == chunks.size());

                output += xewe::str::compose_box_line(chunks[j],
                    line_edge,
                    message_width,
                    margin_l,
                    margin_r,
                    text_align
                );

                if (is_last) {
                    output.append(end.data(), end.size());
                } else {
                    output.append(xewe::str::kCRLF);
                }
            }
        }
    };

    // 1. Calculate Column Widths
    // The column must be wide enough for the longest line in a multi-line cell,
    // not merely the total length of the cell string.
    std::size_t num_cols = 0;
    for (const auto& row : table) num_cols = std::max(num_cols, row.size());

    std::vector<uint16_t> col_widths(num_cols, 0);

    for (const auto& row : table) {
        for (std::size_t c = 0; c < row.size(); ++c) {
            std::string_view cell         = row[c];
            std::size_t      max_line_len = 0;

            std::size_t      start        = 0;
            while (start <= cell.length()) {
                std::size_t end = cell.find('\n', start);
                if (end == std::string_view::npos) end = cell.length();

                const std::size_t segment_len = end - start;
                if (segment_len > max_line_len) max_line_len = segment_len;

                if (end == cell.length()) break;
                start = end + 1;
            }

            std::size_t req_width = max_line_len + 2;
            if (req_width > max_col_width) req_width = max_col_width;

            if (req_width > col_widths[c]) {
                col_widths[c] = static_cast<uint16_t>(req_width);
            }
        }
    }

    // 2. Calculate Total Table Width
    std::size_t total_table_width = edge_character.size();
    for (const auto width : col_widths) {
        total_table_width += width + edge_character.size();
    }

    // Helper: append a complex divider such as +-----+-----+.
    auto append_complex_divider = [&]() {
        std::string line;
        line.reserve(total_table_width);
        line.append(cross_edge_character);

        for (std::size_t c = 0; c < num_cols; ++c) {
            for (std::size_t k = 0; k < col_widths[c]; ++k) {
                if (!sep_fill.empty()) {
                    line += sep_fill[k % sep_fill.size()];
                } else {
                    line += '-';
                }
            }
            line.append(cross_edge_character);
        }

        append_line_crlf(line);
    };

    // Helper: wrap text while respecting explicit newlines.
    auto get_wrapped_lines = [&](std::string_view text,
                                 uint16_t         width) -> std::vector<std::string> {
        std::vector<std::string> result;
        if (width <= 2) width = 3;
        const uint16_t content_width = width - 2;

        std::size_t    start         = 0;
        if (text.empty()) return {""};

        while (start <= text.length()) {
            std::size_t end = text.find('\n', start);
            if (end == std::string_view::npos) end = text.length();

            const std::string_view segment = text.substr(start, end - start);

            if (segment.empty()) {
                result.push_back("");
            } else {
                std::vector<std::string> segment_lines =
                    xewe::str::wrap_words(std::string(segment), content_width);

                if (segment_lines.empty()) {
                    result.push_back("");
                } else {
                    result.insert(result.end(),
                        segment_lines.begin(),
                        segment_lines.end()
                    );
                }
            }

            if (end == text.length()) break;
            start = end + 1;
        }

        return result;
    };

    // 3. Render Header
    if (!header_content.empty()) {
        append_separator(static_cast<uint16_t>(total_table_width),
            sep_fill,
            cross_edge_character
        );

        const uint16_t header_content_width = static_cast<uint16_t>(
            total_table_width - (edge_character.size() * 2)
        );

        append_formatted(header_content,
            xewe::str::kCRLF,
            edge_character,
            'c',
            'w',
            header_content_width,
            0,
            0
        );
    }

    // 4. Render Table Body
    append_complex_divider();

    for (const auto& row : table) {
        std::vector<std::vector<std::string>> row_blocks;
        std::size_t                           max_row_height = 0;

        for (std::size_t c = 0; c < num_cols; ++c) {
            const std::string_view   entry = (c < row.size()) ? row[c] : "";
            std::vector<std::string> wrapped =
                get_wrapped_lines(entry, col_widths[c]);

            if (wrapped.empty()) wrapped.push_back("");

            max_row_height = std::max(max_row_height, wrapped.size());
            row_blocks.push_back(std::move(wrapped));
        }

        for (std::size_t h = 0; h < max_row_height; ++h) {
            std::string line_out;
            line_out.reserve(total_table_width);
            line_out.append(edge_character);

            for (std::size_t c = 0; c < num_cols; ++c) {
                const std::string segment =
                    (h < row_blocks[c].size()) ? row_blocks[c][h] : "";

                line_out += ' ';
                line_out += segment;

                const std::size_t current_len = segment.length();
                const std::size_t target_len =
                    static_cast<std::size_t>(col_widths[c]) - 2;

                if (target_len > current_len) {
                    line_out.append(target_len - current_len, ' ');
                }

                line_out += ' ';
                line_out += edge_character;
            }

            append_line_crlf(line_out);
        }

        append_complex_divider();
    }

    return output;
}

// getters
std::string SerialPort::get_string(std::string_view prompt,
                                   const uint16_t min_length,
                                   const uint16_t max_length,
                                   const uint16_t retry_count,
                                   const uint32_t timeout_ms,
                                   std::string_view default_value,
                                   std::optional<std::reference_wrapper<bool>> success_sink) {
    const std::size_t min_len = static_cast<std::size_t>(min_length);
    const std::size_t max_len = (max_length == 0) ? (INPUT_BUFFER_SIZE - 1)
                                                  : static_cast<std::size_t>(max_length);

    auto              checker = [&](const std::string& line, std::string& out, const char*& err) -> bool {
        if (line.size() < min_len || line.size() > max_len) {
            printf_raw("! Length must be in [%u..%u] chars.\r\n",
                static_cast<unsigned>(min_len),
                static_cast<unsigned>(max_len)
            );
            err = nullptr;
            return false;
        }
        out = line;
        return true;
    };

    return get_core<std::string>(prompt, retry_count, timeout_ms, std::string(default_value),
        success_sink, "> ", /*crlf*/ true, checker
    );
}

int SerialPort::get_int(std::string_view prompt,
                        const int min_value,
                        const int max_value,
                        const uint16_t retry_count,
                        const uint32_t timeout_ms,
                        const int default_value,
                        std::optional<std::reference_wrapper<bool>> success_sink) {
    return get_integral<int>(prompt, min_value, max_value, retry_count, timeout_ms, default_value, success_sink);
}

uint8_t SerialPort::get_uint8(std::string_view prompt,
                              const uint8_t min_value,
                              const uint8_t max_value,
                              const uint16_t retry_count,
                              const uint32_t timeout_ms,
                              const uint8_t default_value,
                              std::optional<std::reference_wrapper<bool>> success_sink) {
    return get_integral<uint8_t>(prompt, min_value, max_value, retry_count, timeout_ms, default_value, success_sink);
}

uint16_t SerialPort::get_uint16(std::string_view prompt,
                                const uint16_t min_value,
                                const uint16_t max_value,
                                const uint16_t retry_count,
                                const uint32_t timeout_ms,
                                const uint16_t default_value,
                                std::optional<std::reference_wrapper<bool>> success_sink) {
    return get_integral<uint16_t>(prompt, min_value, max_value, retry_count, timeout_ms, default_value, success_sink);
}

uint32_t SerialPort::get_uint32(std::string_view prompt,
                                const uint32_t min_value,
                                const uint32_t max_value,
                                const uint16_t retry_count,
                                const uint32_t timeout_ms,
                                const uint32_t default_value,
                                std::optional<std::reference_wrapper<bool>> success_sink) {
    return get_integral<uint32_t>(prompt, min_value, max_value, retry_count, timeout_ms, default_value, success_sink);
}

float SerialPort::get_float(std::string_view prompt,
                            const float min_value,
                            const float max_value,
                            const uint16_t retry_count,
                            const uint32_t timeout_ms,
                            const float default_value,
                            std::optional<std::reference_wrapper<bool>> success_sink) {
    float minv = min_value, maxv = max_value;
    if (minv > maxv) std::swap(minv, maxv);

    auto checker = [&](const std::string& line, float& out, const char*& err) -> bool {
        const char* s   = line.c_str();
        char*       end = nullptr;
        double      dv  = strtod(s, &end);
        while (end && *end == ' ') ++end;
        if (s == end || (end && *end != '\0')) {
            err = "! Invalid number. Please enter a decimal value.";
            return false;
        }
        if (dv != dv) {
            err = "! Invalid number.";
            return false;
        } // NaN
        float v = static_cast<float>(dv);
        if (v < minv || v > maxv) {
            printf_raw("! Out of range [%g..%g].\r\n",
                static_cast<double>(minv),
                static_cast<double>(maxv)
            );
            err = nullptr;
            return false;
        }
        out = v;
        return true;
    };

    return get_core<float>(prompt, retry_count, timeout_ms, default_value,
        success_sink, "> ", /*crlf*/ true, checker
    );
}

bool SerialPort::get_yn(std::string_view prompt,
                        const uint16_t retry_count,
                        const uint32_t timeout_ms,
                        const bool default_value,
                        std::optional<std::reference_wrapper<bool>> success_sink) {
    auto checker = [&](const std::string& line, bool& out, const char*& err) -> bool {
        std::string low = xewe::str::to_lower(line);
        if (low == "y" || low == "yes" || low == "1" || low == "true") {
            out = true;
            return true;
        }
        if (low == "n" || low == "no" || low == "0" || low == "false") {
            out = false;
            return true;
        }
        err = "! Please answer 'y' or 'n'.";
        return false;
    };

    return get_core<bool>(prompt, retry_count, timeout_ms, default_value,
        success_sink, "(y/n) > ", /*crlf*/ true, checker
    );
}

uint8_t SerialPort::get_menu_choice(std::string_view prompt,
                                    const std::vector<std::string> options,
                                    const uint8_t min_value,
                                    const uint8_t max_value,
                                    const uint16_t retry_count,
                                    const uint32_t timeout_ms,
                                    const uint8_t default_value,
                                    std::optional<std::reference_wrapper<bool>> success_sink) {
    // 1. Display the prompt if provided
    if (!prompt.empty()) {
        println_raw(prompt);
    }

    // 2. Determine effective boundaries
    uint8_t actual_min = min_value;
    uint8_t actual_max = max_value;

    // Auto-adjust bounds if defaults were left untouched but options were provided
    if (!options.empty()) {
        if (actual_min == std::numeric_limits<uint8_t>::min()) {
            actual_min = 1; // Default to 1-based indexing for menus
        }
        if (actual_max == std::numeric_limits<uint8_t>::max()) {
            // Prevent overflow if actual_min + options.size() exceeds uint8_t max
            uint16_t calc_max = static_cast<uint16_t>(actual_min) + static_cast<uint16_t>(options.size()) - 1;
            actual_max        = (calc_max > 255) ? 255 : static_cast<uint8_t>(calc_max);
        }
    }

    // 3. Display the menu options (if any)
    for (std::size_t i = 0; i < options.size(); ++i) {
        printf_raw("  %u) %s\r\n", static_cast<unsigned>(actual_min + i), options[i].c_str());
    }

    // 4. Delegate to get_uint8
    // If we only have a prompt and no options, don't double-prompt "Choice >".
    // Just use empty string so the user sees " > " right under their custom prompt.
    std::string_view input_prompt = (options.empty() && !prompt.empty()) ? "" : "Choice";

    return get_uint8(input_prompt, actual_min, actual_max, retry_count, timeout_ms, default_value, success_sink);
}

bool SerialPort::has_line() const { return line_ready; }

std::string SerialPort::read_line() {
    if (!line_ready) return {};
    std::string out(input_buffer, line_length);
    line_ready       = false;
    line_length      = 0;
    input_buffer_pos = 0;
    //     input_buffer[0]  = '\0';
    return out;
}

void SerialPort::clear_input() {
    while (Serial.available()) {
        (void)Serial.read();
        yield();
    }
    input_buffer_pos = 0;
    line_length      = 0;
    line_ready       = false;
    //     input_buffer[0]  = '\0';
}

void SerialPort::print_raw(std::string_view message) {
    Serial.write(reinterpret_cast<const uint8_t*>(message.data()), message.size());
}

void SerialPort::println_raw(std::string_view message) {
    Serial.write(reinterpret_cast<const uint8_t*>(message.data()), message.size());
    Serial.write(reinterpret_cast<const uint8_t*>(xewe::str::kCRLF), 2);
}

void SerialPort::printf_raw(const char* fmt,
                            ...) {
    if (!fmt) return;

    bool has_spec = false;
    for (const char* p = fmt; *p; ++p) {
        if (*p == '%') {
            if (*(p + 1) == '%') {
                ++p;
                continue;
            }
            has_spec = true;
            break;
        }
    }
    if (!has_spec) {
        std::size_t n = strlen(fmt);
        if (n) Serial.write(reinterpret_cast<const uint8_t*>(fmt), n);
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);

    if (needed <= 0) {
        va_end(ap2);
        return;
    }

    std::vector<char> buf(static_cast<std::size_t>(needed) + 1u);
    vsnprintf(buf.data(), buf.size(), fmt, ap2);
    va_end(ap2);

    Serial.write(reinterpret_cast<const uint8_t*>(buf.data()), static_cast<std::size_t>(needed));
}

bool SerialPort::read_line_with_timeout(std::string& out,
                                        const uint32_t timeout_ms) {
    uint32_t start = millis();
    for (;;) {
        loop();
        if (has_line()) {
            out = read_line();
            return true;
        }
        if (timeout_ms != 0 && (millis() - start >= timeout_ms)) {
            return false;
        }
        yield();
    }
}

void SerialPort::write_line_crlf(std::string_view s) {
    Serial.write(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    Serial.write(reinterpret_cast<const uint8_t*>(xewe::str::kCRLF), 2);
}

template <typename T>
T SerialPort::get_integral(std::string_view prompt,
                           const T min_value,
                           const T max_value,
                           const uint16_t retry_count,
                           const uint32_t timeout_ms,
                           const T default_value,
                           std::optional<std::reference_wrapper<bool>> success_sink) {
    T minv = min_value, maxv = max_value;
    if (minv > maxv) std::swap(minv, maxv);

    auto checker = [&](const std::string& line, T& out, const char*& err) -> bool {
        T v{};
        if (!xewe::str::parse_int<T>(line, v)) {
            err = "! Invalid number. Please enter a base-10 integer.";
            return false;
        }
        if (v < minv || v > maxv) {
            printf_raw("! Out of range [%lld..%lld].\r\n",
                static_cast<long long>(minv),
                static_cast<long long>(maxv)
            );
            err = nullptr;
            return false;
        }
        out = v;
        return true;
    };

    return get_core<T>(prompt, retry_count, timeout_ms, default_value, success_sink, "> ", true, checker);
}

} // namespace xewe
