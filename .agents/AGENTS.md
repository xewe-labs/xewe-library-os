# AGENTS.md — xewe-os-core

This is **WAX 1.3**, the WAX Agentic Workspace. The rules, preferences, and skills below belong
to this version. Human documentation lives at https://github.com/maxdokukin/wax_agents.

This folder is the agentic part of the project. It gives you boundaries, context, and tools,
in that order. Read it as described below before doing anything else in the repository.

## Read in this order

1. **`RULES.md` — root boundaries.** Absolute rules, the same in every project. Cite them by
   ID when you explain a decision or a refusal.
2. **`PREFERENCES.md` — user boundaries.** This project's chosen defaults (R-15). They bind
   like rules until a human changes them; cite them by ID too.
3. **`handoffs/HANDOFF.md` — context.** The head of the work record: where the last session
   stopped and what is open. Reach it only through the `pickup` procedure of the
   `wax_handoff` skill (R-04), because the directory has invariants that the skill checks
   before you rely on anything in it.
4. **`skills/` — tools.** One folder per skill, each with a `SKILL.md`. Discover them by
   reading the frontmatter of every `skills/*/SKILL.md` (R-10); a folder without a valid one
   is not a skill (R-11).

## Session shape

- **Pickup → work → handoff.** Start with `wax_handoff` pickup, do the work, end with
  `wax_handoff` handoff. A session that changed anything and did not end with a handoff is
  incomplete (R-06); say so rather than letting it pass.

## Never do these without being asked

- **Edit `RULES.md`, `PREFERENCES.md`, or this file** (R-15, P-05). Propose changes in the
  handoff instead (P-10).
- **Touch anything under `handoffs/` by hand** (R-04). The skill is the only door.
- **Add, rename, or remove a top-level item in `.agents/`** (R-13).
- **Commit, push, tag, release, publish, or delete what you did not create** (P-09).

## Precedence

- **Human instruction in this session > `RULES.md` > `PREFERENCES.md` > this file > a
  skill** (R-03). A project's own `.agents/` wins over any organization-level agent file. Record every
  human-instructed deviation in the handoff, quoting the instruction.

## Reporting back

- **Say what you actually ran** and label anything unverified as unverified (P-07).
- **Never report a skipped or failed step as done** (P-08). Failures come with their output.

## Project: xewe-os-core (XeWeCore)

The Arduino library XeWeCore: a serial console, a `$group command` CLI, typed NVS storage,
helpers and a module framework for ESP32. Human documentation: `README.md` and `doc/` (start at
`doc/README.md` and `doc/concepts.md`). Project rules are X-01 … X-15 at the end of `RULES.md`.
Organization rules: `https://github.com/xewe-labs/.github/blob/main/AGENTS.md`; they apply where
this file is silent.

### Layout

- `src/XeWeCore.h` is the only top-level header; everything else is in `src/XeWeCore/`:
  `Utils.h` + `Utils/*.h` (header-only), `Serial`, `Cli`, `FlexData.h`, `Nvs` (+ `Nvs.tpp`),
  `Settings`, `Module`, `XeWeOs`, `Testing`. Files include each other with relative quotes.
- Include direction: Utils ← Serial ← Cli; FlexData ← Nvs; Serial ← Settings (Nvs only
  forward-declared); all ← Module ← XeWeOs. `Module.h` forward-declares `class Os;`; only `.cpp`
  files include `XeWeOs.h`.
- One namespace `xewe` (plus `xewe::str`, `xewe::color`, `xewe::pins`, `xewe::detail`,
  `xewe::testing`). Global symbols: `XeWeOs`, the `DBG_*`/`DEBUG_*`/`STRINGIFY_XEWE`/`TO_STRING`
  macros and `XEWE_CORE_VERSION` (+ `_MAJOR/_MINOR/_PATCH`, equal to `library.properties`).
- `examples/01_Hello`, `02_MyModule` teach levels 1 and 2; `11_Utils` … `15_Os` are reference
  demos. `tests/unit` runs on the host, `tests/board` on a board. `doc/` is the human reference.

### Check your work

```bash
tests/unit/run.sh        # host tests: c++17 (94 tests) and gnu++2b (118 tests), -fno-exceptions; expect "0 failed checks"
```

- **Host tests** build the probes and `tests/unit/test/*.cpp` against the shims in
  `tests/unit/shim/` (Arduino, FreeRTOS, NVS in memory, ESP log). FlexData and Settings tests need
  ArduinoJson: `ARDUINOJSON_SRC=<path to ArduinoJson/src>`, else the shared toolchain copy.
- **Examples** compile for esp32c3, esp32c6 and esp32s3. With plain arduino-cli:
  `arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc examples/01_Hello`. With the tools:
  `./setup.sh` once, then `./run.sh --examples --chip <c3|c6|s3>` or `--all-chips`.
- **Lint:** `arduino-lint --compliance strict --library-manager update .` Run it from a clone
  whose folder name is a valid library name; in a working copy only LS003 (folder name) may fail.
- **Board tests** (`tests/board`) run only through the `xewe-os-tools` pytest plugin from an xewe
  project with a board attached, never on their own: `doc/tests.md` has the command.
  Take the board lock (`flock <project>/.board.lock`). Never flash a board unasked.
- **Test hooks:** `XEWE_TESTING` compiles in the `$test` group (`src/XeWeCore/Testing.{h,cpp}`)
  that the `test_hooks_*` files drive. Build it with `xewe build --chip s3 --define XEWE_TESTING=1`
  then `xewe flash --no-build`, or `XEWE_HWTEST_DEFINES=XEWE_TESTING=1` for the pytest run, or
  arduino-cli `--build-property compiler.cpp.extra_flags=-DXEWE_TESTING=1`. Without the define a
  build gets 0 bytes from it (X-09).

### Adding or changing a core feature

One change carries all four:

1. **Code** in `src/XeWeCore/`, inside the layout and include direction above.
2. **A host test** in `tests/unit/test/` (and the board table too when the case also runs through
   `$test`, see X-10).
3. **The doc page** in `doc/` for every public function, lifecycle step, command, prompt, NVS key
   or build define it adds or changes (X-13).
4. **An example**, when users see it: keep `examples/02_MyModule` in step with every new hook or
   changed signature; it is what module authors copy.

Then run the host tests, compile the examples and lint. A version bump changes
`library.properties`, `library.json` and `XEWE_CORE_VERSION*` together; releases go through the
publish tool (X-14).

### Component notes

- **Utils.** Header-only. `Color.h`, `String.h`, `Pins.h`, `Listeners.h` are host-includable
  (standard library only; `Pins.h` may include `<sdkconfig.h>` behind `__has_include`): code in
  other repositories includes them in host tests, and `run.sh` compiles each without the shim.
  `color::hsv_to_rgb` is pinned bit for bit (`color_hsv_to_rgb_pinned`). `str::parse_hex_color`
  accepts exactly `rrggbb` / `#rrggbb` and leaves outputs untouched on failure; `to_hex_color`
  writes uppercase `#RRGGBB`. `xewe::pins` is a fixed table; `claim` refuses a pin held by another
  owner or out of range and only warns on a strapping pin; strapping lists cite datasheets in
  `doc/utils/pins.md`. `lower` and `to_lower` are both public with callers; keep both.
  `xewe::validate` delegates to `str::parse_int` / `parse_float`.
- **Serial.** The type is `xewe::SerialPort` because `Serial` is a core macro. Prompts:
  `retry_count == 0` means infinite, `timeout_ms == 0` means no timeout; modules rely on those
  defaults. The core's own confirmations (`Module::disable`, `System::reset`) are bounded:
  `get_yn("OK?", 2, 15000, false, answered)`. `get_core` sets `success_sink` on every exit path;
  a new prompt type goes through it. Lines: 255-byte buffer (254 usable), 4-line queue, an
  over-long line dropped whole with one notice. Output is CRLF; `print` wraps only when
  `message_width > 0`. There is no `println`: use `print` or `printf`. Table cells are
  `std::string_view`; callers keep them alive.
- **Cli.** Handlers get a `xewe::span` into a local vector: copy what you keep. The parsed
  `execute` matches on name only, the direct one on name and `arg_count`. `$<group>`,
  `$<group> help` and `$help <group>` all print the group. The argument count is checked before a
  handler runs. `add_group` on an existing id keeps its commands; module ids are validated earlier
  by `Os::register_module` through `Cli::name_error`.
- **Nvs and FlexData.** Everything here persists across reflashes. Version a struct with a
  `schema` field, not `kBlobVersion` (one shared constant). `from_blob` overwrites in place.
  Read misses are silent; name errors, init failures (once), the automatic partition erase and
  open/set/commit failures are reported verbatim as in `doc/nvs/nvs.md`. FlexData JSON assignment
  is type-matched (`is<M>()` before `as<M>()`); field presence is a `uint32_t` set only by the
  JSON load, never stored, at most 32 fields. `FlexData.h` stays platform-neutral; Nvs reports
  through `set_error_handler` and does not include `Utils.h` or `Serial.h`.
- **Os, Module, System.** `System` is the only module here. `disable()` resets (wipes the
  namespace) and cascades to dependents; with `verbose == false` there is no confirmation.
  `System::reset()` erases the whole partition. `XEWE_DEVICE_NAME` must be a build flag (or come
  from `<XeWeBuildInfo.h>`); a sketch `#define` never reaches `XeWeOs.cpp`. The lifecycle order in
  `Module::begin()` is contractual (`doc/os/module.md`). The private `modules` vector is declared
  before the service members in `Os`; do not reorder them. `Module` is neither copyable nor
  movable. `System::begin_routines_required()` silences the ESP log on purpose. Settings tables:
  the engine is reached only through `settings_engine`, so a firmware without a table does not
  link it; never call `detail::settings_*` or `register_settings_commands` from code that always
  links. Secrets are never printed. A schema line format change raises `"schema":1` in
  `Settings::header` and updates `doc/os/settings.md`. `ListenerSet::notify` re-reads each slot,
  so `remove` during `notify` is safe.
