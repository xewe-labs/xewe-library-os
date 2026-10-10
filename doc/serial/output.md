# Printing

`src/XeWeCore/Serial.h` — formatted output: plain lines, boxed text, separators, headers and
tables.

Every function here writes CRLF line endings (`xewe::str::kCRLF`).

## print

```cpp
void print(std::string_view message        = {},
           std::string_view end            = xewe::str::kCRLF,
           std::string_view edge_character = {},
           const char       text_align     = 'l',
           const char       wrap_mode      = 'w',
           const uint16_t   message_width  = 0,
           const uint16_t   margin_l       = 0,
           const uint16_t   margin_r       = 0);
```

| Parameter | |
|---|---|
| `message` | the text; embedded `\n` splits it into lines, and a trailing `\r` is stripped from each |
| `end` | written after the final line only; intermediate lines always get CRLF |
| `edge_character` | drawn at both ends of every line, e.g. `"|"` |
| `text_align` | `'l'`, `'r'`, `'c'`; anything else behaves as `'l'` |
| `wrap_mode` | `'c'`/`'C'` = hard character chunks, anything else = word wrap |
| `message_width` | field width for the text. **`0` disables wrapping and alignment entirely** |
| `margin_l`, `margin_r` | spaces between the edge and the field |

`print()` with no arguments prints an empty line.

**Wrapping only happens when `message_width > 0`.** With the default `0`, the text is printed
as-is between the margins, and `text_align` and `wrap_mode` have no effect.

```cpp
serial.print("Saved.");
serial.print("A long sentence that should be wrapped inside a box.",
             xewe::str::kCRLF, "|", 'l', 'w', 46, 1, 1);
```

## printf

```cpp
void printf(const char* fmt, ...);
```

Formats with [`xewe::str::vformat`](../utils/string.md) and prints with all of `print`'s defaults,
so it **appends CRLF**. `vformat` sizes the result exactly (two passes of `vsnprintf`), so long
output is not truncated. `printf(nullptr)` prints an empty line.

There is no `__attribute__((format))`, so a format/argument mismatch is not caught at compile
time.

## printf_fmt

```cpp
void printf_fmt(std::string_view end,
                std::string_view edge_character,
                const char       text_align,
                const char       wrap_mode,
                const uint16_t   message_width,
                const uint16_t   margin_l,
                const uint16_t   margin_r,
                const char*      fmt,
                ...);
```

`printf` with `print`'s layout parameters. **None of them have defaults**, so all seven come
first, in order, before the format string:

```cpp
serial.printf_fmt(xewe::str::kCRLF, "|", 'c', 'w', 46, 1, 1,
                  "Uptime %lu s", millis() / 1000);
```

## print_separator

```cpp
void print_separator(const uint16_t   total_width    = 50,
                     std::string_view fill           = "-",
                     std::string_view edge_character = "+");
```

Draws `+------...------+` at `total_width` characters. `fill` is cycled, so `"-="` and other
multi-character patterns work. `total_width == 0` prints an empty line; a width no larger than the
two edges degrades to a truncated edge string.

## print_spacer

```cpp
void print_spacer(const uint16_t   total_width    = 50,
                  std::string_view edge_character = {});
```

The same shape filled with spaces: a blank row inside a box. With the default empty edge it is a
run of spaces.

## print_header

```cpp
void print_header(std::string_view message,
                  const uint16_t   total_width          = 50,
                  std::string_view edge_character       = "|",
                  std::string_view cross_edge_character = "+",
                  std::string_view sep_fill             = "-");
```

Prints a separator, then the message centred with a one-character margin on each side, then
another separator.

**A `\sep` token inside the message draws a separator line.** The message is split on the literal
four characters `\sep`, and each part is printed centred and followed by a separator:

```cpp
serial.print_header("blink-device\sepVersion 0.1.0");
```

```
+------------------------------------------------+
|                  blink-device                  |
+------------------------------------------------+
|                 Version 0.1.0                  |
+------------------------------------------------+
```

The content width is `total_width - (2 * edge_character.size() + 2)` when an edge is set and the
width allows it, otherwise `total_width`.

## print_table and render_table

```cpp
void print_table(const std::vector<std::vector<std::string_view>>& table,
                 std::string_view header_content       = {},
                 const uint16_t   max_col_width        = 30,
                 std::string_view edge_character       = "|",
                 std::string_view cross_edge_character = "+",
                 std::string_view sep_fill             = "-");

std::string render_table(const std::vector<std::vector<std::string_view>>& table,
                         std::string_view header_content       = {},
                         const uint16_t   max_col_width        = 30,
                         std::string_view edge_character       = "|",
                         std::string_view cross_edge_character = "+",
                         std::string_view sep_fill             = "-") const;
```

`render_table` is `const` and returns the whole table as one CRLF-separated string.
`print_table` writes exactly that string. Use `render_table` when the table also goes elsewhere,
such as a log or a web response.

Layout rules:

* Column count is the longest row; short rows are padded with empty cells.
* Column width is the longest single line in any cell of that column, **plus 2**, clamped to
  `max_col_width`. The 2 is one space on each side of the text.
* Cells may contain `'\n'`; each segment is word-wrapped to `width - 2`. Row height is the tallest
  cell in the row.
* Keep `max_col_width` at 3 or more. Below that, cells wrap at one character but the column is
  narrower, so the edges no longer line up.
* A `+---+---+` divider is drawn before the first row and after every row.
* A non-empty `header_content` adds a full-width separator and a centred title above the table.
* An empty `table` renders an empty string.

```cpp
std::string age_str = std::to_string(age);           // must outlive the call
serial.print_table({
    {"Field", "Value"},
    {"Name",  name},
    {"Age",   age_str},
}, "Summary");
```

**The cells are `std::string_view`s.** What they point at must outlive the call, so build
temporaries into named variables first, as above. A `std::to_string(...)` written inline in the
table is destroyed before the table is rendered.
