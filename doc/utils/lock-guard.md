# xewe::LockGuard

`src/XeWeCore/Utils/LockGuard.h` — RAII guard for a FreeRTOS mutex.

This header includes `<freertos/FreeRTOS.h>` and `<freertos/semphr.h>`, so it needs a core with
FreeRTOS, which every ESP32 core has. `<XeWeCore.h>` always includes it.

```cpp
SemaphoreHandle_t mutex = xSemaphoreCreateMutex();

void touch_shared_state() {
    xewe::LockGuard guard(mutex);    // takes here
    shared_value++;
}                                    // gives here, on every exit path
```

## Constructor

```cpp
explicit LockGuard(SemaphoreHandle_t mutex_handle);
```

Takes the mutex with `portMAX_DELAY`.

## Destructor

```cpp
~LockGuard();
```

Gives the mutex. Runs on every scope exit, including an early `return`.

## Copy and move

```cpp
LockGuard(const LockGuard&)            = delete;
LockGuard& operator=(const LockGuard&) = delete;
```

Non-copyable. No move constructor is declared either, and declaring the copy operations as deleted
suppresses the implicit ones — so a `LockGuard` **cannot be moved or returned from a function**.
It is a scope-local object only.

## Notes

* **The constructor blocks indefinitely.** `portMAX_DELAY` means there is no timeout and no
  failure path: if the mutex is never given, the task waits forever. `xSemaphoreTake`'s return
  value is not checked.
* **Not recursive.** Taking the same mutex twice from one task deadlocks. A plain
  `xSemaphoreCreateMutex()` is not recursive; if you need reentrancy, use
  `xSemaphoreCreateRecursiveMutex()` and its own take/give calls rather than this guard.
* The handle is not validated. Constructing a guard over a null handle is undefined behaviour;
  check the result of `xSemaphoreCreateMutex()` at creation time.
