# Os

`src/XeWeCore/XeWeOs.h`. The Os owns the core services and every registered module.

```cpp
XeWeOs os({
    .project_name    = "blink-device",
    .version         = "0.1.0",
    .build_timestamp = __DATE__ " " __TIME__,
});

BlinkModule blink(os, {.pin = 8});           // declared after the Os

void setup() { os.begin(); }
void loop()  { os.loop();  }
```

## OsConfig

```cpp
struct OsConfig {
    std::string            project_name    = "xewe-device";
    std::string            version         = "0.0.0";
    std::string            build_timestamp = {};
    std::string            url             = "https://github.com/xewe-labs/xewe-os-core";
    SerialPortConfig       serial          = {};
    bool                   print_banner    = true;
};
```

| Field | Default | |
|---|---|---|
| `project_name` | `"xewe-device"` | first line of the boot header |
| `version` | `"0.0.0"` | printed as `Version <version>` |
| `build_timestamp` | empty | printed only when set; `__DATE__ " " __TIME__` is the usual value |
| `url` | this library's repo | printed under a separator; **set it to your own project or clear it** |
| `serial` | see [SerialPortConfig](../serial/config.md) | `115200` baud, 2048/1024 buffers, 1000 ms startup delay, echo on |
| `print_banner` | `true` | the ASCII XeWe banner at the top of the boot log |

## Core services

Public members, available to every module as `os.<name>`:

| Member | Type | Docs |
|---|---|---|
| `serial` | `xewe::SerialPort` | [Serial](../serial/output.md) |
| `nvs` | `xewe::Nvs` | [Nvs](../nvs/nvs.md) |
| `cli` | `xewe::Cli` | [Cli](../cli/commands.md) |
| `system` | `xewe::System` | [system.md](system.md) |

`system` is a member, so it is always the first module registered and the first to begin.

## Constructor

```cpp
explicit Os(OsConfig config = {});

Os(const Os&)            = delete;
Os& operator=(const Os&) = delete;
```

Non-copyable. Modules hold a reference to it.

The constructor points [`xewe::pins::error_handler`](../utils/pins.md#messages) at
`report_error`, so GPIO claim conflicts from module constructors are queued like registration
errors.

The private `modules` vector and `config` are declared before the public service members. The
vector therefore exists when `system` constructs itself and registers.

## begin

```cpp
void begin();
```

Call once from `setup()`. In order:

1. `serial.begin(config.serial)`, which blocks for `startup_delay_ms`.
2. Points the Nvs error handler and `xewe::flex_error_handler` at `serial.print`. Nvs and FlexData
   errors therefore appear on the console, not in the ESP log.
3. Prints the banner when `config.print_banner` is set, then any errors queued by
   [`report_error`](#report_error) (rejected module ids or command names).
4. With `XEWE_TESTING` defined, registers the `$test` hooks used by the board tests.
5. Reads `init_setup_flag` from the **`root` NVS namespace**. It is unset on the first boot of a
   device.
6. Calls `begin()` on every registered module, in registration order.
7. **On the first boot only:** prints `Initial Setup Complete`, writes `root/init_setup_flag` and
   **restarts the device** if that write succeeded. If NVS cannot be written (init failure,
   missing or full partition, commit error), it prints
   `! NVS write failed: init_setup_flag not saved, not restarting` after the Nvs error and carries
   on. A device with broken NVS stays reachable over the CLI instead of boot-looping. Module
   first-boot setup then runs again on every boot until NVS works.
8. Prints `System Setup Complete`.

**The first boot of a new device ends in a reboot** when NVS works (step 7). Code after
`os.begin()` in `setup()` does not run on that boot. Anything with a one-time side effect outside
NVS has to tolerate running again.

Boot output order: banner → queued errors → the `System` project/version header → each module's
`<name> Setup` header → `System Setup Complete`.

### Build-time device name

`XEWE_DEVICE_NAME` (a string literal) pre-answers the first-boot name prompt, so provisioning can
run unattended:

```bash
xewe build --chip s3 --define 'XEWE_DEVICE_NAME="Laptop Chiller"'
arduino-cli compile --build-property "compiler.cpp.extra_flags='-DXEWE_DEVICE_NAME=\"Laptop Chiller\"'" ...
```

The inner single quotes keep a name with spaces in one compiler argument. arduino-cli splits the
recipe on spaces outside quotes; without them `Chiller"` becomes a stray file name.

When it is defined, `System`'s first-boot setup does not prompt. If `system/device_name` is empty
it writes `XEWE_DEVICE_NAME` there. Either way it prints `Device name: <name>`. A name already in
NVS is kept, and `$system set_device_name` still renames the device afterwards.

A `#define` in the sketch does **not** work. The name is read by `XeWeOs.cpp`, a library file, so
it has to be a build flag or come from the tools' generated `<XeWeBuildInfo.h>`, which
`XeWeOs.cpp` includes when present. Without the define the device prompts for a name.

## loop

```cpp
void loop();
```

Call every iteration. Runs `cli.loop()` first, then `loop()` on each **enabled** module. A
disabled module's `loop()` is never called. Public functions of a module that can be disabled
should still start with `if (is_disabled()) return;`, because other code can call them.

## register_module

```cpp
bool register_module(Module& module);
```

Called from `Module`'s constructor; a sketch does not call it. Returns `false` and registers
nothing when the id is empty, contains whitespace, equals `help` in any case, is longer than 15
characters, or is already registered. "Already registered" includes a CLI group whose id differs
only in case. The rejection is reported through `report_error` as
`! Module id '<id>' <reason>: module not registered`. The module gets no CLI group, so it never
begins, never loops and adds no commands. See [Module](module.md#constructor).

## report_error

```cpp
void report_error(const char* fmt, ...);
```

`printf`-style; prints the message, truncated at 127 characters. Before `begin()` (static
constructors, Serial not up yet) the message is queued, and `begin()` prints it right after the
banner.

## get_module, get_modules, get_config

```cpp
Module*                       get_module (std::string_view id) const;
const std::vector<Module*>&   get_modules()                    const;
const OsConfig&               get_config ()                    const;
```

`get_module` returns `nullptr` when nothing matches; the match is exact and case-sensitive. Use it
to reach a module you hold no reference to. For a real dependency, prefer a constructor reference
plus `add_requirement` ([Module](module.md#add_requirement)).

`get_config` is how `System` reads the project name and version for the boot header.

## Declaration order

Modules register themselves from their constructors, and globals in one translation unit are
constructed top to bottom. So:

* the Os is declared **first**,
* a module is declared **after** every module it depends on,
* declaration order is begin order is loop order.

```cpp
XeWeOs os({...});
Wifi         wifi(os);
WebInterface web(os, wifi);        // after wifi
```
