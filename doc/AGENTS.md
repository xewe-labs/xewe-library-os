# AGENTS.md — xewe-library-os

Rules for coding agents working **anywhere in this repository**, not only in `doc/`.
Organization-wide rules are in
[`.github/AGENTS.md`](https://github.com/xewe-labs/.github/blob/main/AGENTS.md) and win where this
file is silent: never publish, never tag or push, never commit unasked, never flash a board.

## What this library is

The **core only**. It knows no concrete modules, and it must stay that way.

* **Never add a feature module here.** Wi-Fi, buttons, scheduling, a web interface — those are
  separate repositories (`xewe-os-module-*`) or firmware-local modules in `src/<Name>/`. A
  concrete module in this library re-creates the monolith this design replaced.
* The only module that belongs here is `System`, because the controller owns it.

## Things that destroy user data

* **`disable()` calls `reset()`**, which wipes the module's NVS namespace, **and cascades to every
  dependent module.** When `verbose` is false there is no confirmation. Do not call it to "reset
  state" in a test.
* **`System::reset()` erases the entire NVS partition** via `nvs.erase_all()` — every namespace on
  the device, including `root/init_setup_flag`, so the next boot re-runs initial setup. It is a
  factory reset.
* `System::reset`'s `disable_confirmed` starts `false` on purpose, so a programmatic call always
  aborts. Do not "fix" that to `true`.

## Do not break these

* **A module `id` is the CLI group *and* the NVS namespace**, so it is capped at 15 characters by
  XeWeNvs. Changing how the id is used changes where every device's stored data lives.
* **The NVS keys `is_enabled`, `not_first_boot` and `init_complete` are the on-device state
  format.** Renaming one strands the state on every deployed device; a module would come up as if
  it had never booted.
* **The lifecycle order in `Module::begin()` is contractual** and documented step by step in
  [`doc/module.md`](module.md). Modules across the organization rely on
  `begin_routines_required` running before init, and `begin_routines_common` running last.
* **Modules register from their constructors**, so declaration order in the sketch is begin and
  loop order. The private `modules` vector is declared before the public service members in
  `ModuleController` for exactly that reason — do not reorder those members.
* **`Module` is neither copyable nor movable;** the controller holds raw pointers.
* **`System::begin_routines_required()` calls `esp_log_level_set("*", ESP_LOG_NONE)`.** It is
  deliberate: the console is a user interface. If you silence or re-enable logging while
  debugging, put it back.
* **Never name an object `cli`.** The ESP32 Arduino core defines `cli` as a function-like macro.
  The core uses `xewe_cli`; keep it that way in every snippet.
* **`loop()` must not block** in any module, and prompts (`get_yn`, `get_string`) belong in setup
  routines only — they block until answered.

## Dependencies

XeWeUtils, XeWeSerial, XeWeNvs, XeWeCli, and ArduinoJson through XeWeNvs. Each is included only
through its entry header (`#include <XeWeNvs.h>`), and every one must be declared in
`library.properties` → `depends=`.

## When changing this library

* `src/` has exactly one top-level header, `src/XeWeOS.h`; everything else lives in a folder per
  class. Code is in `namespace xewe::os`.
* `library.json` is **generated** from `library.properties`
  (`python3 publish.py manifest` in
  [`publish-arduino-library`](https://github.com/xewe-labs/publish-arduino-library)). Never
  hand-edit it; `check` fails when it is stale.
* Versions are **lockstep** across all XeWe libraries. Never bump this one alone.
* Source files start with the SPDX header from
  [`.github/guidelines/license-header.txt`](https://github.com/xewe-labs/.github/blob/main/guidelines/license-header.txt).
  Markdown files do not.
* **Keep `extras/ModuleTemplate` in step.** It is what module authors copy; a new hook or changed
  signature has to appear there too.
* **Documentation is part of the change.** A new or changed public function, lifecycle step, NVS
  key or `$system` command updates its page in `doc/` in the same breath — this reference is
  written to be exhaustive, so a gap is a bug.
* Check your work without publishing anything:

  ```bash
  python3 publish-arduino-library/publish.py check xewe-library-os
  ```
