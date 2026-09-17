# Module

`src/Module/Module.h` — the base class for everything that plugs into XeWe OS.

A module registers itself with the controller on construction, stores its own settings, and
overrides only the hooks it needs. Start from
[`extras/ModuleTemplate`](../extras/ModuleTemplate) (every hook, commented) or
[`examples/02_CustomModule/BlinkModule.h`](../examples/02_CustomModule/BlinkModule.h) (a complete one).

## Constructor

```cpp
Module(ModuleController& controller,
       std::string       id,
       std::string       name,
       std::string       description,
       bool              requires_init_setup,
       bool              can_be_disabled,
       bool              has_cli_commands);
```

| Parameter | |
|---|---|
| `id` | lowercase slug. It is the **CLI group** (`$<id>`) *and* the **NVS namespace**, so it must be **15 characters or fewer** |
| `name` | display name, used in headers and help titles |
| `description` | shown when the module asks to be enabled on first boot |
| `requires_init_setup` | the module has a one-time setup that must complete once |
| `can_be_disabled` | the user may turn it off; adds `$<id> enable` / `$<id> disable` |
| `has_cli_commands` | create the `$<id>` group and register the generic commands |

The constructor sets `enabled = true`, calls `controller.register_module(*this)`, and — when
`has_cli_commands` — creates the CLI group and registers the generic commands below.

```cpp
class BlinkModule : public xewe::os::Module {
public:
    BlinkModule(xewe::os::ModuleController& os, BlinkConfig config = {})
        : Module(os, "blink", "Blink", "Blinks an LED",
                 /* requires_init_setup */ true,
                 /* can_be_disabled     */ true,
                 /* has_cli_commands    */ true)
        , config(config) {
        register_command({"period", "Set period", "$blink period 250", 1,
                          [this](std::span<const std::string> args) { /* ... */ }});
    }
};
```

**The `id` is capped at 15 characters** because it is used as an NVS namespace; a longer one is
rejected by XeWeNvs and the module silently fails to persist anything.

## Copy and move

```cpp
virtual ~Module() noexcept = default;
Module(const Module&) = delete;  Module& operator=(const Module&) = delete;
Module(Module&&)      = delete;  Module& operator=(Module&&)      = delete;
```

Neither copyable nor movable — the controller holds raw pointers to modules. Modules are globals
or members, never vector elements.

## Lifecycle

```cpp
void         begin                  ();     // non-virtual; called by ModuleController::begin()
virtual void begin_routines_required();
virtual void begin_routines_init    ();
virtual void begin_routines_regular ();
virtual void begin_routines_common  ();
```

`begin()` is not virtual and runs this sequence:

1. Reads `not_first_boot` and `is_enabled` from NVS. On the very first boot the module starts
   enabled.
2. Prints a `<name> Setup` header — **only when `can_be_disabled` or `requires_init_setup`**. A
   module that is neither prints no header.
3. Checks requirements. If any required module is disabled: prints `<name> module requires:` and
   one `<req name>, use: $<req id> enable` line per missing requirement, forces this module
   disabled, persists that, and **returns** — none of the four routines run.
   Otherwise, if `can_be_disabled` is `false`, it force-enables and persists, so **a mandatory
   module comes back automatically** once its requirements return.
4. If the module is disabled, prints why and returns.
5. **First boot only:** a module with `can_be_disabled` prints
   `Would you like to enable <name> module?` followed by its description and asks `get_yn()`. A
   "no" persists the choice and returns. Either way `not_first_boot` is persisted.
6. `begin_routines_required()` — every boot the module is enabled.
7. If init setup is not yet complete: `begin_routines_init()`, then persist `init_complete`
   **only if the module is still enabled** (the routine may have disabled itself). Otherwise:
   `begin_routines_regular()`.
8. `begin_routines_common()` — always last.

All four hooks default to doing nothing. Override only what you need.

## loop

```cpp
virtual void loop();
```

Called every iteration while the module is enabled. **It must not block** — every module shares
one loop. No `delay()`, no blocking prompts.

## add_requirement

```cpp
void add_requirement(Module& other);
```

Declares that this module needs another. Take the dependency by reference in the constructor,
store it, and register it:

```cpp
WebInterface(xewe::os::ModuleController& os, Wifi& wifi)
    : Module(os, "web", "Web Interface", "...", false, true, true), wifi(wifi) {
    add_requirement(wifi);
}
```

Declare `wifi` before `web_interface` in the sketch.

A self-reference is ignored. The call records the edge **in both directions**: `other` becomes a
requirement of this module, and this module becomes a dependent of `other`. That back-edge is what
makes `disable()` cascade. Neither list is readable from outside.

## enable, disable, reset

```cpp
virtual void enable (const bool verbose = false, const bool do_restart = true);
virtual void disable(const bool verbose = false, const bool do_restart = true);
virtual void reset  (const bool verbose = false, const bool do_restart = true,
                     const bool keep_enabled = true);
```

**`do_restart` defaults to `true`** — all three reboot the device unless told not to.

**`enable`** does nothing if already enabled, and refuses (printing the requirement list) if any
requirement is disabled. Otherwise it persists `is_enabled` and restarts.

**`disable`** does nothing if already disabled, and refuses with `<name> module can't be disabled`
when `can_be_disabled` is `false`. Then:

* When `verbose`, it prints a `[WARNING] / Disabling <name> / Will reset it` header — listing the
  dependents that will go with it — and asks `OK?`. A "no" prints `Aborted` and returns.
  **When `verbose` is `false` there is no confirmation at all.**
* It cascades `disable(false, false)` to **every dependent module**, without prompting them.
* It ends by calling `reset(verbose, do_restart, /*keep_enabled=*/false)`.

**Disabling a module wipes its NVS namespace, and its dependents', without asking unless
`verbose`.** This is the sharpest edge in the library.

**`reset`** erases the module's NVS namespace, re-writes `not_first_boot = true`, recomputes
`enabled` as `(!can_be_disabled || keep_enabled) && requirements_enabled()`, persists
`is_enabled` when the result is enabled, and restarts when asked.

Because `not_first_boot` is rewritten, a reset module **does not re-ask the first-boot enable
question** — but `init_complete` is gone, so `begin_routines_init()` does run again.

## Status and getters

```cpp
virtual std::string status             (const bool verbose = false) const;
bool                is_enabled         (const bool verbose = false) const;
bool                is_disabled        (const bool verbose = false) const;
bool                init_setup_complete(const bool verbose = false) const;
bool                has_cli_cmds       ()                           const;

std::string_view    get_id             ()                           const;
std::string_view    get_name           ()                           const;
std::string_view    get_description    ()                           const;
```

| | |
|---|---|
| `status` | returns `"<name> module enabled\|disabled"`, read **from NVS**, not the in-RAM flag; prints it when `verbose`. Override to add your own state |
| `is_enabled` | prints `<name> module enabled` **only when the answer is true** |
| `is_disabled` | prints only when true, and distinguishes missing requirements (listing them) from a user-disabled module (`to enable: $<id> enable`) |
| `init_setup_complete` | `!requires_init_setup \|\| nvs.read<bool>(id, "init_complete")`. **Its `verbose` parameter is accepted but never used** |
| `has_cli_cmds` | the constructor flag |

The verbose flags print asymmetrically on purpose: each is meant for the branch that needs
explaining.

## Protected helpers

```cpp
bool register_command    (Command command);
bool requirements_enabled(const bool verbose = false) const;
void run_with_dots       (const std::function<void()>& work,
                          uint32_t duration_ms     = 1000,
                          uint32_t dot_interval_ms = 200);
```

**`register_command`** adds a command to this module's `$<id>` group. Returns `false` immediately
when the module was constructed with `has_cli_commands == false` — a silent no-op otherwise easy
to miss. The `Command` aggregate is
[XeWeCli's](https://github.com/xewe-labs/xewe-library-cli/blob/main/doc/commands.md):
`{name, description, sample_usage, arg_count, function}`.

**`requirements_enabled`** reports whether every required module is enabled. With `verbose` it
walks all of them and prints the missing ones; without, it short-circuits on the first.

**`run_with_dots`** runs `work` repeatedly for `duration_ms` while printing a `.` every
`dot_interval_ms`, then a newline. It is a blocking progress indicator for a setup routine — never
call it from `loop()`. A `dot_interval_ms` of `0` is clamped to 1. It does not stop early: `work`
is called as fast as possible for the full duration.

## Generic CLI commands

With `has_cli_commands`, every module gets these for free:

| Command | Args | Description | Effect |
|---|---|---|---|
| `$<id> status` | 0 | Get module status | `status(true)` |
| `$<id> reset` | 0 | Reset the module | `reset(true, true)` — **wipes the namespace and reboots with no confirmation** |
| `$<id> enable` | 0 | Enable this module | `enable(true, true)`; only when `can_be_disabled` |
| `$<id> disable` | 0 | Disable this module | `disable(true, true)`; only when `can_be_disabled` |

`$<id>` on its own, and `$help <id>`, print the group's command table.

`$<id> reset` prompts for nothing — unlike `$system reset`, which
[overrides `reset`](system.md#reset) to add a confirmation.

## NVS keys

Each module stores its state in the NVS namespace named after its `id`:

| Key | Type | |
|---|---|---|
| `is_enabled` | `bool` | persisted enable state; what `status()` reads |
| `not_first_boot` | `bool` | set once the first-boot questions have been asked |
| `init_complete` | `bool` | set after `begin_routines_init()` completes, when `requires_init_setup` |

Your own keys go in the same namespace, which is why `$<id> reset` wipes exactly this module's
data. `System` additionally stores `device_name`; the controller uses the separate `root`
namespace for `init_setup_flag`.

## Rules of thumb

* **The framework knows no concrete modules.** A firmware project keeps its own in
  `src/<Name>/<Name>.{h,cpp}` and assembles them in the `.ino`; reusable ones become libraries.
* **Dependencies are constructor references** plus `add_requirement`, not lookups.
* **Use the core services through the controller:** `controller.serial`, `controller.nvs` with
  `id` as the namespace, `controller.xewe_cli.execute(line)`, `controller.system`.
* **`loop()` must not block.**
* **A public function of a disableable module starts with `if (is_disabled()) return;`** — the
  module stays registered when disabled and others may still call it.
* **Never name an object `cli`;** the Arduino core defines it as a macro. The core uses
  `xewe_cli`.
