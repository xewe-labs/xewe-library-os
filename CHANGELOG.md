# Changelog

All notable changes to XeWeCore. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow [Semantic Versioning](https://semver.org/). "(host-verified only)" marks a change
that passed the host tests but has not yet run on a board; "(compile-verified only)" marks one that
neither host nor board has exercised (the code that is not host-built).

## Unreleased

### Fixed

- **C-H1** Use-after-free when a command handler adds or removes commands in its own group: both `execute` paths now call a copy of the `std::function` (`src/XeWeCore/Cli.cpp`). (host-verified only)
- **H1** A corrupt FlexData blob could make `blob_read` reserve gigabytes and abort (boot loop if read in `begin()`); the reservation is now capped at the bytes left (`src/XeWeCore/FlexData.h`).
- **N-H1** Boot loop when NVS cannot be written: the first boot now restarts only if `root/init_setup_flag` was saved, else prints `! NVS write failed: init_setup_flag not saved, not restarting` and continues to the CLI (`src/XeWeCore/XeWeOs.cpp`). (compile-verified only)
- **Wave-1 #1** `$<module> disable` and `$system reset` confirmations blocked `Os::loop` forever; they are now bounded (see Changed) (`src/XeWeCore/Module.cpp`, `src/XeWeCore/XeWeOs.cpp`). (host-verified only)
- **Wave-1 #2** An input line over 254 characters was split and its tail ran as a separate command; the whole line is now dropped (see Changed) (`src/XeWeCore/Serial.cpp`, `src/XeWeCore/Serial.h`). (host-verified only)
- **M2** `str::parse_int` accepted `-1`/`-0` for unsigned types and saturated on 64-bit overflow; both now fail (`src/XeWeCore/Utils/String.h`).
- **M3** `validate<T>` truncated the value to `T` when the bounds were wider than `T`; it now parses as `T` (`src/XeWeCore/Utils/Validator.h`).
- **N-M1** NVS init, namespace-open, write and commit failures were silent; they now reach the error handler (`src/XeWeCore/Nvs.cpp`, `src/XeWeCore/Nvs.h`). (host-verified only)
- **N-M2** The automatic whole-partition erase on `ESP_ERR_NVS_NO_FREE_PAGES`/`NEW_VERSION_FOUND` was silent; it is now reported (`src/XeWeCore/Nvs.cpp`). (host-verified only)
- **C-M1** The parsed and direct `execute` paths disagreed on same-name commands with different argument counts; the parsed path now also matches on count (`src/XeWeCore/Cli.cpp`). (host-verified only)
- **C-L1** Most CLI error messages were followed by a blank line (`src/XeWeCore/Cli.cpp`). (host-verified only)
- **N-L1** `Nvs::ScopedHandle` was copyable, so a copy would close the handle twice; copying is now deleted (`src/XeWeCore/Nvs.h`).
- **N-L2** `nvs.write(ns, key, "literal")` warned under `-Wall` (`src/XeWeCore/Nvs.tpp`).
- **Module id length** An id over 15 characters made every NVS operation fail, so the module re-ran first boot on every boot; `Os::register_module` now refuses it (`src/XeWeCore/XeWeOs.cpp`, `src/XeWeCore/Module.cpp`). (compile-verified only)
- **Duplicate ids (C-M2)** A second module with an existing id (case-insensitive) silently merged into the first one's CLI group; it is now refused with `! Module id '<id>' is already registered: module not registered` (`src/XeWeCore/XeWeOs.cpp`, `src/XeWeCore/Module.cpp`). (compile-verified only)
- **C-L2** Unreachable registrations were accepted: `Cli::name_error` now rejects empty names and names with whitespace, and for module ids also `help` and more than 15 characters (`src/XeWeCore/Cli.h`, `src/XeWeCore/Cli.cpp`, `src/XeWeCore/Module.cpp`). (host-verified only)

### Added

- `XEWE_TESTING` compile-time hooks: the `$test` CLI group used by the hardware tests, all code inside `#ifdef XEWE_TESTING`, zero bytes in a production build (`src/XeWeCore/Testing.h`, `src/XeWeCore/Testing.cpp`, `src/XeWeCore/XeWeOs.cpp`).
- Hardware test suite run through the `xewe-os-tools` pytest plugin: black-box, hook and soak/recovery tests (`extras/hwtest/`).
- Host NVS shim and new host tests for NVS, FlexData, the tokenizer, parsers, CLI edge cases and bounded prompts (`extras/host/shim/{nvs,nvs_flash,esp_err,esp_log}.h`, `extras/host/test/test_{nvs,parsers,tokenizer,cli_edges,confirm}.cpp`, `extras/host/test/json/`, `extras/host/run.sh`).
- `Os::report_error(fmt, ...)`: prints a registration error, or queues it until `Os::begin()` when raised before Serial is up (`src/XeWeCore/XeWeOs.h`, `src/XeWeCore/XeWeOs.cpp`).

### Changed

- `$<module> disable` and `$system reset` confirmations are bounded: `get_yn("OK?", 2, 15000, false, …)`, two attempts of 15 s, anything but a clear "yes" cancels, worst-case stall 30 s (`src/XeWeCore/Module.cpp`, `src/XeWeCore/XeWeOs.cpp`). (host-verified only)
- An input line over 254 characters is dropped whole with one `! Input line too long (max 254 chars): dropped` notice instead of being split (`src/XeWeCore/Serial.cpp`). (host-verified only)
- NVS errors (init, automatic erase, open, write, commit) are reported through `Nvs::set_error_handler`, with the `esp_err_to_name()` code (`src/XeWeCore/Nvs.cpp`). (host-verified only)
- `.gitignore` ignores `__pycache__/` (from `extras/hwtest/`).

### Docs

- Updated for tonight's changes: `doc/serial/input.md`, `doc/serial/prompts.md`, `doc/cli/commands.md`, `doc/cli/execution.md`, `doc/nvs/nvs.md`, `doc/os/os.md`, `doc/os/module.md`, `doc/os/system.md`, `doc/utils/string.md`, `doc/utils/validator.md`.
- `doc/AGENTS.md`: new sections on the test hooks and the hardware tests; corrected the input-buffer, prompt and NVS-error rules.
- `README.md`: the release notes moved to this file.

## 2.0.1

### Fixed

- Serial input keeps up to four completed lines in a queue instead of one, so lines arriving together are no longer overwritten or corrupted; a line over 254 characters carried the 255th character into the next line instead of dropping it (superseded in Unreleased: such a line is now dropped whole). No API change (`src/XeWeCore/Serial.cpp`, `src/XeWeCore/Serial.h`, `doc/serial/input.md`).

## 2.0.0

### Changed

- XeWeCore 2.0.0 replaces XeWeUtils, XeWeSerial, XeWeCli, XeWeNvs and XeWeOS (all 1.0.0) in one library; behaviour, NVS keys, module ids and CLI commands are unchanged, so devices keep their stored data. See [Upgrading](README.md#upgrading-from-the-100-libraries).
- The default `OsConfig::url` printed in the boot header points to `https://github.com/xewe-labs/xewe-os-core`.
