# xewe::span

`src/XeWeCore/Utils/Span.h` — a contiguous view over elements someone else owns.

On C++20 and later `xewe::span<T>` **is** `std::span<T>`, an alias and nothing more. On C++17 it
is a small stand-in with only the operations the XeWe code uses.

The ESP32 core builds at `-std=gnu++2b`, so on device this is always the alias. The stand-in
serves host builds at `-std=c++17`. Public signatures such as `command_function_t` are spelled
`xewe::span` so they are identical on both.

```cpp
#include <XeWeCore.h>

void handler(xewe::span<const std::string> args) {
    if (args.empty()) return;
    for (const auto& a : args) { /* ... */ }
}
```

## What it supports

| Member | Notes |
|---|---|
| `span()` | empty |
| `span(pointer, size_type)` | pointer and length |
| `span(C&)` / `span(const C&)` | any contiguous container whose `data()` converts — `std::vector`, `std::array` |
| `span(T (&)[N])` | a C array |
| `size()`, `empty()`, `data()` | |
| `operator[]` | unchecked |
| `begin()`, `end()` | range-`for` |

## Notes

* **It does not own anything.** The storage it points at must outlive the span. The usual trap is
  building one from a temporary container.
* **It is not a `std::span` implementation.** No `subspan`, `first`, `last`, `extent`, reverse
  iterators or static extents. A new member must be one C++20 `std::span` also has, or the two
  stop behaving alike.
* **The C++17 fallback accepts a temporary** where a real `std::span` would reject it (it has no
  borrowed-range constraint). Code that compiles under C++17 can therefore fail on a C++20 core.
  Do not rely on it.
