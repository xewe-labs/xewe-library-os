# Os

`src/XeWeCore/XeWeOs.h` — owns the core services and every registered module.

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
    xewe::SerialPortConfig serial          = {};
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

Member declaration order in the class is deliberate: the private `modules` vector and `config` are
declared **before** the public service members, so the vector already exists when `system`
constructs itself and registers.

## begin

```cpp
void begin();
```

Call once from `setup()`. In order:

1. `serial.begin(config.serial)` — which blocks for `startup_delay_ms`.
2. Installs an NVS error handler that prints to the console:
   `nvs.set_error_handler([this](std::string_view m) { serial.print(m); })`. XeWeCore Nvs errors
   therefore appear on the serial port rather than in the ESP log.
3. Prints the banner, when `config.print_banner`, then any registration errors queued by
   [`report_error`](#report_error) (rejected module ids or command names).
4. Reads `init_setup_flag` from the **`root` NVS namespace**. It is unset on the very first boot
   of a device.
5. Calls `begin()` on every registered module, in registration order.
6. **On the first boot only:** prints `Initial Setup Complete`, writes `root/init_setup_flag`, and
   **restarts the device** — only if that write succeeded. If NVS cannot be written (init failure,
   missing or full partition, commit error) it prints
   `! NVS write failed: init_setup_flag not saved, not restarting` (after the Nvs error itself) and
   carries on, so a device with broken NVS stays reachable over the CLI instead of boot-looping;
   module first-boot setup then re-runs on every boot until NVS works.
7. Prints `System Setup Complete`.

**The first boot of a new device ends in a reboot** (when NVS works; see step 6). Everything after `os.begin()` in `setup()` is
not reached on that boot. Anything with a one-time side effect outside NVS has to tolerate running
again.

Boot output order is: banner → the `System` project/version header → each module's
`<name> Setup` header → `System Setup Complete`.

### Build-time device name

`XEWE_DEVICE_NAME` (a string literal) pre-answers the first-boot name prompt, so provisioning can
run unattended:

```bash
xewe build --chip s3 --define 'XEWE_DEVICE_NAME="Laptop Chiller"'
arduino-cli compile --build-property "compiler.cpp.extra_flags='-DXEWE_DEVICE_NAME=\"Laptop Chiller\"'" ...
```

The inner single quotes keep a name with spaces in one compiler argument (arduino-cli splits the
recipe on spaces outside quotes); without them `Chiller"` becomes a stray file name.

When it is defined, `System`'s first-boot setup does not prompt: if `system/device_name` is empty
it writes `XEWE_DEVICE_NAME` there, and either way it prints `Device name: <name>`. A name already
in NVS (set earlier with `$system set_device_name`) is kept. `$system set_device_name` still renames
the device afterwards. A `#define` in the sketch does **not** work: the name is read by
`XeWeOs.cpp`, a library file, so it has to be a build flag or come from the tools' generated
`<XeWeBuildInfo.h>` (which `XeWeOs.cpp` includes when present). Without the define the prompt is
unchanged and nothing of this is compiled in.

## loop

```cpp
void loop();
```

Call every iteration. Runs `cli.loop()` first, then `loop()` on each module **that is
enabled** — a disabled module never has `loop()` called, which is why public functions of a
disableable module should guard with `if (is_disabled()) return;`.

## register_module

```cpp
bool register_module(Module& module);
```

Called from `Module`'s constructor; a sketch does not call it. Returns `false` and registers
nothing when the id is already registered, empty, contains whitespace, equals `help` or is longer
than 15 characters (see [Module](module.md)). The rejection is reported through `report_error` and
the module gets no CLI group, so it never begins, never loops and adds no commands.

## report_error

```cpp
void report_error(const char* fmt, ...);
```

`printf`-style; prints the message (truncated at 127 characters). Before `begin()` (static
constructors, Serial not up yet) it is queued and printed by `begin()` right after the banner.

## get_module, get_modules, get_config

```cpp
Module*                       get_module (std::string_view id) const;
const std::vector<Module*>&   get_modules()                    const;
const OsConfig& get_config ()                    const;
```

`get_module` returns `nullptr` when nothing matches; the match is exact and case-sensitive. It is
the way to reach a module you do not hold a reference to. Prefer a constructor reference plus
`add_requirement` for a real dependency — see [module.md](module.md).

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
