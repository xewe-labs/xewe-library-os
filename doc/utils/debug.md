# Debug macros

`src/XeWeCore/Utils/Debug.h` — opt-in debug printing, enabled per class from build flags.

The macros are in the global namespace, not `xewe::`. They print to `Serial`, so a sketch using
them must have called `Serial.begin()` (or `xewe::SerialPort::begin()`, which does).

## The flag convention

Each class defines its own flag with a default of `0` next to its code:

```cpp
#ifndef DEBUG_MyClass
#define DEBUG_MyClass 0
#endif
```

Enable it from the build instead of editing the library:

```bash
arduino-cli compile --build-property "compiler.cpp.extra_flags=-DDEBUG_MyClass=1" ...
```

```ini
; PlatformIO
build_flags = -DDEBUG_MyClass=1
```

`AsyncTimer` is the only class in this library that ships a flag: `DEBUG_AsyncTimer`.

## DBG_ENABLED

```cpp
#define DBG_ENABLED(cls)       (DEBUG_##cls)
```

Expands to the flag itself, so it can be used in an `if` of your own.

## DBG_PRINTLN

```cpp
DBG_PRINTLN(cls, msg)
```

Prints `[DBG] [<cls>]: ` followed by `Serial.println(msg)`. `cls` is a bare class name, not a
string: `DBG_PRINTLN(MyClass, "started")`.

## DBG_PRINTF

```cpp
DBG_PRINTF(cls, fmt, ...)
```

Same prefix, then the formatted text. No trailing newline is added — put `\n` in the format
string. Formats with `snprintf` into a stack buffer and prints the result, so it does not depend on
`Print::printf`. Output longer than
`DBG_PRINTF_BUFFER_SIZE` (128 by default) is truncated; raise it from build flags if you need
more. For formatting outside a debug macro, use
[`xewe::str::format`](string.md#formatting).

## Notes

* **The check is a runtime `if`, not `#if`.** Both macros expand to
  `do { if (DEBUG_<cls>) { ... } } while (0)`, so the arguments are still compiled and `Serial` is
  still referenced at link time even when the flag is `0`. The compiler drops the dead branch, but
  a `msg` expression with side effects still has to compile, and a `Serial` must exist.
* A missing flag is a compile error, not a silent `0` — `DEBUG_##cls` has to expand to something.
  That is why each class defines its own default.
