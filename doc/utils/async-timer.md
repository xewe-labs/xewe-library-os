# xewe::AsyncTimer\<T\>

`src/XeWeCore/Utils/AsyncTimer.h` — non-blocking interpolation between two values over a fixed
duration.

```cpp
xewe::AsyncTimer<uint8_t> fade(2000, 0, 255);  // 0 -> 255 over 2 s

void start_fade() { fade.initiate(); }       // nothing moves until this is called

void loop() {
    if (fade.is_active()) analogWrite(pin, fade.get_current_value());
}
```

`T` must be arithmetic; anything else is a `static_assert` failure
(`"AsyncTimer<T> requires an arithmetic type"`).

Debug flag: `DEBUG_AsyncTimer`, defaulted to `0` in the header. See [debug.md](debug.md).

## Constructor

```cpp
AsyncTimer(uint32_t delay, T start = T(), T target = T());
```

| Parameter | |
|---|---|
| `delay` | duration of the interpolation, in milliseconds |
| `start` | value at progress `0.0` |
| `target` | value at progress `1.0` |

Constructing does **not** start the timer. `is_active()` is `false` until `initiate()`.

## initiate

```cpp
void initiate();
```

Starts (or restarts) the run: stamps `millis()`, clears `done`, sets `progress` to `0.0` and
`initiated` to `true`. It back-dates the internal calculation stamp so the first sample is not
throttled.

## reset

```cpp
void reset();
void reset(T new_start, T new_target);
void reset(uint32_t new_delay, T new_start, T new_target);
```

Clears all state — including `initiated`, so **a fresh `initiate()` is required** before the timer
runs again. The two- and three-argument overloads replace the endpoints (and the delay) first,
then do the same reset.

## get_current_value

```cpp
T get_current_value() const;
```

Recomputes progress (subject to the throttle below) and returns the interpolated value. Returns
`target` exactly once the run is done; otherwise `T(start + (target - start) * progress)`.

## Other getters

```cpp
T        get_start_value () const;
T        get_target_value() const;
double   get_progress    () const;
uint32_t get_delay_ms    () const;
bool     is_done         () const;
bool     is_not_done     () const;
bool     is_active       () const;
```

| | |
|---|---|
| `get_progress` | `0.0`-`1.0`. **Does not recompute** — see the note below |
| `is_done` | recomputes, then reports whether the run has finished |
| `is_not_done` | `!is_done()` |
| `is_active` | whether `initiate()` has been called and `terminate()`/`reset()` has not |

## terminate

```cpp
void terminate();
```

Stops the run early by clearing `initiated`. It leaves `done` and `progress` at their last values,
and because the internal recalculation early-returns when `!initiated`, `is_done()` keeps
reporting whatever it reported before. Use `reset()` when you want the state cleared too.

## debug_dump

```cpp
void debug_dump(const char* label = "DUMP") const;
```

Prints the full internal state through `DBG_PRINTF`, so it produces output only when
`DEBUG_AsyncTimer=1`. `initiate()` and `reset()` call it themselves.

## Notes

* **Progress is recomputed at most every 5 ms** (`calc_interval_ms`, private), and only from
  `get_current_value()` and `is_done()`. `get_progress()` returns the cached value, so it can be
  up to 5 ms stale — or `0.0` if neither of the other two has been called since `initiate()`.
* All the state getters are `const`; the cached fields are `mutable` to allow it.
* **Integral `T` truncates.** The interpolation is done in `double` and cast back, so an
  `AsyncTimer<int>` steps rather than glides, and a short `delay` over a wide range skips values.
* **A descending run needs a signed or small `T`.** The expression evaluates `target - start` in
  `T`'s promoted type: `uint8_t` and `uint16_t` promote to `int` and are fine, but an
  `AsyncTimer<uint32_t>` with `target < start` wraps around and produces nonsense. Use a signed
  type for a countdown.
* Nothing here uses interrupts or tasks — progress advances only when you call a getter.
