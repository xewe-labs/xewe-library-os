# XeWeCore reference

Every public type and function, by component. Sketches include `<XeWeCore.h>`; the component
headers under `src/XeWeCore/` can be included after it.

## Os — `src/XeWeCore/XeWeOs.h`, `Module.h`

| Page | Covers |
|---|---|
| [os/os.md](os/os.md) | `xewe::Os` / `XeWeOs`, `OsConfig`, `begin`, `loop`, module lookup, the core services |
| [os/module.md](os/module.md) | `xewe::Module`: constructor, lifecycle, enable/disable/reset, requirements, commands |
| [os/system.md](os/system.md) | `xewe::System`, the built-in module, and the `$system` commands |

## Serial — `src/XeWeCore/Serial.h`

| Page | Covers |
|---|---|
| [serial/config.md](serial/config.md) | `xewe::SerialPortConfig` and `begin` |
| [serial/output.md](serial/output.md) | `print`, `printf`, boxes, separators, headers, tables |
| [serial/input.md](serial/input.md) | non-blocking line input: `loop`, `has_line`, `read_line` |
| [serial/prompts.md](serial/prompts.md) | blocking typed prompts: `get_string`, `get_int`, `get_yn`, ... |

## Cli — `src/XeWeCore/Cli.h`

| Page | Covers |
|---|---|
| [cli/commands.md](cli/commands.md) | `Command`, groups, registration |
| [cli/execution.md](cli/execution.md) | the two `execute` overloads, tokenizer and error strings |
| [cli/help.md](cli/help.md) | `$help` output |

## Nvs — `src/XeWeCore/Nvs.h`, `FlexData.h`

| Page | Covers |
|---|---|
| [nvs/nvs.md](nvs/nvs.md) | `xewe::Nvs`: typed read/write, namespaces, errors |
| [nvs/flexdata.md](nvs/flexdata.md) | `xewe::FlexData<T>`: declare fields once, get blob and JSON forms |
| [nvs/blob-format.md](nvs/blob-format.md) | the stored byte layout and how to version a struct |

## Utils — `src/XeWeCore/Utils.h`

| Page | Covers |
|---|---|
| [utils/string.md](utils/string.md) | `xewe::str`: case, trimming, parsing, wrapping, formatting, box lines |
| [utils/validator.md](utils/validator.md) | `xewe::validate<T>` |
| [utils/async-timer.md](utils/async-timer.md) | `xewe::AsyncTimer<T>` |
| [utils/color.md](utils/color.md) | `xewe::color` HSV/RGB |
| [utils/span.md](utils/span.md) | `xewe::span` |
| [utils/lock-guard.md](utils/lock-guard.md) | `xewe::LockGuard` |
| [utils/debug.md](utils/debug.md) | `DBG_*` macros and `DEBUG_<Class>` flags |

Rules for coding agents: [AGENTS.md](AGENTS.md). Release notes: [CHANGELOG.md](../CHANGELOG.md).
