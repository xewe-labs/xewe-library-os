// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Utils/String.h
#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <algorithm>
#include <limits>
#include <type_traits>
#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>


#define STRINGIFY_XEWE(x) #x
#define TO_STRING(x)      STRINGIFY_XEWE(x)

namespace xewe::str {

inline std::string lower(std::string s) {
    std::transform(
        s.begin(), s.end(),
        s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); }
    );
    return s;
}

inline std::string upper(std::string s) {
    std::transform(
        s.begin(), s.end(),
        s.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); }
    );
    return s;
}

inline std::string capitalize(std::string s) {
    bool new_word = true;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (std::isalnum(c)) {
            s[i]     = static_cast<char>(new_word ? std::toupper(c) : std::tolower(c));
            new_word = false;
        } else {
            new_word = true;
        }
    }
    return s;
}

inline std::string to_hex(const uint8_t* b, size_t n) {
    static const char* k = "0123456789ABCDEF";
    std::string        s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; i++) {
        s.push_back(k[b[i] >> 4]);
        s.push_back(k[b[i] & 0x0F]);
    }
    return s;
}

// --------------------------------------------------------------------------------------
// Hex colour strings: "rrggbb" or "#rrggbb", exactly six hex digits (either case)
// --------------------------------------------------------------------------------------

// true and r/g/b set on success; false and r/g/b untouched on anything else
// ("#ff", "ff00zz", 7 digits, "", "0xff0000", spaces, a sign)
inline bool parse_hex_color(const std::string& s, uint8_t& r, uint8_t& g, uint8_t& b) {
    const size_t start = (!s.empty() && s[0] == '#') ? 1 : 0;
    if (s.size() - start != 6) return false;
    uint8_t v[3] = {};
    for (size_t i = 0; i < 6; ++i) {
        const char c = s[start + i];
        uint8_t    d;
        if (c >= '0' && c <= '9') d = static_cast<uint8_t>(c - '0');
        else if (c >= 'a' && c <= 'f') d = static_cast<uint8_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = static_cast<uint8_t>(c - 'A' + 10);
        else return false;
        v[i / 2] = static_cast<uint8_t>((v[i / 2] << 4) | d);
    }
    r = v[0];
    g = v[1];
    b = v[2];
    return true;
}

// "#RRGGBB", upper case like to_hex (parse_hex_color accepts either case)
inline std::string to_hex_color(uint8_t r, uint8_t g, uint8_t b) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
    return buf;
}

// --------------------------------------------------------------------------------------
// Time and Timezone String Helpers
// --------------------------------------------------------------------------------------

// accepts GMT, GMT0, UTC, UTC0, GMT±H, GMT±HH, GMT±H:MM, GMT±HH:MM, GMT±HMM and GMT±HHMM
// (any case); nothing may follow the offset
inline bool parse_gmt_offset(std::string_view s, std::string& normalized_gmt) {
    std::string tz = upper(std::string(s));
    if (tz == "GMT" || tz == "GMT0" || tz == "UTC" || tz == "UTC0") {
        normalized_gmt = "GMT+00:00";
        return true;
    }

    if (tz.find("GMT") != 0 || tz.length() < 5) return false;

    char sign = tz[3];
    if (sign != '+' && sign != '-') return false;

    const std::string_view num(tz.c_str() + 4, tz.length() - 4);
    const auto all_digits = [](std::string_view d) {
        if (d.empty()) return false;
        for (char c : d) if (c < '0' || c > '9') return false;
        return true;
    };
    const auto to_int = [](std::string_view d) {
        int v = 0;
        for (char c : d) v = v * 10 + (c - '0');
        return v;
    };

    int          h     = 0, m = 0;
    const size_t colon = num.find(':');
    if (colon != std::string_view::npos) {
        const std::string_view hh = num.substr(0, colon);
        const std::string_view mm = num.substr(colon + 1);
        if (hh.size() > 2 || mm.size() != 2 || !all_digits(hh) || !all_digits(mm)) return false;
        h = to_int(hh);
        m = to_int(mm);
    } else {
        if (num.size() > 4 || !all_digits(num)) return false;
        const int val = to_int(num);
        if (num.size() <= 2) {
            h = val;
        } else {
            h = val / 100;
            m = val % 100;
        }
    }

    if (h > 14 || m >= 60) return false;
    if (h == 14 && m > 0) return false;

    char buf[16];
    snprintf(buf, sizeof(buf), "GMT%c%02d:%02d", sign, h, m);
    normalized_gmt = buf;
    return true;
}

inline bool parse_day(std::string_view day_str, uint8_t& day_num) {
    std::string d = upper(std::string(day_str));
    if (d == "MO") {
        day_num = 0;
        return true;
    }
    if (d == "TU") {
        day_num = 1;
        return true;
    }
    if (d == "WE") {
        day_num = 2;
        return true;
    }
    if (d == "TH") {
        day_num = 3;
        return true;
    }
    if (d == "FR") {
        day_num = 4;
        return true;
    }
    if (d == "SA") {
        day_num = 5;
        return true;
    }
    if (d == "SU") {
        day_num = 6;
        return true;
    }
    return false;
}

inline bool parse_time(std::string_view time_str, uint16_t& minutes) {
    int         h = 0, m = 0;
    std::string t(time_str);
    if (sscanf(t.c_str(), "%d:%d", &h, &m) == 2) {
        if (h >= 0 && h <= 23 && m >= 0 && m <= 59) {
            minutes = static_cast<uint16_t>(h * 60 + m);
            return true;
        } else if (h == 24 && m == 0) {
            minutes = static_cast<uint16_t>(h * 60 + m);
            return true;
        }
    }
    return false;
}

// --------------------------------------------------------------------------------------
// Scheduler & JSON Parsing
// --------------------------------------------------------------------------------------

inline std::vector<std::string> extract_commands(std::string_view blob) {
    std::vector<std::string> cmds;
    bool                     in_quotes = false, escaped = false;
    std::string              current_cmd;

    for (char c : blob) {
        if (escaped) {
            current_cmd += c;
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            if (in_quotes) {
                cmds.push_back(current_cmd);
                current_cmd.clear();
                in_quotes = false;
            } else {
                in_quotes = true;
            }
        } else if (in_quotes) {
            current_cmd += c;
        }
    }

    if (cmds.size() == 1) {
        int q_count = 0;
        for (size_t i = 0; i < cmds[0].length(); ++i) {
            if (cmds[0][i] == '"' && (i == 0 || cmds[0][i - 1] != '\\')) q_count++;
        }
        if (q_count >= 2) return extract_commands(cmds[0]);
    }
    return cmds;
}

inline std::string escape_json(std::string_view s) {
    std::string res;
    res.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '"') res += "\\\"";
        else if (c == '\\') res += "\\\\";
        else res += c;
    }
    return res;
}

// --------------------------------------------------------------------------------------
// Small, header-only string utilities intended for embedded targets.
// --------------------------------------------------------------------------------------

inline constexpr char kCRLF[] = "\r\n";

inline std::string    repeat(char ch, size_t count) {
    return std::string(count, ch);
}

inline void trim(std::string& s) {
    const char* ws = " \t\r\n";
    s.erase(0, s.find_first_not_of(ws));
    s.erase(s.find_last_not_of(ws) + 1);
}

inline void rtrim_cr(std::string& s) {
    if (!s.empty() && s.back() == '\r') s.pop_back();
}

inline std::string to_lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

inline std::vector<std::string_view> split_lines_sv(std::string_view text, char delim = '\n') {
    std::vector<std::string_view> out;
    size_t                        start = 0;
    while (start <= text.size()) {
        size_t pos = text.find(delim, start);
        if (pos == std::string_view::npos) {
            out.emplace_back(text.substr(start));
            break;
        } else {
            out.emplace_back(text.substr(start, pos - start));
            start = pos + 1;
        }
    }
    return out;
}

// an empty separator splits nothing: the result is the whole input as one element
inline std::vector<std::string> split_by_token(std::string_view s, std::string_view token) {
    std::vector<std::string> out;
    if (token.empty()) {
        out.emplace_back(s);
        return out;
    }
    size_t                   start = 0;
    while (start <= s.size()) {
        size_t pos = s.find(token, start);
        if (pos == std::string_view::npos) {
            out.emplace_back(s.substr(start));
            break;
        } else {
            out.emplace_back(s.substr(start, pos - start));
            start = pos + token.size();
        }
    }
    return out;
}

inline std::vector<std::string> wrap_fixed(std::string_view s, size_t width) {
    std::vector<std::string> out;
    if (width == 0) {
        out.emplace_back(s);
        return out;
    }
    for (size_t i = 0; i < s.size(); i += width) {
        out.emplace_back(s.substr(i, std::min(width, s.size() - i)));
    }
    return out;
}

inline std::vector<std::string> wrap_words(std::string_view s, size_t width) {
    std::vector<std::string> out;
    if (width == 0) {
        out.emplace_back(s);
        return out;
    }
    auto        is_space = [](unsigned char c) { return std::isspace(c) != 0; };

    std::string line;
    line.reserve(width);

    size_t i = 0, n = s.size();
    while (i < n) {
        while (i < n && is_space(static_cast<unsigned char>(s[i]))) ++i;
        if (i >= n) break;

        size_t j = i;
        while (j < n && !is_space(static_cast<unsigned char>(s[j]))) ++j;
        std::string_view word = s.substr(i, j - i);
        i                     = j;

        if (line.empty()) {
            if (word.size() <= width) {
                line.assign(word);
            } else {
                size_t k = 0;
                while (k < word.size()) {
                    size_t take = std::min(width, word.size() - k);
                    out.emplace_back(word.substr(k, take));
                    k += take;
                }
            }
        } else {
            if (line.size() + 1 + word.size() <= width) {
                line.push_back(' ');
                line.append(word);
            } else {
                out.emplace_back(line);
                line.clear();
                if (word.size() <= width) {
                    line.assign(word);
                } else {
                    size_t k = 0;
                    while (k < word.size()) {
                        size_t take = std::min(width, word.size() - k);
                        if (line.empty()) {
                            line.assign(word.substr(k, take));
                        } else {
                            out.emplace_back(line);
                            line.assign(word.substr(k, take));
                        }
                        k += take;
                    }
                }
            }
        }
    }
    if (!line.empty()) out.emplace_back(std::move(line));
    if (out.empty()) out.emplace_back(std::string{});
    return out;
}

inline std::string align_into(std::string_view s, size_t width, char align) {
    if (width == 0 || s.size() >= width) return std::string(s);
    size_t pad = width - s.size();
    switch (align) {
        case 'r': return repeat(' ', pad) + std::string(s);
        case 'c': {
            size_t left  = pad / 2;
            size_t right = pad - left;
            return repeat(' ', left) + std::string(s) + repeat(' ', right);
        }
        default: // 'l'
            return std::string(s) + repeat(' ', pad);
    }
}

inline std::string repeat_pattern(std::string_view pat, size_t count) {
    if (count == 0) return {};
    if (pat.empty()) return std::string(count, ' ');
    std::string out;
    out.resize(count);
    size_t p = 0;
    for (size_t i = 0; i < count; ++i) {
        out[i] = pat[p];
        p      = (p + 1) % pat.size();
    }
    return out;
}

inline std::string make_spacer_line(uint16_t total_width, std::string_view edge = "|") {
    if (total_width == 0) return {};
    if (edge.empty()) return std::string(total_width, ' ');
    const size_t e = edge.size();
    if (total_width <= e) return std::string(edge.substr(0, total_width));
    if (total_width <= 2 * e) return std::string(edge.substr(0, total_width));
    const uint16_t inner = static_cast<uint16_t>(total_width - 2 * e);
    std::string    out;
    out.reserve(total_width);
    out.append(edge);
    out.append(inner, ' ');
    out.append(edge);
    return out;
}

inline std::string make_rule_line(uint16_t total_width, std::string_view fill = "-", std::string_view edge = "+") {
    if (total_width == 0) return {};
    if (edge.empty()) return repeat_pattern(fill, total_width);
    const size_t e = edge.size();
    if (total_width <= e) return std::string(edge.substr(0, total_width));
    if (total_width <= 2 * e) return std::string(edge.substr(0, total_width));
    const uint16_t inner = static_cast<uint16_t>(total_width - 2 * e);
    std::string    out;
    out.reserve(total_width);
    out.append(edge);
    out += repeat_pattern(fill, inner);
    out.append(edge);
    return out;
}

inline std::string compose_box_line(std::string_view content,
    std::string_view                                 edge,
    size_t                                           message_width,
    size_t                                           margin_l,
    size_t                                           margin_r,
    char                                             align) {
    const size_t edge_len = edge.size();
    const size_t field    = (message_width == 0) ? content.size() : message_width;

    std::string  line;
    line.reserve(edge_len * 2 + margin_l + field + margin_r);

    if (edge_len) line.append(edge);

    if (message_width == 0) {
        line += repeat(' ', margin_l);
        line.append(content);
        line += repeat(' ', margin_r);
    } else {
        std::string payload = align_into(content, message_width, align);
        line += repeat(' ', margin_l);
        line += payload;
        line += repeat(' ', margin_r);
    }

    if (edge_len) line.append(edge);
    return line;
}

inline std::string vformat(const char* fmt, va_list ap) {
    if (!fmt) return {};
#if defined(__GNUC__)
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int needed = vsnprintf(nullptr, 0, fmt, ap_copy);
    va_end(ap_copy);
    if (needed <= 0) return {};
    std::string out;
    out.resize(static_cast<size_t>(needed));
    vsnprintf(out.data(), out.size() + 1, fmt, ap);
    return out;
#else
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    return std::string(buf);
#endif
}

// printf-style formatting into a std::string. Use this instead of Serial.printf,
// which the AVR/SAMD/STM32 Print class does not provide.
inline std::string format(const char* fmt, ...) {
    va_list     ap;
    va_start(ap, fmt);
    std::string out = vformat(fmt, ap);
    va_end(ap);
    return out;
}

template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
inline bool parse_int(std::string_view s, T& out) {
    size_t start = 0, end = s.size();
    while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    if (start >= end) return false;

    std::string tmp(s.substr(start, end - start));
    char*       pEnd = nullptr;

    errno            = 0;

    if constexpr (std::is_signed<T>::value) {
        long long v = strtoll(tmp.c_str(), &pEnd, 10);
        if (pEnd == tmp.c_str() || *pEnd != '\0') return false;
        if (errno == ERANGE) return false;  // beyond long long: strtoll saturates
        if (v < static_cast<long long>(std::numeric_limits<T>::min()) ||
            v > static_cast<long long>(std::numeric_limits<T>::max())) return false;
        out = static_cast<T>(v);
        return true;
    } else {
        if (tmp[0] == '-') return false;    // strtoull would negate "-1" to the maximum
        unsigned long long v = strtoull(tmp.c_str(), &pEnd, 10);
        if (pEnd == tmp.c_str() || *pEnd != '\0') return false;
        if (errno == ERANGE) return false;
        if (v > static_cast<unsigned long long>(std::numeric_limits<T>::max())) return false;
        out = static_cast<T>(v);
        return true;
    }
}

// Same contract as parse_int, for floating-point types: trims, requires the whole
// token to be numeric, and reports failure instead of throwing.
template <typename T, typename = std::enable_if_t<std::is_floating_point<T>::value>>
inline bool parse_float(std::string_view s, T& out) {
    size_t start = 0, end = s.size();
    while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    if (start >= end) return false;

    std::string tmp(s.substr(start, end - start));
    char*       pEnd = nullptr;
    errno            = 0;

    const double v = strtod(tmp.c_str(), &pEnd);
    if (pEnd == tmp.c_str() || *pEnd != '\0') return false;
    if (errno == ERANGE) return false;

    out = static_cast<T>(v);
    return true;
}

} // namespace xewe::str
