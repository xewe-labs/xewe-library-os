# xewe::str

`src/XeWeCore/Utils/String.h` — string helpers shared across the XeWe libraries: case, trimming,
splitting, wrapping, box drawing, and parsers for numbers, times and days.

Everything here is `inline` and header-only. Arguments are taken as `std::string_view` where the
function only reads, and by value as `std::string` where it returns a modified copy.

## Macros

```cpp
#define STRINGIFY_XEWE(x) #x
#define TO_STRING(x)      STRINGIFY_XEWE(x)
```

Global, not namespaced. `TO_STRING` expands its argument first, so it turns a macro into its
value: `TO_STRING(BUILD_VERSION)`.

## Constants

```cpp
inline constexpr char kCRLF[] = "\r\n";
```

The line ending used across XeWe serial output.

## Case and hex

```cpp
inline std::string lower     (std::string s);
inline std::string to_lower  (std::string s);
inline std::string upper     (std::string s);
inline std::string capitalize(std::string s);
inline std::string to_hex    (const uint8_t* b, size_t n);
```

| | |
|---|---|
| `lower` / `to_lower` | two separate implementations of the same thing; prefer `lower` |
| `upper` | uppercases every character |
| `capitalize` | title-cases each word: uppercase first alphanumeric of a run, lowercase the rest. Any non-alphanumeric character starts a new word |
| `to_hex` | uppercase hex of `n` bytes, no separators and no `0x` prefix |

## Hex colours

```cpp
inline bool        parse_hex_color(const std::string& s, uint8_t& r, uint8_t& g, uint8_t& b);
inline std::string to_hex_color   (uint8_t r, uint8_t g, uint8_t b);
```

| | |
|---|---|
| `parse_hex_color` | accepts `rrggbb` or `#rrggbb`: exactly six hex digits, either case, at most one leading `#`. Returns `false` and leaves `r`/`g`/`b` untouched for anything else: `""`, `#`, `#ff`, `fff`, `ff00zz`, seven digits, `##00ffff`, `0xff00`, a sign, spaces |
| `to_hex_color` | `#RRGGBB`, uppercase like `to_hex`; `parse_hex_color` reads it back |

```cpp
uint8_t r, g, b;
if (!xewe::str::parse_hex_color(args[0], r, g, b)) { os.serial.print("! Expected RRGGBB"); return; }
```

## Trimming and splitting

```cpp
inline void                          trim          (std::string& s);
inline void                          rtrim_cr      (std::string& s);
inline std::vector<std::string_view> split_lines_sv(std::string_view text, char delim = '\n');
inline std::vector<std::string>      split_by_token(std::string_view s, std::string_view token);
```

| | |
|---|---|
| `trim` | in place; strips leading and trailing ` `, `\t`, `\r`, `\n` |
| `rtrim_cr` | in place; removes **one** trailing `\r`, for CRLF input |
| `split_lines_sv` | splits on `delim`; always emits a final element, which is empty when the text ends with the delimiter |
| `split_by_token` | splits on a multi-character separator |

**`split_lines_sv` returns views into its argument.** The input must outlive the result; splitting
a temporary is a dangling read.

## Wrapping and alignment

```cpp
inline std::vector<std::string> wrap_fixed    (std::string_view s, size_t width);
inline std::vector<std::string> wrap_words    (std::string_view s, size_t width);
inline std::string              align_into    (std::string_view s, size_t width, char align);
inline std::string              repeat        (char ch, size_t count);
inline std::string              repeat_pattern(std::string_view pat, size_t count);
```

| | |
|---|---|
| `wrap_fixed` | hard character chunks of exactly `width`, ignoring word boundaries |
| `wrap_words` | greedy word wrap; words longer than `width` are hard-split; runs of whitespace collapse to one space |
| `align_into` | pads `s` to `width`; `'r'` right, `'c'` centre (extra pad goes right), anything else left |
| `repeat` | `count` copies of one character |
| `repeat_pattern` | cycles `pat` until `count` characters are produced; an empty pattern yields spaces |

Edge cases: both wrappers return the input unchanged as a single element when `width == 0`;
`wrap_words` on empty input yields one empty string. `align_into` returns `s` unchanged when it is
already at least `width` long, or when `width == 0`.

## Box drawing

```cpp
inline std::string make_spacer_line(uint16_t total_width, std::string_view edge = "|");
inline std::string make_rule_line  (uint16_t total_width, std::string_view fill = "-",
                                    std::string_view edge = "+");
inline std::string compose_box_line(std::string_view content,
                                    std::string_view edge,
                                    size_t           message_width,
                                    size_t           margin_l,
                                    size_t           margin_r,
                                    char             align);
```

`make_spacer_line` draws `|` + spaces + `|`; `make_rule_line` draws `+` + `---` + `+`, with `fill`
cycled by `repeat_pattern` so multi-character fills such as `"-="` work. Both return `{}` when
`total_width` is `0`, and degrade to a truncated edge string when the width is no larger than the
edges.

`compose_box_line` builds one content row: `edge`, `margin_l` spaces, the payload, `margin_r`
spaces, `edge`. When `message_width` is `0` the content is emitted as-is with no field width and
no alignment; otherwise it is padded to `message_width` with `align_into`. These three are what
[`xewe::SerialPort`](../serial/output.md)
renders its headers, separators and tables with.

## Formatting

```cpp
inline std::string vformat(const char* fmt, va_list ap);
```

`vsnprintf` into a `std::string`. Returns `{}` for a null `fmt` or a non-positive formatted length.

**On GCC it is exact** (a two-pass `vsnprintf` sizes the result first). On any other compiler it
falls back to a **256-byte stack buffer and truncates** silently. Every supported core's toolchain
is GCC or Clang, so the fast path is what you get on device.

```cpp
inline std::string format(const char* fmt, ...);
```

The variadic form of `vformat`, with the same guarantees. Handy when you want a `std::string` rather than direct output:

```cpp
Serial.print(xewe::str::format("hue %3u\n", hue).c_str());
```

## Number parsing

```cpp
template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
inline bool parse_int(std::string_view s, T& out);
```

Writes to `out` and returns `true` only on a clean parse. It trims surrounding whitespace, accepts
**base 10 only**, rejects trailing characters, and range-checks against
`std::numeric_limits<T>`. Signed `T` goes through `strtoll`, unsigned through `strtoull`.

A leading `-` for an unsigned `T` is rejected (`"-1"`, `"-0"`), and a value beyond the 64-bit
range (`strtoll`/`strtoull` set `ERANGE`) fails instead of saturating: `parse_int<uint64_t>("-1", out)`
and `parse_int<long long>("9223372036854775808", out)` both return `false`.

```cpp
template <typename T, typename = std::enable_if_t<std::is_floating_point<T>::value>>
inline bool parse_float(std::string_view s, T& out);
```

The same contract for floating-point types, through `strtod`. It trims, rejects trailing
characters, and reports `false` on overflow (`ERANGE`) — which also fires on denormal
**underflow**, so `parse_float<double>("1e-320", d)` returns `false`.

Both functions copy the view into a `std::string` first, so neither is allocation-free.

Compared with `xewe::validate`: these return a `bool` and leave range-checking to you; `validate`
adds the range check and returns an `std::optional`. Both are exception-free and both reject
trailing garbage — they agree on what counts as a number.

## Time and day parsing

```cpp
inline bool parse_time      (std::string_view time_str, uint16_t& minutes);
inline bool parse_day       (std::string_view day_str,  uint8_t&  day_num);
inline bool parse_gmt_offset(std::string_view s, std::string& normalized_gmt);
```

**`parse_time`** reads `H:MM` with `sscanf` and writes minutes since midnight. Accepts `00:00`
through `23:59`, **plus the special value `24:00`** (`1440`), which schedulers use as an
end-of-day marker. Anything else returns `false`.

**`parse_day`** takes a two-letter, case-insensitive day and writes its index:

| `MO` | `TU` | `WE` | `TH` | `FR` | `SA` | `SU` |
|---|---|---|---|---|---|---|
| 0 | 1 | 2 | 3 | 4 | 5 | 6 |

Only the two-letter form is accepted — `"Mon"` and `"Monday"` both fail.

**`parse_gmt_offset`** normalizes a timezone offset into `GMT±HH:MM`:

| Input | Result |
|---|---|
| `GMT`, `GMT0`, `UTC`, `UTC0` | `GMT+00:00` |
| `GMT-8`, `GMT+5:30` | `GMT-08:00`, `GMT+05:00` … sign then `H:MM` |
| `GMT+0530` | four bare digits split as `HHMM` |
| `GMT+5` | up to six characters total is read as hours only |

Range: `0 ≤ h ≤ 14`, `0 ≤ m < 60`, and `h == 14` requires `m == 0`. Anything outside that, or a
string not starting with `GMT`, returns `false` and leaves `normalized_gmt` untouched.

## Command extraction

```cpp
inline std::vector<std::string> extract_commands(std::string_view blob);
inline std::string              escape_json     (std::string_view s);
```

`extract_commands` pulls double-quoted segments out of a blob, honouring `\` escapes. If exactly
one segment comes out and that segment itself contains two or more unescaped quotes, it recurses
once into it — which unwraps a command list that arrived as a JSON string inside a JSON string.

`escape_json` escapes `"` and `\` only. It is not a general JSON escaper: control characters,
newlines and non-ASCII bytes pass through unchanged.
