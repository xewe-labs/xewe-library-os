# Prompts

`src/XeWeCore/Serial.h` — typed questions asked over the serial console.

**Every `get_*` function blocks** until it gets a valid answer, times out, or runs out of
attempts. Internally they busy-wait, calling `loop()` and `yield()`, so co-operative tasks keep
running — but the sketch's own `loop()` does not. Prompts belong in `setup()` and in module setup
routines, never in a running `loop()`.

**A prompt reachable from a running device must be bounded.** A command can arrive from the
scheduler, a button or the web UI with nobody at the console, and an unbounded prompt
(`retry_count = 0` or `timeout_ms = 0`) then stops `Os::loop` — Wi-Fi, web server, scheduler,
buttons — until someone answers. The core's own confirmations follow this rule:

| Prompt | Call | On timeout or invalid answer |
|---|---|---|
| `$<module> disable` → `OK?` (`Module::disable`) | `get_yn("OK?", 2, 15000, false, answered)` | after the 2nd failed attempt: `! No answer: disable cancelled`, `Aborted` |
| `$system reset` → `OK?` (`System::reset`) | `get_yn("OK?", 2, 15000, false, answered)` | after the 2nd failed attempt: `! No answer: reset cancelled`, `Aborted` |

Two attempts of 15 s each: a timeout (`! Timeout.`) or an invalid answer (`! Please answer 'y' or
'n'.`) re-prompts once, a second one cancels. Worst-case stall of `Os::loop`: 30 s. An answer typed
after the second timeout is an ordinary (rejected) command line. Only the first-boot provisioning prompts (enable
question, device name) stay unbounded — they run in `setup()` and must be answered.

```cpp
bool     ok;
uint32_t period = serial.get_uint32("Blink period in ms?", 50, 10000,
                                    3,      // 3 attempts, then give up
                                    30000,  // 30 s per attempt
                                    500,    // returned if it gives up
                                    ok);

if (!ok) serial.print("No answer; using 500 ms.");
```

## The shared parameters

Every prompt ends with the same four parameters, and they all behave the same way.

| Parameter | |
|---|---|
| `retry_count` | **`0` means infinite** — the call only returns on valid input. `N ≥ 1` means **N total attempts**, after which the default is returned |
| `timeout_ms` | per attempt. **`0` means wait forever.** On expiry it prints `! Timeout.` and, if attempts remain, re-prompts |
| `default_value` | returned when the prompt gives up. Never returned while `retry_count == 0` |
| `success_sink` | `std::optional<std::reference_wrapper<bool>>`; set to `true` on a real answer and `false` when the default was returned |

Pass a `bool` by name for `success_sink` — the implicit conversion to
`std::reference_wrapper<bool>` does the rest.

**`success_sink` is the only way to distinguish a genuine answer from a fallback.** If the user
types `500` and the default is also `500`, the return value alone cannot tell you which happened.
Anything that persists the result should check it.

Each prompt first calls [`clear_input()`](input.md#clear_input), then prints `prompt` on its own
line (when non-empty), then prints a short iteration prompt before each attempt: `> ` for most,
`(y/n) > ` for `get_yn`. The marker is printed on its own line, so typed input echoes on
the line below it.

## get_string

```cpp
std::string get_string(std::string_view prompt        = {},
                       const uint16_t   min_length    = 0,
                       const uint16_t   max_length    = 0,
                       const uint16_t   retry_count   = 0,
                       const uint32_t   timeout_ms    = 0,
                       std::string_view default_value = {},
                       std::optional<std::reference_wrapper<bool>> success_sink = std::nullopt);
```

`min_length` and `max_length` are **character counts**, inclusive. `max_length == 0` means "no
explicit maximum" and resolves to 254, the usable line-buffer size.

The answer is not trimmed, and an empty line is a valid answer when `min_length == 0`. Out of
bounds prints `! Length must be in [min..max] chars.`

## get_int, get_uint8, get_uint16, get_uint32

```cpp
int      get_int   (std::string_view prompt = {},
                    const int      min_value = std::numeric_limits<int>::min(),
                    const int      max_value = std::numeric_limits<int>::max(),
                    const uint16_t retry_count = 0, const uint32_t timeout_ms = 0,
                    const int      default_value = 0,
                    std::optional<std::reference_wrapper<bool>> success_sink = std::nullopt);

uint8_t  get_uint8 (/* same shape, uint8_t  bounds */);
uint16_t get_uint16(/* same shape, uint16_t bounds */);
uint32_t get_uint32(/* same shape, uint32_t bounds */);
```

Parsed with [`xewe::str::parse_int`](../utils/string.md#number-parsing):
base 10, surrounding whitespace trimmed, **trailing characters rejected**.

Messages: `! Invalid number. Please enter a base-10 integer.` and `! Out of range [min..max].`

If `min_value > max_value` they are **silently swapped** rather than rejected.

## get_float

```cpp
float get_float(std::string_view prompt        = {},
                const float      min_value     = -std::numeric_limits<float>::infinity(),
                const float      max_value     =  std::numeric_limits<float>::infinity(),
                const uint16_t   retry_count   = 0,
                const uint32_t   timeout_ms    = 0,
                const float      default_value = 0.0f,
                std::optional<std::reference_wrapper<bool>> success_sink = std::nullopt);
```

`strtod`, trailing spaces allowed but no other trailing characters, and NaN is rejected. Inverted
bounds are swapped as above. Messages: `! Invalid number. Please enter a decimal value.`,
`! Invalid number.`, `! Out of range [min..max].`

## get_yn

```cpp
bool get_yn(std::string_view prompt        = {},
            const uint16_t   retry_count   = 0,
            const uint32_t   timeout_ms    = 0,
            const bool       default_value = false,
            std::optional<std::reference_wrapper<bool>> success_sink = std::nullopt);
```

Accepted, case-insensitively:

| true | false |
|---|---|
| `y`, `yes`, `1`, `true` | `n`, `no`, `0`, `false` |

Anything else prints `! Please answer 'y' or 'n'.` The iteration prompt is `(y/n) > `.

Note there is no `min`/`max` pair here, so the shared parameters start one position earlier than
in the numeric prompts.

## get_menu_choice

```cpp
uint8_t get_menu_choice(std::string_view               prompt        = {},
                        const std::vector<std::string> options       = {},
                        const uint8_t                  min_value     = std::numeric_limits<uint8_t>::min(),
                        const uint8_t                  max_value     = std::numeric_limits<uint8_t>::max(),
                        const uint16_t                 retry_count   = 0,
                        const uint32_t                 timeout_ms    = 0,
                        const uint8_t                  default_value = 0,
                        std::optional<std::reference_wrapper<bool>> success_sink = std::nullopt);
```

Prints the prompt, then the options, then delegates to `get_uint8`.

```cpp
uint8_t mode = serial.get_menu_choice("Pick a mode:", {"Solid", "Fade", "Rainbow"});
```

```
Pick a mode:
  1) Solid
  2) Fade
  3) Rainbow
Choice
>
```

**Bounds are derived when they are left at their defaults and `options` is non-empty:**
`min_value` of `0` becomes `1` (menus are 1-based), and `max_value` of `255` becomes
`min + options.size() - 1`, clamped to 255. Pass explicit bounds to override either.

The returned value is the printed number, so subtract `min_value` for a zero-based index.

Because it delegates, the retry, timeout, default and `success_sink` semantics are `get_uint8`'s,
the sub-prompt is `Choice`, and **the option list is not reprinted on a retry**. When `options` is
empty and a `prompt` was given, the `Choice` sub-prompt is suppressed to avoid a double prompt.
