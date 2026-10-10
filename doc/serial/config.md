# SerialPortConfig and begin

`src/XeWeCore/Serial.h` — how the port is opened.

## SerialPortConfig

```cpp
struct SerialPortConfig {
    unsigned long baud_rate        = 115200;
    std::size_t   tx_buffer_size   = 2048;
    std::size_t   rx_buffer_size   = 1024;
    uint32_t      startup_delay_ms = 1000;  // give USB CDC time to enumerate
    bool          echo             = true;  // echo typed characters back
};
```

| Field | Default | |
|---|---|---|
| `baud_rate` | `115200` | passed to `Serial.begin` |
| `tx_buffer_size` | `2048` | set **before** `Serial.begin`; a table or header can be several hundred bytes in one burst |
| `rx_buffer_size` | `1024` | the hardware buffer, separate from the 255-byte line buffer |
| `startup_delay_ms` | `1000` | a plain `delay()` after opening the port |
| `echo` | `true` | echo each received character back, so a terminal shows what is typed |

## begin

```cpp
void begin(const SerialPortConfig& cfg = {});
```

Stores `echo`, sets the TX and RX buffer sizes, calls `Serial.begin(baud_rate)`, then waits
`startup_delay_ms`.

```cpp
xewe::SerialPort serial;

void setup() {
    serial.begin();                                  // defaults
    serial.begin({.baud_rate = 921600, .echo = false});
}
```

**`begin()` blocks for a second by default.** A USB CDC port (ESP32-C3, C6 and S3 with
`CDCOnBoot`) needs time to enumerate on the host. Without the delay the first lines go to a port
nobody listens to yet. On a real UART, set `startup_delay_ms = 0` to boot faster.

**The buffer sizes apply on ESP32 only** (the calls sit behind `ARDUINO_ARCH_ESP32`). They must be
set before `Serial.begin`, so they cannot change after `begin()`.

## Notes

* `SerialPort` uses no heap for input. Its line buffer and four-line queue take about 1.3 KB of
  static RAM. Use one instance per physical port, usually a global.
* Calling `begin()` twice re-opens the port and waits again.
* `SerialPort` has no `DEBUG_` flag (see [debug](../utils/debug.md)).
