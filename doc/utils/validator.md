# xewe::validate

`src/XeWeCore/Utils/Validator.h` — parse a string into `T` and range-check it in one call.

```cpp
if (auto level = xewe::validate<uint8_t>(args[0], 0, 255)) {
    analogWrite(pin, *level);
} else {
    serial.print("Level must be 0-255.");
}
```

## validate

```cpp
template <typename T, typename LimitT>
std::optional<T> validate(std::string_view value, LimitT min, LimitT max);
```

Returns the parsed value, or `std::nullopt` if it does not parse or falls outside `[min, max]`.

`LimitT` is a separate template parameter so plain literals deduce naturally — `validate<uint8_t>(s, 0, 255)`
compiles without casting the bounds.

| `T` | What `min`/`max` mean | Parsed with |
|---|---|---|
| `std::string` | **string length**, inclusive | no parsing; the view is copied |
| signed integral | numeric range, inclusive | `xewe::str::parse_int<T>` |
| unsigned integral | numeric range, inclusive | `xewe::str::parse_int<T>` |
| `float`, `double` | numeric range, inclusive | `xewe::str::parse_float` |
| anything else, **including `bool`** | — | compile error |

An unsupported `T` fails with `static_assert`:
`"Unsupported Validator::validate<T>() type."`

## Notes

* **This function is exception-free.** It delegates to
  [`xewe::str::parse_int`](string.md#number-parsing) and `parse_float`, which report failure by
  returning `false`. It compiles with `-fno-exceptions`, which most Arduino cores use.
* **Trailing characters are rejected.** `validate<int>("12abc", 0, 100)` and
  `validate<int>("1.5", 0, 100)` return `std::nullopt`. `validate` and `xewe::str::parse_int`
  agree on what counts as a number.
* **Base 10 only.** `validate<int>("0x1F", 0, 255)` returns `std::nullopt`. Surrounding
  whitespace is trimmed and a leading `+`/`-` is accepted.
* **The value must fit `T` itself**, whatever the bounds: `validate<int8_t>("300", 0, 1000)`
  returns `std::nullopt`, never a truncated `44`.
* **It allocates.** Each call copies the view into a `std::string` before parsing.
* `min` and `max` are **not** swapped if you pass them inverted — an inverted range simply matches
  nothing and every call returns `std::nullopt`. (The `SerialPort` numeric prompts do swap; this
  does not.)
* The header also exposes `template <typename> struct always_false : std::false_type {};`, used
  for the `static_assert` above.
