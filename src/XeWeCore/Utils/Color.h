// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Utils/Color.h
#pragma once

#include <array>
#include <algorithm> // for std::max, std::min
#include <cstdint>


namespace xewe::color {

inline std::array<uint8_t, 3> hsv_to_rgb(const std::array<uint8_t, 3>& hsv) {
    // Convert uint8_t [0-255] inputs to float [0.0-1.0]
    const float h_f = hsv[0] / 255.0f;
    const float s_f = hsv[1] / 255.0f;
    const float v_f = hsv[2] / 255.0f;

    float       r_f = 0.0f, g_f = 0.0f, b_f = 0.0f;

    // Apply the provided library's logic
    const int   i = static_cast<int>(h_f * 6.0f);
    const float f = h_f * 6.0f - i;
    const float p = v_f * (1.0f - s_f);
    const float q = v_f * (1.0f - f * s_f);
    const float t = v_f * (1.0f - (1.0f - f) * s_f);

    switch (i % 6) {
        case 0:
            r_f = v_f;
            g_f = t;
            b_f = p;
            break;
        case 1:
            r_f = q;
            g_f = v_f;
            b_f = p;
            break;
        case 2:
            r_f = p;
            g_f = v_f;
            b_f = t;
            break;
        case 3:
            r_f = p;
            g_f = q;
            b_f = v_f;
            break;
        case 4:
            r_f = t;
            g_f = p;
            b_f = v_f;
            break;
        case 5:
            r_f = v_f;
            g_f = p;
            b_f = q;
            break;
    }

    // Convert back to uint8_t [0-255]
    uint8_t r = static_cast<uint8_t>(r_f * 255.0f);
    uint8_t g = static_cast<uint8_t>(g_f * 255.0f);
    uint8_t b = static_cast<uint8_t>(b_f * 255.0f);

    return {r, g, b};
}

// Converts an array of RGB [0-255] to an array of HSV [0-255]
inline std::array<uint8_t, 3> rgb_to_hsv(const std::array<uint8_t, 3>& rgb) {
    // Convert uint8_t [0-255] inputs to float [0.0-1.0]
    const float r_f = rgb[0] / 255.0f;
    const float g_f = rgb[1] / 255.0f;
    const float b_f = rgb[2] / 255.0f;

    float       h_f = 0.0f, s_f = 0.0f, v_f = 0.0f;

    // Apply the provided library's logic
    const float max_val = std::max({r_f, g_f, b_f});
    const float min_val = std::min({r_f, g_f, b_f});
    v_f                 = max_val;

    const float d       = max_val - min_val;
    s_f                 = (max_val == 0.0f) ? 0.0f : (d / max_val);

    if (max_val == min_val) {
        h_f = 0.0f;
    } else {
        if (max_val == r_f) {
            h_f = (g_f - b_f) / d + (g_f < b_f ? 6.0f : 0.0f);
        } else if (max_val == g_f) {
            h_f = (b_f - r_f) / d + 2.0f;
        } else if (max_val == b_f) {
            h_f = (r_f - g_f) / d + 4.0f;
        }
        h_f /= 6.0f;
    }

    // Convert back to uint8_t [0-255]
    uint8_t h = static_cast<uint8_t>(h_f * 255.0f);
    uint8_t s = static_cast<uint8_t>(s_f * 255.0f);
    uint8_t v = static_cast<uint8_t>(v_f * 255.0f);

    return {h, s, v};
}
} // namespace xewe::color
