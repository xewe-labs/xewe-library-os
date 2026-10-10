# XeWeCore documentation

XeWeCore is a modular firmware core for ESP32: a serial console, a `$group command` command line,
typed NVS storage, helpers and a module framework, in one Arduino library. Sketches include
`<XeWeCore.h>`; the component headers under `src/XeWeCore/` can be included after it.

## Reading path

1. **[Concepts](concepts.md)**: what the core is for, modules, the console, storage, the
   settings schema, listeners, and the principles behind them.
2. **Pick your level** (below) and run its example.
3. **[Os](os/os.md) → [Module](os/module.md) → [Settings table](os/settings.md)**: the three
   pages a module author needs.
4. **The reference pages** below, when you need the exact behaviour of a function, command or
   stored key.

## Three levels

| Level | Who | Tooling | What you get |
|---|---|---|---|
| 1 | Arduino IDE user | Library Manager → `XeWeCore`; compile, upload | the console: `$help`, `$system status`, a device name in NVS, prompts. Example [`01_Hello`](../examples/01_Hello/01_Hello.ino) |
| 2 | Arduino IDE user | the same, plus one module of your own in the sketch folder | your own `$mymodule …` commands, settings in NVS, enable/disable. Example [`02_MyModule`](../examples/02_MyModule/02_MyModule.ino) |
| 3 | project builder | the [xewe-os](https://github.com/xewe-labs/xewe-os) template and its build tools | ready-made modules (Wi-Fi, web interface, scheduler, …), multi-chip builds, board tests, releases |

The modules repository is not an Arduino library: ready-made modules are level 3. Level 2 is
always *your own* module.

## Reference

### Os: `src/XeWeCore/XeWeOs.h`, `Module.h`, `Settings.h`

| Page | Covers |
|---|---|
| [os/os.md](os/os.md) | `xewe::Os` / `XeWeOs`, `OsConfig`, `begin`, `loop`, module lookup, the core services, `XEWE_DEVICE_NAME` |
| [os/module.md](os/module.md) | `xewe::Module`: constructor, lifecycle, enable/disable/reset, requirements, commands, listeners, NVS keys |
| [os/settings.md](os/settings.md) | the settings table: `setting<>`, flags, `$<id> set\|get\|schema`, schema lines, `schema_extra` |
| [os/system.md](os/system.md) | `xewe::System`, the built-in module, and the `$system` commands including `$system schema` |

### Serial: `src/XeWeCore/Serial.h`

| Page | Covers |
|---|---|
| [serial/config.md](serial/config.md) | `xewe::SerialPortConfig` and `begin` |
| [serial/output.md](serial/output.md) | `print`, `printf`, boxes, separators, headers, tables |
| [serial/input.md](serial/input.md) | non-blocking line input: `loop`, `has_line`, `read_line` |
| [serial/prompts.md](serial/prompts.md) | blocking typed prompts: `get_string`, `get_int`, `get_yn`, ... |

### Cli: `src/XeWeCore/Cli.h`

| Page | Covers |
|---|---|
| [cli/commands.md](cli/commands.md) | `Command`, groups, registration |
| [cli/execution.md](cli/execution.md) | the two `execute` overloads, tokenizer and error strings |
| [cli/help.md](cli/help.md) | `$help` output |

### Nvs: `src/XeWeCore/Nvs.h`, `FlexData.h`

| Page | Covers |
|---|---|
| [nvs/nvs.md](nvs/nvs.md) | `xewe::Nvs`: typed read/write, namespaces, errors |
| [nvs/flexdata.md](nvs/flexdata.md) | `xewe::FlexData<T>`: declare fields once, get blob and JSON forms; type rules, field presence |
| [nvs/blob-format.md](nvs/blob-format.md) | the stored byte layout and how to version a struct |

### Utils: `src/XeWeCore/Utils.h`

| Page | Covers |
|---|---|
| [utils/string.md](utils/string.md) | `xewe::str`: case, hex colours, trimming, parsing, wrapping, formatting, box lines |
| [utils/validator.md](utils/validator.md) | `xewe::validate<T>` |
| [utils/async-timer.md](utils/async-timer.md) | `xewe::AsyncTimer<T>` |
| [utils/color.md](utils/color.md) | `xewe::color` HSV/RGB |
| [utils/pins.md](utils/pins.md) | `xewe::pins`: GPIO ownership registry, strapping pins |
| [utils/span.md](utils/span.md) | `xewe::span` |
| [utils/lock-guard.md](utils/lock-guard.md) | `xewe::LockGuard` |
| [utils/debug.md](utils/debug.md) | `DBG_*` macros and `DEBUG_<Class>` flags |

`xewe::ListenerSet` (`Utils/Listeners.h`) is documented with modules:
[os/module.md#listeners](os/module.md#listeners).

## Also

* [Examples](examples.md): what each sketch in `examples/` shows.
* [Tests](tests.md): `setup.sh`, `run.sh`, the host unit tests and the board tests.
* Rules for changing the core (layout, include direction, what a change must carry):
  [`.agents/AGENTS.md`](../.agents/AGENTS.md).
