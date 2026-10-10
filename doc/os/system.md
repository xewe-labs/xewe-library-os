# System

`src/XeWeCore/XeWeOs.h`. The built-in module: always present, always first, cannot be disabled.

`System` is a member of `Os`, reachable as `os.system`. It is constructed as:

```cpp
Module(os, "system", "System", "Stores integral commands and routines",
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
    explicit    System                 (Os& os);

    void        begin_routines_required()                               override;
    void        begin_routines_init    ()                               override;
    void        reset                  (const bool verbose      = false,
                                        const bool do_restart   = true,
                                        const bool keep_enabled = true) override;
    std::string status                 (const bool verbose = false)     const override;

    std::string get_device_name        ();
    void        print_schema_all       ();   // $system schema
    void        restart                (uint16_t delay_ms = 1000);
};
```

## $system commands

| Command | Args | Effect |
|---|---|---|
| `$system restart` | 0 | `restart(1000)` |
| `$system reboot` | 0 | identical alias |
| `$system info` | 0 | chip and build info, [below](#system-info) |
| `$system set_device_name "<name>"` | 1 | stores `system/device_name` and prints `Device name set to: <name>` |
| `$system mac` | 0 | one line per interface that reads back: `wifi_sta`, `wifi_ap`, `bt`, `eth` |
| `$system schema` | 0 | every module's settings as JSON Lines, [below](#system-schema) |
| `$system uid` | 0 | `base_mac <hex>` from the eFuse base MAC, and `uid64 <hex>`: the first 8 bytes of its SHA-256 |
| `$system status` | 0 | inherited, [overridden](#status) to print a table of every module |
| `$system reset` | 0 | inherited, [overridden](#reset): a full factory reset |

There is **no** `$system enable` or `$system disable`, because `can_be_disabled` is `false`.

`$help`, `$system` and `$system help` print the command table ([help](../cli/help.md)).

**`set_device_name` does not reboot and does not notify anything.** A module that cached the name
at boot keeps the old one until the next restart.

### $system info

One `printf`, four lines:

```text
Model <n>  Cores <n>  Rev <n>
IDF <esp-idf version>
Flash <bytes> bytes @ <hz> Hz
MAC <AA:BB:CC:DD:EE:FF>
```

`Model` is the numeric `esp_chip_model_t`. `MAC` is the Wi-Fi station MAC, formatted like
`$system mac`.

### $system schema

Every module's [settings](settings.md) as JSON Lines: a header
`{"schema":1,"core":"2.1.0","device":"<name>","modules":["system",...]}`, then each module's rows
(table rows, then `schema_extra` rows) with `"module":"<id>"` first, then
`{"end":"system","count":<rows>}`. `modules` lists every registered module. Implemented by
`System::print_schema_all()`.

## begin_routines_required

Prints the boot header from the Os config: `<project_name>`, `Version <version>`,
`Build Timestamp <build_timestamp>` when set, and the url when set. Then it calls:

```cpp
esp_log_level_set("*", ESP_LOG_NONE);
```

**This silences all ESP-IDF logging for the entire firmware**, not just this library. Every
`ESP_LOGE`/`ESP_LOGW`/`ESP_LOGI` from the IDF, from Wi-Fi and from any other library goes quiet
from the first boot header on. The console is a user interface here, not a log. When something
should have logged and did not, undo this first.

It also mutes the Nvs default `ESP_LOGE` sink. That is harmless: `Os::begin` has already pointed
Nvs errors at the serial console.

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

The `>` marker is printed on its own line, so typed input echoes on the line below it.

The confirmed value is written to `system/device_name`. An empty name is accepted if the user
confirms it.

Built with `XEWE_DEVICE_NAME`, there is no prompt: the build-time name is written when
`system/device_name` is empty and `Device name: <name>` is printed. See
[Build-time device name](os.md#build-time-device-name).

## status

```cpp
std::string status(const bool verbose = false) const override;
```

With `verbose`, prints a table titled `System Status` with a row per registered module. The
`Status` cell is that module's `status(false)`; for `System` itself that is `System OK`. A module
with a settings table adds its `key: value` lines to the cell.

```
+-----------------------------------------------+
|                 System Status                 |
+-------------+---------+-----------------------+
| Module Name | Enabled | Status                |
+-------------+---------+-----------------------+
| System      | Yes     | System OK             |
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

1. When `verbose`, prints `[WARNING] / Resetting System / Will reset all modules` and asks `OK?`
   with a bounded prompt: two attempts of 15 s, default "no"; a typo or a timeout re-prompts once.
   A "no" prints `Aborted`. A second timeout or invalid answer prints
   `! No answer: reset cancelled`, then `Aborted`. Nothing is reset in either case.
2. Calls `reset(true, false, false)` on every other registered module. Each namespace is wiped,
   with no reboot in between.
3. Calls `nvs.erase_all()`, which wipes the **entire NVS partition**. That includes
   `root/init_setup_flag`, so the next boot runs the whole initial setup again, and data that
   belongs to other libraries.
4. Calls `Module::reset(verbose, do_restart, keep_enabled)`.

**The confirmation is mandatory.** Without `verbose` there is no prompt and the reset counts as
not confirmed, so `system.reset(false, ...)` from code prints `Aborted` and does nothing. Only a
call with `verbose = true`, such as `$system reset`, can reset the device.

## get_device_name

```cpp
std::string get_device_name();
```

Reads `system/device_name`, returning `""` when it has never been set.

**It is not `const`**, so it cannot be called from a `const` member function, such as a
`status() const` override. Cache the name in your module instead.

## restart

```cpp
void restart(uint16_t delay_ms = 1000);
```

Prints a `Rebooting` header, waits `delay_ms`, then calls `ESP.restart()`. The delay gives the
serial buffer time to flush so the user sees why the device went away.

This is what every module's `enable`, `disable` and `reset` call when `do_restart` is true.
