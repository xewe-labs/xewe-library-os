# RULES.md — root rules for agents in this project

Every rule below is absolute. It applies without exception, in every project that carries this
folder, and is never changed at install time or by an agent. Rules are cited by ID (`R-01` …
`R-15`) and are never renumbered. User-level rules live in `PREFERENCES.md` (R-15). If two
instructions conflict, R-03 decides.

## 1. Entry and Read Order

- **R-01 Read order.** On opening the project, read completely and in this order: `AGENTS.md`,
  `RULES.md`, `PREFERENCES.md`, `handoffs/HANDOFF.md` (through R-04), then the frontmatter of
  every `skills/*/SKILL.md`. Do no project work before all of these have been read.
- **R-02 Pickup before work.** The first action after reading is the `pickup` procedure of the
  `wax_handoff` skill. Restate the previous exit point to the human before touching anything
  else.
- **R-03 Precedence.** An explicit human instruction given in the current session outranks
  `RULES.md`, which outranks `PREFERENCES.md`, which outranks `AGENTS.md`, which outranks any
  individual skill. A preference never overrides a rule.
  Every human-instructed deviation is recorded in that session's handoff under "Design
  decisions", quoting the instruction.

## 2. Handoffs

- **R-04 Access only through `wax_handoff`.** Nothing under `handoffs/` is read, created,
  edited, moved, or deleted except by executing the `pickup` or `handoff` procedure of the
  `wax_handoff` skill. The exceptions belong to `wax_init`: filling the project name into the
  shipped genesis entry of a brand-new copy, and, during an upgrade, adding head keys that the
  current format requires to an existing `HANDOFF.md`. It never creates, edits, or removes an
  entry.
- **R-05 HEAD moves with the directory.** Writing an entry under `handoffs/handoffs/` and
  updating `handoffs/HANDOFF.md` are one operation. Never do one without the other.
- **R-06 Every session ends with a handoff.** A session that produced changes and no handoff
  is incomplete; say so to the human.
- **R-07 The template is copied, never filled in place.** `handoffs/handoffs/yyyy-mm-dd-hh-mm-ss.md`
  keeps its literal name and its placeholder content permanently.
- **R-08 Naming.** Entry filenames are UTC timestamps in the form `yyyy-mm-dd-hh-mm-ss.md`.
  HEAD is always the lexically greatest entry filename. Never backdate an entry.
- **R-09 Stop on inconsistency.** If `handoffs/HANDOFF.md` disagrees with the directory
  (HEAD, count, index, or a missing file), do not repair, do not write, do not guess. Report
  the exact mismatch to the human and wait.

## 3. Skills

- **R-10 Discover skills from their frontmatter.** A skill is a folder `skills/<name>/` that
  holds a `SKILL.md`. Skills are found by reading the `name` and `description` frontmatter of
  every `skills/*/SKILL.md`, the same way the host tool finds them. Nothing else indexes them.
- **R-11 Valid or nonexistent.** A folder under `skills/` whose `SKILL.md` is missing or fails
  R-12 is not a skill and must not be used. Adding or removing a skill is one change: the
  whole folder, with a valid `SKILL.md`.
- **R-12 Skill shape.** A skill is a folder containing `SKILL.md` whose YAML frontmatter has
  exactly two keys, `name` and `description`, and whose `name` equals the folder name.
  Supporting files sit flat beside `SKILL.md` or under `references/`, `scripts/`, `assets/`,
  or `evals/`.

## 4. Structure of `.agents/`

- **R-13 Fixed top level.** `.agents/` contains exactly `AGENTS.md`, `RULES.md`,
  `PREFERENCES.md`, `handoffs/`, and `skills/`, plus only the items listed in P-11. Never add,
  rename, or remove a top-level item.
- **R-14 Uppercase names are fixed.** `AGENTS.md`, `RULES.md`, `PREFERENCES.md`, `HANDOFF.md`,
  and every `SKILL.md` keep their names and locations.
- **R-15 Rules are root, preferences are user-level.** `RULES.md` is never edited by an agent
  or at install time. `PREFERENCES.md` ships with defaults; they are changed only during the
  `setup` procedure of `wax_init`, or later on an explicit human instruction quoted in that
  session's handoff. Preferences are cited by ID (`P-01` …) and bind exactly like rules until
  changed.

## 5. Project rules (xewe-os-core)

Added for this project on the owner's instruction; they bind like the rules above and are cited
as `X-NN`. They are not part of the WAX reference.

- **X-01 One top-level header.** `src/XeWeCore.h` is the only header directly under `src/`;
  everything else lives in `src/XeWeCore/`. Every sketch, example and doc snippet includes
  `<XeWeCore.h>` first (arduino-cli discovers the library only from its top-level header).
- **X-02 Include direction.** Nothing below `Module.h` includes upward; `Module.h` never includes
  `XeWeOs.h`. `Serial` includes none of `XeWeOs.h`, `Nvs.h`, `Cli.h`; `Cli` does not include
  `XeWeOs.h`. `Nvs` never includes `Utils.h` or `Serial.h`. `FlexData.h` has no `esp_*` or
  `nvs.h`. `Utils/` has no `.cpp`. `Color.h`, `String.h`, `Pins.h`, `Listeners.h` include no
  `<Arduino.h>`.
- **X-03 Forbidden patterns.** `cli(` or `cli (` anywhere (core macro); a class named
  `xewe::Serial`; `try`/`catch`, `throw`, `std::stoll`/`std::stod` (the host build uses
  `-fno-exceptions`); a `std::function` global where a function pointer does; a `DEBUG_<Class>`
  flag enabled in a header; a module constructor parameter named `os` (name it `host`); `[&]` in
  a module's command handler (capture `[this]`); blocking in any `loop()`; a prompt outside a
  setup routine; a feature module (Wi-Fi, buttons, web, …) in this repository.
- **X-04 C++17 floor.** No `std::span` and no other C++20 library feature in new code; use
  `xewe::span`. The one exception is `Nvs::write_blob`, which keeps its `std::span` signature.
- **X-05 Fifteen characters, stable keys.** Module ids, NVS namespaces, NVS keys and settings-table
  keys are 1–15 characters. Never rename or retype a stored key (`is_enabled`, `not_first_boot`,
  `init_complete`, `root/init_setup_flag`, `system/device_name`, any table key), change
  `MAX_KEY_LEN`, the float encoding, a `FlexData` field list or `kBlobVersion`, or the `ESP_LOGE`
  tag `"XeWeNvs"`: devices in the field lose their data.
- **X-06 Interface behaviour is announced, not tuned.** CLI error strings, prompt defaults,
  the bounded confirmations (2 × 15 s), the 254-character line, the 4-line queue and whole-line
  drop, CRLF output, wrap-only-when-width, the two `execute` matching rules, `hsv_to_rgb`
  arithmetic and the strict `parse_hex_color` change only together with their doc page, as a
  deliberate, announced change.
- **X-07 Destructive calls stay out of tests.** No `erase_all()`, `disable()` or `System::reset()`
  to clean up; board-test namespaces start with `xt`; `test_recovery_nvs_wipe.py`
  (`XEWE_HWTEST_DESTRUCTIVE=1`) runs only on request.
- **X-08 Credentials.** Never open, print or copy a dotenv or key file; the tools read them.
- **X-09 Test hooks cost nothing.** Everything hook-related, including the call site in
  `XeWeOs.cpp`, stays inside `#ifdef XEWE_TESTING`; a build without it has an empty
  `Testing.cpp.o` and no `$test` string. No crash, abort or watchdog hooks.
- **X-10 Test layout.** Host tests: `tests/unit/test/test_*.cpp` (ArduinoJson-dependent in
  `test/json/`), compile probes `tests/unit/probe/*.probe.cpp`, stand-ins `tests/unit/shim/`, all
  driven by `tests/unit/run.sh`. Board tests: `tests/board/test_*.py`, run through the
  `xewe-os-tools` plugin. A parser, tokenizer or FlexData case lives in both tables
  (`test_parsers.cpp` ↔ `test_hooks_utils.py`, `test_tokenizer.cpp` ↔ `test_hooks_cli_tokenizer.py`,
  `test_flexdata.cpp` ↔ `test_hooks_flexdata.py`).
- **X-11 Comments describe the code.** A comment says what the code does or why (an invariant, a
  limit, a hardware fact). No process in code: no agent or session names, dates, decision or
  finding ids, "was …" history, review or report references. History lives in the xewe-labs
  `docs/`.
- **X-12 File headers.** Every source file starts with the SPDX lines from the organization's
  `guidelines/license-header.txt`; the third line is the path, `// xewe-os-core/<path>`. Markdown
  files have none.
- **X-13 Docs are part of the change.** A public function, lifecycle step, command, prompt, NVS key
  or build define that is added or changed updates its page in `doc/` in the same change. `doc/`
  describes current behaviour only: no history, no changelog, no version tags.
- **X-14 Release through the publish tool.** Versions and releases go through
  `xewe-os-publish-library` (`publish.py check`, `publish.py bump <version>`,
  `publish.py release`), only when the human asks. Version strings in `library.properties`, `library.json` and `XEWE_CORE_VERSION*` move
  together.
- **X-15 Examples teach.** `examples/02_MyModule` shows every module hook and the current
  signatures; `01_Hello` and `02_MyModule` must never include `<XeWeBuildInfo.h>` (a plain IDE
  build has none).
