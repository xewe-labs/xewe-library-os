# xewe::pins

`src/XeWeCore/Utils/Pins.h` — which module owns which GPIO, and which GPIOs are strapping pins on
the chip being built for. Header-only, host-includable, no heap.

```cpp
if (!xewe::pins::claim(cfg.pin, id.c_str())) return;   // in a module's begin, before pinMode
...
xewe::pins::release(cfg.pin, id.c_str());              // when the module lets the pin go
```

The core's own `System` module claims no pins. A pin that nobody claims is not checked: two
modules that both claim their pins are protected from each other, and a module that does not
claim is invisible to the registry.

## claim, release, owner_of

```cpp
inline bool        claim   (int gpio, const char* owner);
inline bool        release (int gpio, const char* owner);
inline const char* owner_of(int gpio);
```

| | |
|---|---|
| `claim` | `true` when the pin was free (now `owner`'s) or already `owner`'s (names compared with `strcmp`). `false` when another owner holds it, and for a GPIO outside `0`–`48`. A null `owner` is stored as `"?"` |
| `release` | frees the pin if `owner` holds it (names compared with `strcmp`); `false` and no change otherwise |
| `owner_of` | the owner's name, or `nullptr` when free or out of range |

**The owner pointer is stored, not copied:** pass a string literal or a module's `id.c_str()`
(stable for the module's lifetime), never a temporary.

**A strapping pin is claimed with a warning, never refused.** Its level at reset selects the boot
mode, so a pull-up, pull-down or load on it can stop the chip from booting normally; the module
author decides whether that matters for the hardware.

## Messages

Reported through `xewe::pins::error_handler`, called as `error_handler(error_context, message)`:

```cpp
inline void (*error_handler)(void* context, const char* message) = nullptr;
inline void*  error_context = nullptr;
```

A plain function pointer rather than a `std::function` (as `flex_error_handler` is) because a
`std::function` global costs flash and RAM in every firmware that includes the header, claim or
not. The `Os` constructor points it at [`Os::report_error`](../os/os.md#report_error), so a claim
from a module constructor is queued and printed after the banner. Unset (no `Os`), the registry is
silent.

| Case | Message |
|---|---|
| held by someone else | `! GPIO 8 already claimed by led, refused for fan` |
| out of range | `! GPIO 60 out of range (0-48): not claimed by fan` |
| strapping pin (claimed) | `! GPIO 8 is a strapping pin (led): its level at reset selects the boot mode` |

The strapping warning is printed once, when the pin goes from free to claimed.

## Strapping pins

```cpp
inline constexpr uint64_t kStrappingEsp32, kStrappingEsp32C3, kStrappingEsp32C6, kStrappingEsp32S3;
inline constexpr uint64_t kStrapping;          // the one for CONFIG_IDF_TARGET_*, 0 on the host
constexpr bool            is_strapping(int gpio);
```

| Chip | Strapping GPIOs | Source |
|---|---|---|
| ESP32-C3 | 2, 8, 9 | ESP32-C3 datasheet, "Strapping Pins" |
| ESP32-C6 | 4 (MTMS), 5 (MTDI), 8, 9, 15 | ESP32-C6 datasheet, "Boot Configurations"; core 3.3.12 `soc/esp32c6/register/soc/io_mux_reg.h` (GPIO4 = MTMS, GPIO5 = MTDI) and `efuse_reg.h` (JTAG select strap on GPIO15) |
| ESP32-S3 | 0, 3, 45, 46 | ESP32-S3 datasheet, "Boot Configurations"; core 3.3.12 `variants/waveshare_esp32_s3_rgb_matrix/pins_arduino.h` ("GPIO45 and GPIO46 are strapping pins") |
| ESP32 | 0, 2, 5, 12 (MTDI), 15 (MTDO) | ESP32 datasheet, "Strapping Pins" |

Any other target gets `kStrapping == 0`. The target comes from `<sdkconfig.h>`, included when the
ESP32 core provides it.

## Cost

Nothing unless a module calls `claim`, `release` or `owner_of`: then a 49-entry owner table
(196 bytes on the ESP32), the strapping mask and the functions are linked. The `Os` constructor
always sets `error_handler` and `error_context` (two pointers and a one-line function).
