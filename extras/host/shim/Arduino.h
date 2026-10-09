// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/extras/host/shim/Arduino.h
#pragma once

// Clean-room <Arduino.h> for host builds of the hardware-free parts of XeWeCore
// (Utils, Serial formatting, Cli). It provides only the core API those parts use,
// plus test hooks: a settable clock, a capture buffer behind Serial.write() and an
// injectable RX queue behind Serial.available()/read().
//
// Deliberately absent, so the ESP32-only paths stay behind their guards:
//   Serial.printf()            - SerialPort formats itself
//   Serial.setTxBufferSize()   - guarded by ARDUINO_ARCH_ESP32 in Serial.cpp
//   Serial.setRxBufferSize()   - guarded by ARDUINO_ARCH_ESP32 in Serial.cpp

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <string>
#include <string_view>


namespace host {
inline unsigned long now_ms = 0;                        // settable fake clock
inline void          set_millis (unsigned long ms) { now_ms = ms; }
inline void          advance    (unsigned long ms) { now_ms += ms; }
inline void        (*on_yield)  ()               = nullptr; // test hook: lets busy-wait loops see time pass
} // namespace host

inline unsigned long millis            () { return host::now_ms; }
inline unsigned long micros            () { return host::now_ms * 1000UL; }
inline void          delay             (unsigned long ms) { host::now_ms += ms; }
inline void          delayMicroseconds (unsigned int) {}
inline void          yield             () { if (host::on_yield) host::on_yield(); }
inline void          analogWrite       (int, int) {}
inline void          digitalWrite      (int, int) {}
inline void          pinMode           (int, int) {}
inline int           analogRead        (int) { return 0; }
inline int           digitalRead       (int) { return 0; }

constexpr int        HIGH   = 1;
constexpr int        LOW    = 0;
constexpr int        INPUT  = 0;
constexpr int        OUTPUT = 1;

class HostSerial {
public:
    // test hooks
    std::string      tx;                                // everything written
    std::deque<char> rx;                                // bytes waiting to be read
    bool             echo_to_stdout = false;

    void   inject    (std::string_view s) { rx.insert(rx.end(), s.begin(), s.end()); }
    std::string take () { std::string out; out.swap(tx); return out; }

    // Arduino API subset
    void   begin     (unsigned long = 115200) {}
    void   end       () {}
    int    available () { return static_cast<int>(rx.size()); }
    int    read      () { if (rx.empty()) return -1; char c = rx.front(); rx.pop_front(); return static_cast<unsigned char>(c); }
    int    peek      () { return rx.empty() ? -1 : static_cast<unsigned char>(rx.front()); }
    void   flush     () {}

    size_t write     (uint8_t c)                    { put(std::string_view(reinterpret_cast<const char*>(&c), 1)); return 1; }
    size_t write     (const uint8_t* buf, size_t n) { put(std::string_view(reinterpret_cast<const char*>(buf), n)); return n; }

    void   print     (const char* s)        { put(s); }
    void   print     (const std::string& s) { put(s); }
    void   print     (char c)               { put(std::string_view(&c, 1)); }
    void   print     (int v)                { put(std::to_string(v)); }
    void   print     (unsigned v)           { put(std::to_string(v)); }
    void   print     (long v)               { put(std::to_string(v)); }
    void   print     (unsigned long v)      { put(std::to_string(v)); }
    void   print     (double v)             { put(std::to_string(v)); }

    template <typename T>
    void   println   (const T& v)           { print(v); put("\r\n"); }
    void   println   ()                     { put("\r\n"); }

    explicit operator bool() const { return true; }

private:
    void   put       (std::string_view s) {
        tx.append(s.data(), s.size());
        if (echo_to_stdout) std::fwrite(s.data(), 1, s.size(), stdout);
    }
};

inline HostSerial Serial;
