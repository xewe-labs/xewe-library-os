# AGENTS.md — xewe-os-core (XeWeCore)

Rules for coding agents working **anywhere in this repository**, not only in `doc/`.
Organization-wide rules are in
[`.github/AGENTS.md`](https://github.com/xewe-labs/.github/blob/main/AGENTS.md) and win where this
file is silent: never publish, never tag or push, never commit unasked, never flash a board.

XeWeCore 2.0.0 is the merge of XeWeUtils, XeWeSerial, XeWeCli, XeWeNvs and XeWeOS. Behaviour is
unchanged from those 1.0.0 libraries except that the default `OsConfig::url` printed in the boot
header now points to `https://github.com/xewe-labs/xewe-os-core`; otherwise only names, includes
and the layout moved. The rules below
are theirs, kept per component, with the ones the merge made obsolete rewritten and marked.

## Layout

* **`src/` has exactly one top-level header,** `src/XeWeCore.h`. Arduino puts every library's
  `src/` on the include path, so a second top-level header (especially `String.h`, `Serial.h`,
  `Timer.h`) collides with other libraries and with system headers.
* **Everything else lives in `src/XeWeCore/`**: `Utils.h` + `Utils/*.h`, `Serial`, `Cli`,
  `FlexData`, `Nvs` (+ `Nvs.tpp`), `Module`, `XeWeOs`. Files include each other with relative
  quotes (`#include "Serial.h"`, `#include "String.h"` inside `Utils/`).
* **Sketches include `<XeWeCore.h>`.** arduino-cli only discovers a library from its top-level
  headers: a sketch whose only include is `<XeWeCore/Serial.h>` fails with "No such file"
  (verified with arduino-cli 1.5.1). `XeWeCore/<Part>.h` may be included **after** the umbrella.
  Every example and doc snippet uses `<XeWeCore.h>`.
* **Include direction:** Utils ← Serial ← Cli, FlexData ← Nvs, all ← Module ← XeWeOs. Nothing
  below `Module.h` includes upward. `Module.h` forward-declares `class Os;` and must never include
  `XeWeOs.h` (that is a cycle); only the `.cpp` files do.
* **One namespace, `xewe`** (plus `xewe::str` and `xewe::color`). The only global symbols are
  `XeWeOs` (alias of `xewe::Os`) and the macros below.
* **Only dependency: ArduinoJson 7** (`depends=ArduinoJson (>=7.0.0)`). `architectures=esp32`.

## Utils (`src/XeWeCore/Utils/`)

* **Header-only, and it must stay that way.** There is no `.cpp` under `Utils/`.
* **`LockGuard` is included unconditionally** by `Utils.h` (esp32 only; every ESP32 core has
  FreeRTOS). The host tests provide a FreeRTOS stand-in in `extras/host/shim/freertos/`.
* **`AsyncTimer` is `xewe::AsyncTimer` since 2.0.0** (it was global in XeWeUtils 1.0.0). Do not
  move it back.
* **`lower` and `to_lower` are duplicates on purpose-by-accident.** Both are public and callers
  exist; remove one only as a deliberate, announced breaking change.
* **Debug flags default to `0`** behind `#ifndef DEBUG_<Class>`. Never enable one in the header;
  enable it from build flags.
* **Macros stay global and unrenamed:** `DBG_ENABLED`, `DBG_PRINTLN`, `DBG_PRINTF`,
  `DBG_PRINTF_BUFFER_SIZE`, `DEBUG_AsyncTimer`, `STRINGIFY_XEWE`, `TO_STRING`.
* **`xewe::validate` must stay exception-free.** A malformed string is a normal input here, not
  an error path — but it is handled by delegating to `xewe::str::parse_int` / `parse_float`, which
  report failure by returning `false`. Never reintroduce `std::stoll`/`std::stod` or `try`/`catch`;
  `extras/host/run.sh` builds with `-fno-exceptions`.

## Serial (`src/XeWeCore/Serial.{h,cpp}`)

* **The type is `xewe::SerialPort`, not `xewe::Serial`.** `Serial` is a macro in the ESP32 core
  (`#define Serial HWCDCSerial` / `USBSerial` / `Serial0`, by board menu), and `Serial.cpp` and the
  `DBG_*` macros call the Arduino `Serial` unqualified from inside `namespace xewe`. A class named
  `xewe::Serial` breaks both.
* **Must not include `XeWeOs.h`, `Nvs.h` or `Cli.h`.** It works standalone.
* **Prompt semantics are load-bearing.** `retry_count == 0` means infinite and `timeout_ms == 0`
  means no timeout. Modules across the organization call `get_yn()` and `get_string()` with no
  retry arguments and rely on the call not returning until it is answered. Changing either default
  silently changes every first-boot setup flow.
* **`get_core` sets `success_sink` on every exit path.** A new prompt type must go through it, or
  callers lose the only signal that distinguishes a default from an answer.
* **The 255-byte `INPUT_BUFFER_SIZE` is documented behaviour**, including the silent split of a
  longer line. Growing it is fine; changing `get_string`'s `max_length == 0` fallback, which
  resolves to `INPUT_BUFFER_SIZE - 1`, is a behaviour change to announce.
* **Table cells are `std::string_view`.** Do not add an overload that stores them, and do not
  "fix" a caller by passing a temporary — the lifetime rule is the API.
* **Output is CRLF** (`xewe::str::kCRLF`) everywhere. Terminals on the other end assume it.
* **`print` wraps only when `message_width > 0`.** Callers pass `0` constantly; making wrapping
  unconditional reflows every boot message in every firmware.
* There is no `println`. Use `print` (CRLF by default), `println_raw` or `printf`.

## Cli (`src/XeWeCore/Cli.{h,cpp}`)

* **Never write `cli(` or `cli (`.** The ESP32 Arduino core defines `cli()` as a function-like
  macro (`Arduino.h`), so `xewe::Cli cli(serial);` does not compile. *(Changed in 2.0.0:)* the
  `Os` member is named `cli` and is brace-initialised (`cli{serial}`); member access `os.cli.x()`
  is safe. A standalone `xewe::Cli` object in a sketch or snippet is conventionally named
  `xewe_cli` — the prefix exists only because `cli` cannot be used (the `cli(` macro), not because
  the name is reserved. What 2.0.0 removed is the old *member* `controller.xewe_cli` (now `os.cli`).
* **Must not include `XeWeOs.h`.** It works standalone.
* **The handler's `xewe::span` is a view into a local vector.** Do not change `execute` to hand out
  something that looks storable, and do not "fix" a handler by keeping the span — copy the
  strings.
* **The two `execute` overloads match differently on purpose:** the parsed path on name only, the
  direct path on name *and* `arg_count`. Unifying them changes which handler runs for existing
  firmware.
* **`$<group>`, `$<group> help` and `$help <group>` all print that group's help.** All three are
  used in device docs and example comments.
* **Error strings are part of the interface.** They are documented verbatim in
  [`cli/execution.md`](cli/execution.md) and users grep for them; change one and update the page in
  the same commit.
* **Argument count is checked before the handler runs.** Handlers index `args[0]` without
  bounds-checking because of that guarantee.
* **`add_group` on an existing id must keep its commands.** Modules rely on it when a group is
  touched twice.

## Nvs and FlexData (`src/XeWeCore/Nvs.{h,cpp,tpp}`, `FlexData.h`)

Everything here persists across reboots and survives a reflash. Take that seriously:

* **Never call `erase_all()` to "clean up" during a test.** It erases the entire NVS partition —
  every namespace, including Wi-Fi calibration data and anything other libraries stored. It exists
  for a deliberate factory reset.
* **Do not change a `FlexData` layout casually.** Adding, removing, reordering or retyping a `fld`
  entry invalidates every blob already on every device. There is no schema in the blob, so old
  bytes are read against the new field list and produce plausible garbage.
* **Version the schema with a field, not with `kBlobVersion`.** `kBlobVersion` is one constant on
  `xewe::FlexData` shared by every struct: shadowing it in a derived struct compiles but is
  silently ignored, and raising it in the library invalidates every stored struct on every device.
  Put a `schema` field first in `fields()` and check it after reading. See
  [`nvs/blob-format.md`](nvs/blob-format.md).
* **`from_blob` overwrites in place.** It is documented that a failed decode leaves the object
  partially written; do not add a "helpful" rollback without updating that page, and do not write
  caller code that trusts the object after a `false`.
* **The 15-character `MAX_KEY_LEN` applies to namespaces as well as keys.** A module's `id` is its
  namespace, so raising or lowering this changes what module ids are legal across the
  organization.
* **Only name validation reports errors.** That is deliberate, not an oversight: `read` returning
  a default for a missing key is the normal path, and reporting it would flood the console on
  every boot. Do not add error reporting to the open/set/commit paths without a decision.
* **`float`/`double` are stored as blobs.** Changing them to a different encoding makes every
  stored float on every device unreadable.
* `std::string_view` and `const char*` are deliberately write-only — there is nowhere to return a
  view into.
* **Arduino `String` is not a supported `FlexData` field type.** The `static_assert` is the
  intended behaviour; do not add an overload without considering the blob layout.
* **The `ESP_LOGE` tag stays `"XeWeNvs"`** so existing log filters keep working.
* **`Nvs` keeps `std::span`, not `xewe::span`**, as in 1.0.0. Nvs must not include `Utils.h` or
  `Serial.h`; it reports errors through `set_error_handler`.
* **`FlexData.h` is platform-neutral** — `<Arduino.h>`, `<ArduinoJson.h>` and the standard
  library, with no `esp_*` and no `nvs.h`. Keep it that way.
* Template definitions live in `Nvs.tpp`, included at the bottom of `Nvs.h`.

## Os, Module and System (`src/XeWeCore/Module.{h,cpp}`, `XeWeOs.{h,cpp}`)

The **core only**. It knows no concrete modules, and it must stay that way.

* **Never add a feature module here.** Wi-Fi, buttons, scheduling, a web interface — those live
  in the `xewe-os-modules` repository under `modules/<slug>/`, or are firmware-local modules in
  `src/<Name>/`.
* The only module that belongs here is `System`, because `Os` owns it (public member
  `os.system`, constructed last, registered first).
* **`disable()` calls `reset()`**, which wipes the module's NVS namespace, **and cascades to every
  dependent module.** When `verbose` is false there is no confirmation. Do not call it to "reset
  state" in a test.
* **`System::reset()` erases the entire NVS partition** via `nvs.erase_all()` — every namespace on
  the device, including `root/init_setup_flag`, so the next boot re-runs initial setup. It is a
  factory reset.
* `System::reset`'s `disable_confirmed` starts `false` on purpose, so a programmatic call always
  aborts. Do not "fix" that to `true`.
* **A module `id` is the CLI group *and* the NVS namespace**, so it is capped at 15 characters.
  Changing how the id is used changes where every device's stored data lives.
* **The NVS keys `is_enabled`, `not_first_boot`, `init_complete`, `root/init_setup_flag` and
  `system/device_name` are the on-device state format.** Renaming one strands the state on every
  deployed device. Devices flashed with XeWeOS 1.0.0 keep their data under 2.0.0.
* **The lifecycle order in `Module::begin()` is contractual** and documented step by step in
  [`os/module.md`](os/module.md). Modules across the organization rely on
  `begin_routines_required` running before init, and `begin_routines_common` running last.
* **Modules register from their constructors**, so declaration order in the sketch is begin and
  loop order. The private `modules` vector is declared before the public service members in
  `Os` for exactly that reason — do not reorder those members.
* **`Module` is neither copyable nor movable;** `Os` holds raw pointers.
* **The protected member is `os` (`xewe::Os&`)** *(changed in 2.0.0, was `controller`)*. A derived
  constructor names its `xewe::Os&` parameter **`host`** (never `os`, which would hide the member),
  passes it to `Module(host, ...)`, and command handlers capture `[this]` only and use `os.` inside.
  `[&]` would bind a shadowing parameter silently. See `extras/ModuleTemplate` and the modules
  repository's `CONTRACT.md`.
* **`System::begin_routines_required()` calls `esp_log_level_set("*", ESP_LOG_NONE)`.** It is
  deliberate: the console is a user interface. If you silence or re-enable logging while
  debugging, put it back.
* **`loop()` must not block** in any module, and prompts (`get_yn`, `get_string`) belong in setup
  routines only — they block until answered.
* **Keep `extras/ModuleTemplate` in step.** It is what module authors copy; a new hook or changed
  signature has to appear there too.

## When changing this library

* Source files start with the SPDX header from
  [`.github/guidelines/license-header.txt`](https://github.com/xewe-labs/.github/blob/main/guidelines/license-header.txt);
  the third line is the path, `// xewe-os-core/src/XeWeCore/<File>`. Markdown files do not.
* **Documentation is part of the change.** A new or changed public function, lifecycle step, NVS
  key or `$system` command updates its page in `doc/` in the same breath — this reference is
  written to be exhaustive, so a gap is a bug.
* Check your work without publishing anything:

  ```bash
  extras/host/run.sh                     # host unit tests: Utils, Serial, Cli
  # board builds: compile every example for esp32c3 / esp32c6 / esp32s3
  ```
