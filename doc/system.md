# System

`src/System/System.h` — the built-in module. Always present, always first, cannot be disabled.

`System` is a member of `ModuleController`, reachable as `controller.system` (or `os.system`). It
is constructed as:

```cpp
Module(controller, "system", "System", "Stores integral commands and routines",
       /* requires_init_setup */ true,
       /* can_be_disabled     */ false,
       /* has_cli_commands    */ true)
```

Because it is a member, it registers before any module declared in the sketch, so it begins first
and its boot header is the first thing after the banner.

## Class

```cpp
class System : public Module {
public:
    explicit    System                 (ModuleController& controller);

    void        begin_routines_required()                               override;
    void        begin_routines_init    ()                               override;
    void        reset                  (const bool verbose      = false,
                                        const bool do_restart   = true,
                                        const bool keep_enabled = true) override;
    std::string status                 (const bool verbose = false)     const override;

    std::string get_device_name        ();
    void        restart                (uint16_t delay_ms = 1000);
};
```

## $system commands

| Command | Args | Effect |
|---|---|---|
| `$system restart` | 0 | `restart(1000)` |
| `$system reboot` | 0 | identical alias |
| `$system info` | 0 | chip model, cores, revision, IDF version, flash size and speed, Wi-Fi station MAC |
| `$system set_device_name "<name>"` | 1 | stores `system/device_name` and prints `Device name set to: <name>` |
| `$system mac` | 0 | one line per interface that reads back: `wifi_sta`, `wifi_ap`, `bt`, `eth` |
| `$system uid` | 0 | `base_mac <hex>` from the eFuse base MAC, and `uid64 <hex>` — the first 8 bytes of its SHA-256 |
| `$system status` | 0 | inherited, but [overridden](#status) to print a table of every module |
| `$system reset` | 0 | inherited, but [overridden](#reset) — a full factory reset |

There is **no** `$system enable` or `$system disable`, because `can_be_disabled` is `false`.

`$help` and `$<group>` come from
[XeWeCli](https://github.com/xewe-labs/xewe-library-cli/blob/main/doc/help.md), not from this
library.

**`set_device_name` does not reboot and does not notify anything.** A module that cached the name
at boot keeps the old one until the next restart.

## begin_routines_required

Prints the boot header from the controller config — `<project_name>`, `Version <version>`, the
build timestamp when set, and the url when set — then calls:

```cpp
esp_log_level_set("*", ESP_LOG_NONE);
```

**This silences all ESP-IDF logging for the entire firmware**, not just this library: every
`ESP_LOGE`/`ESP_LOGW`/`ESP_LOGI` from the IDF, from Wi-Fi, and from any other library goes quiet
from the first boot header onward. It is deliberate — the console is a user interface here, not a
log — but it is the first thing to undo when debugging something that should have logged.

(It also mutes XeWeNvs's default `ESP_LOGE` sink, which is harmless because
`ModuleController::begin` has already redirected NVS errors to the serial console.)

## begin_routines_init

The one-time device naming, run until it is confirmed:

```
Name your device (ex: Kitchen Lights):
>
Kitchen Lights
Confirm "Kitchen Lights"?
(y/n) >
y
```

(The `>` marker is printed on its own line, so typed input echoes on the line below it.)

The confirmed value is written to `system/device_name`. An empty name is accepted if the user
confirms it.

## status

```cpp
std::string status(const bool verbose = false) const override;
```

With `verbose`, prints a table titled `System Status` with a row per registered module:

```
+-----------------------------------------------+
|                 System Status                 |
+-------------+---------+-----------------------+
| Module Name | Enabled | Status                |
+-------------+---------+-----------------------+
| System      | Yes     | System module enabled |
+-------------+---------+-----------------------+
| Blink       | No      | Blink module disabled |
+-------------+---------+-----------------------+
```

**The return value is the literal string `"System OK"`, not the table.** The table is printed as a
side effect and is not obtainable as a string from here.

## reset

```cpp
void reset(const bool verbose = false, const bool do_restart = true,
           const bool keep_enabled = true) override;
```

A **factory reset**:

1. When `verbose`, prints `[WARNING] / Resetting System / Will reset all modules` and asks `OK?`.
2. Calls `reset(true, false, false)` on every other registered module — wiping each namespace
   without rebooting between them.
3. Calls `nvs.erase_all()`, which wipes the **entire NVS partition** — including `root/init_setup_flag`,
   so the next boot re-runs the whole initial setup, and including data belonging to other
   libraries.
4. Calls `Module::reset(verbose, do_restart, keep_enabled)`.

**The confirmation is mandatory in practice.** The internal `disable_confirmed` starts as `false`
and is only set by the prompt, so `system.reset(false, ...)` called from code always prints
`Aborted` and does nothing. Only `$system reset`, which passes `verbose = true`, can actually
reset the device.

## get_device_name

```cpp
std::string get_device_name();
```

Reads `system/device_name`, returning `""` when it has never been set.

**It is not `const`**, so it cannot be called from a `const` member function — including from a
`status() const` override. Cache the name in your module instead.

## restart

```cpp
void restart(uint16_t delay_ms = 1000);
```

Prints a `Rebooting` header, waits `delay_ms`, then calls `ESP.restart()`. The delay gives the
serial buffer time to flush so the user sees why the device went away.

This is what every module's `enable`, `disable` and `reset` call when `do_restart` is true.
