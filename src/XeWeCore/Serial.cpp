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
        if (c == '\n') {
            if (input_overflowed) {
                // the whole over-long line is dropped: nothing of it executes
                input_overflowed = false;
                input_buffer_pos = 0;
                this->println_raw("! Input line too long (max 254 chars): dropped");
                continue;
            }
            push_line();
            continue;
        }
        if (input_overflowed) continue;  // discard the rest of an over-long line
        if (input_buffer_pos >= INPUT_BUFFER_SIZE - 1) {
            input_overflowed = true;     // 255th character: mark the line, drop it at '\n'
            continue;
        }
        input_buffer[input_buffer_pos++] = c;
    }
}

void SerialPort::push_line() {
    input_buffer[input_buffer_pos] = '\0';
    if (queue_count >= INPUT_QUEUE_LINES) {
        // queue full: drop the newest line, keep the ones already waiting
        input_buffer_pos = 0;
        this->println_raw("! Input overflow: line dropped");
        return;
    }
    const std::size_t tail = (queue_head + queue_count) % INPUT_QUEUE_LINES;
    std::memcpy(line_queue[tail], input_buffer, input_buffer_pos + 1);
    line_lengths[tail] = static_cast<uint8_t>(input_buffer_pos);
    ++queue_count;
    input_buffer_pos = 0;
}

namespace {

// the layout of print(): each '\n'-separated line (trailing '\r' dropped) is wrapped when
// message_width > 0 and boxed; CRLF between the pieces, `end` after the last one
void append_text(std::string& out,
                 std::string_view message,
                 std::string_view end,
                 std::string_view edge_character,
                 const char text_align,
                 const char wrap_mode,
                 const uint16_t message_width,
                 const uint16_t margin_l,
                 const uint16_t margin_r) {
    const auto lines_sv = xewe::str::split_lines_sv(message, '\n');
    const bool use_wrap = (message_width > 0);

    for (std::size_t i = 0; i < lines_sv.size(); ++i) {
        std::string_view base_line = lines_sv[i];
        if (!base_line.empty() && base_line.back() == '\r') base_line.remove_suffix(1);

        const std::vector<std::string> chunks = !use_wrap ? std::vector<std::string>{std::string(base_line)}
            : (wrap_mode == 'c' || wrap_mode == 'C') ? xewe::str::wrap_fixed(base_line, message_width)
            : xewe::str::wrap_words(base_line, message_width);

        for (std::size_t j = 0; j < chunks.size(); ++j) {
            out += xewe::str::compose_box_line(chunks[j], edge_character, message_width, margin_l, margin_r, text_align);
            const bool is_last = (i + 1 == lines_sv.size()) && (j + 1 == chunks.size());
            out.append(is_last ? end : std::string_view(xewe::str::kCRLF));
        }
    }
}

} // namespace

// printers
void SerialPort::print(std::string_view message,
                       std::string_view end,
                       std::string_view edge_character,
                       const char text_align,
                       const char wrap_mode,
                       const uint16_t message_width,
                       const uint16_t margin_l,
                       const uint16_t margin_r) {
    std::string out;
    append_text(out, message, end, edge_character, text_align, wrap_mode, message_width, margin_l, margin_r);
    print_raw(out);
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
    va_list ap;
    va_start(ap, fmt);
    const std::string msg = xewe::str::vformat(fmt, ap);
    va_end(ap);
    print(msg, end, edge_character, text_align, wrap_mode, message_width, margin_l, margin_r);
}

void SerialPort::printf(const char* fmt,
                        ...) {
    va_list ap;
    va_start(ap, fmt);
    const std::string msg = xewe::str::vformat(fmt, ap);
    va_end(ap);
    print(msg);
}

void SerialPort::print_separator(const uint16_t total_width,
                                 std::string_view fill,
                                 std::string_view edge_character) {
    println_raw(xewe::str::make_rule_line(total_width, fill, edge_character));
}

void SerialPort::print_spacer(const uint16_t total_width,
                              std::string_view edge_character) {
    println_raw(xewe::str::make_spacer_line(total_width, edge_character));
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

    auto append_line_crlf = [&](std::string_view line) {
        output.append(line);
        output.append(xewe::str::kCRLF);
    };

    // column width: the longest line of a multi-line cell plus one space each side, capped
    std::size_t num_cols = 0;
    for (const auto& row : table) num_cols = std::max(num_cols, row.size());

    std::vector<uint16_t> col_widths(num_cols, 0);

    for (const auto& row : table) {
        for (std::size_t c = 0; c < row.size(); ++c) {
            std::size_t max_line_len = 0;
            for (std::string_view line : xewe::str::split_lines_sv(row[c], '\n')) max_line_len = std::max(max_line_len, line.size());

            const std::size_t req_width = std::min<std::size_t>(max_line_len + 2, max_col_width);
            if (req_width > col_widths[c]) col_widths[c] = static_cast<uint16_t>(req_width);
        }
    }

    std::size_t total_table_width = edge_character.size();
    for (const auto width : col_widths) total_table_width += width + edge_character.size();

    // +-----+-----+
    auto append_complex_divider = [&]() {
        std::string line(cross_edge_character);
        for (std::size_t c = 0; c < num_cols; ++c) {
            for (std::size_t k = 0; k < col_widths[c]; ++k) line += sep_fill.empty() ? '-' : sep_fill[k % sep_fill.size()];
            line.append(cross_edge_character);
        }
        append_line_crlf(line);
    };

    // word-wrapped lines of a cell, explicit newlines kept
    auto get_wrapped_lines = [](std::string_view text, uint16_t width) {
        std::vector<std::string> result;
        if (width <= 2) width = 3;
        for (std::string_view segment : xewe::str::split_lines_sv(text, '\n')) {
            std::vector<std::string> segment_lines = xewe::str::wrap_words(segment, width - 2);
            result.insert(result.end(), segment_lines.begin(), segment_lines.end());
        }
        return result;
    };

    if (!header_content.empty()) {
        append_line_crlf(xewe::str::make_rule_line(static_cast<uint16_t>(total_table_width), sep_fill, cross_edge_character));
        append_text(output, header_content, xewe::str::kCRLF, edge_character, 'c', 'w',
            static_cast<uint16_t>(total_table_width - (edge_character.size() * 2)), 0, 0);
    }

    append_complex_divider();

    for (const auto& row : table) {
        std::vector<std::vector<std::string>> row_blocks;
        std::size_t                           max_row_height = 0;

        for (std::size_t c = 0; c < num_cols; ++c) {
            row_blocks.push_back(get_wrapped_lines(c < row.size() ? row[c] : std::string_view{}, col_widths[c]));
            max_row_height = std::max(max_row_height, row_blocks.back().size());
        }

        for (std::size_t h = 0; h < max_row_height; ++h) {
            std::string line_out(edge_character);
            for (std::size_t c = 0; c < num_cols; ++c) {
                const std::string_view segment    = h < row_blocks[c].size() ? std::string_view(row_blocks[c][h]) : std::string_view{};
                const std::size_t      target_len = static_cast<std::size_t>(col_widths[c]) - 2;
                line_out += ' ';
                line_out += segment;
                if (target_len > segment.size()) line_out.append(target_len - segment.size(), ' ');
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
                                    const std::vector<std::string>& options,
                                    const uint8_t min_value,
                                    const uint8_t max_value,
                                    const uint16_t retry_count,
                                    const uint32_t timeout_ms,
                                    const uint8_t default_value,
                                    std::optional<std::reference_wrapper<bool>> success_sink) {
    if (!prompt.empty()) {
        println_raw(prompt);
    }

    uint8_t actual_min = min_value;
    uint8_t actual_max = max_value;

    // with options and the default bounds, number the options from 1 and cap at the last one
    if (!options.empty()) {
        if (actual_min == std::numeric_limits<uint8_t>::min()) {
            actual_min = 1;
        }
        if (actual_max == std::numeric_limits<uint8_t>::max()) {
            // clamp to 255: more options than fit in a uint8_t
            uint16_t calc_max = static_cast<uint16_t>(actual_min) + static_cast<uint16_t>(options.size()) - 1;
            actual_max        = (calc_max > 255) ? 255 : static_cast<uint8_t>(calc_max);
        }
    }

    for (std::size_t i = 0; i < options.size(); ++i) {
        printf_raw("  %u) %s\r\n", static_cast<unsigned>(actual_min + i), options[i].c_str());
    }

    // get_uint8 prints its own prompt line, then "> " on each attempt. Its line is "Choice", or
    // nothing when the caller's prompt is already on screen and there is no menu under it.
    std::string_view input_prompt = (options.empty() && !prompt.empty()) ? "" : "Choice";

    return get_uint8(input_prompt, actual_min, actual_max, retry_count, timeout_ms, default_value, success_sink);
}

bool SerialPort::has_line() const { return queue_count > 0; }

std::string SerialPort::read_line() {
    if (queue_count == 0) return {};
    std::string out(line_queue[queue_head], line_lengths[queue_head]);
    queue_head = static_cast<uint8_t>((queue_head + 1) % INPUT_QUEUE_LINES);
    --queue_count;
    return out;
}

void SerialPort::clear_input() {
    while (Serial.available()) {
        (void)Serial.read();
        yield();
    }
    input_buffer_pos = 0;
    input_overflowed = false;
    queue_head       = 0;
    queue_count      = 0;
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
    va_list ap;
    va_start(ap, fmt);
    print_raw(xewe::str::vformat(fmt, ap));
    va_end(ap);
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
