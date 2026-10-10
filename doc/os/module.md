# Module

`src/XeWeCore/Module.h`. The base class for everything that plugs into XeWe OS.

A module registers itself with the Os on construction, stores its own settings and overrides
only the hooks it needs. Start from [`examples/02_MyModule`](../../examples/02_MyModule) (a
settings table, a command, a listener) or
[`examples/15_Os/SensorModule.h`](../../examples/15_Os/SensorModule.h) (a module another one
requires).

## Constructor

```cpp
Module(Os& os,
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

The constructor sets `enabled = true` and calls `os.register_module(*this)`. With
`has_cli_commands`, it then creates the CLI group and registers the [generic commands](#generic-cli-commands).

```cpp
class BlinkModule : public xewe::Module {
public:
    BlinkModule(xewe::Os& host, BlinkConfig config = {})
        : Module(host, "blink", "Blink", "Blinks an LED",
                 /* requires_init_setup */ true,
                 /* can_be_disabled     */ true,
                 /* has_cli_commands    */ true)
        , config(config) {
        register_command({"period", "Set period", "$blink period 250", 1,
                          [this](xewe::span<const std::string> args) { /* ... */ }});
    }
};
```

**The `id` is checked at registration** ([`Os::register_module`](os.md#register_module) lists the
rules). A rejected module never begins or loops, gets no CLI group, and its `register_command`
returns `false`. `Os::begin()` prints the error, because modules are constructed before Serial is
up:

```text
! Module id 'averyverylongmodule' is longer than 15 characters (NVS namespace limit): module not registered
```

## Copy and move

```cpp
virtual ~Module() noexcept = default;
Module(const Module&) = delete;  Module& operator=(const Module&) = delete;
Module(Module&&)      = delete;  Module& operator=(Module&&)      = delete;
```

Neither copyable nor movable: the Os holds raw pointers to modules. Modules are globals
or members, never vector elements.

## Lifecycle

```cpp
void         begin                  ();     // non-virtual; called by Os::begin()
virtual void begin_routines_required();
virtual void begin_routines_init    ();
virtual void begin_routines_regular ();
virtual void begin_routines_common  ();
```

`begin()` is not virtual and runs this sequence:

0. When the module declares a [settings table](settings.md): loads it (table defaults,
   then NVS) and registers `$<id> set`, `get` and `schema`. This runs for a disabled module too.
   Without a table this step does nothing.
1. Reads `not_first_boot` and `is_enabled` from NVS. On the very first boot the module starts
   enabled.
2. Prints a `<name> Setup` header, **only when `can_be_disabled` or `requires_init_setup`**. A
   module that is neither prints no header.
3. Checks requirements. If any required module is disabled, it prints `<name> module requires:`
   and one `<req name>, use: $<req id> enable` line per missing requirement. It then disables this
   module, persists `is_enabled = false` and `not_first_boot = true`, and **returns**: none of the
   four routines run. Otherwise, a module with `can_be_disabled == false` is enabled and persisted,
   so **a mandatory module comes back by itself** once its requirements return.
4. If the module is disabled, prints why and returns.
5. **First boot only:** a module with `can_be_disabled` prints
   `Would you like to enable <name> module?` followed by its description and asks `get_yn()`. A
   "no" persists the choice and returns. Either way `not_first_boot` is persisted.
6. `begin_routines_required()`, on every boot the module is enabled.
7. If init setup is not complete: `begin_routines_init()`, then `init_complete` is persisted
   **only if the module is still enabled** (the routine may have disabled it). Otherwise:
   `begin_routines_regular()`.
8. `begin_routines_common()`, always last.

All four hooks default to doing nothing. Override only what you need.

## loop

```cpp
virtual void loop();
```

Called every iteration while the module is enabled. **It must not block**: every module shares
one loop. No `delay()`, no blocking prompts.

## add_requirement

```cpp
void add_requirement(Module& other);
```

Declares that this module needs another. Take the dependency by reference in the constructor,
store it, and register it:

```cpp
WebInterface(xewe::Os& host, Wifi& wifi_ref)
    : Module(host, "web", "Web Interface", "...", false, true, true), wifi(wifi_ref) {
    add_requirement(wifi);
}
```

Declare `wifi` before `web` in the sketch.

A self-reference is ignored. The call records the edge **in both directions**: `other` becomes a
requirement of this module, and this module becomes a dependent of `other`. The back-edge makes
`disable()` cascade. Neither list is readable from outside.

## enable, disable, reset

```cpp
virtual void enable (const bool verbose = false, const bool do_restart = true);
virtual void disable(const bool verbose = false, const bool do_restart = true);
virtual void reset  (const bool verbose = false, const bool do_restart = true,
                     const bool keep_enabled = true);
```

**`do_restart` defaults to `true`**: all three reboot the device unless told not to.

**`enable`** does nothing if already enabled, and refuses (printing the requirement list) if any
requirement is disabled. Otherwise it persists `is_enabled` and restarts.

**`disable`** does nothing if already disabled, and refuses with `<name> module can't be disabled`
when `can_be_disabled` is `false`. Then:

* When `verbose`, it prints a `[WARNING] / Disabling <name> / Will reset it` header that lists
  the dependents going with it. It then asks `OK?` with a **bounded** prompt:
  `get_yn("OK?", 2, 15000, false, …)`, two attempts of 15 s, default "no". A typo or a timeout
  re-prompts once; the worst stall is 30 s. A "no" prints `Aborted` and returns. A second timeout
  or invalid answer prints `! No answer: disable cancelled`, then `Aborted`, and returns. A
  `disable` from the scheduler, a button or the web UI therefore cannot freeze an unattended
  device. See [prompts](../serial/prompts.md).
  **When `verbose` is `false` there is no confirmation.**
* It cascades `disable(false, false)` to **every dependent module**, without prompting them.
* It ends by calling `reset(verbose, do_restart, /*keep_enabled=*/false)`.

**Disabling a module wipes its NVS namespace, and its dependents', without asking unless
`verbose`.** This is the sharpest edge in the library.

**`reset`** erases the module's NVS namespace and re-writes `not_first_boot = true`. It reloads
the [settings table](settings.md), so every row is back to its default. It sets `enabled` to
`(!can_be_disabled || keep_enabled) && requirements_enabled()`, persists `is_enabled` when that is
true, and restarts when asked.

Because `not_first_boot` is rewritten, a reset module **does not ask the first-boot enable
question again**. `init_complete` is gone, so `begin_routines_init()` runs again.

## Status and getters

```cpp
virtual std::string status             (const bool verbose = false) const;
bool                is_enabled         (const bool verbose = false) const;
bool                is_disabled        (const bool verbose = false) const;
bool                init_setup_complete(const bool verbose = false) const;
bool                has_cli_cmds       ()                           const;

std::string_view    get_id             ()                           const;
std::string_view    get_name           ()                           const;
```

| | |
|---|---|
| `status` | returns `"<name> module enabled\|disabled"`, read **from NVS**, not the in-RAM flag, plus one `\n<key>: <value>` line per [settings](settings.md) row (a set secret shows `********`); prints it when `verbose`. Override to add your own state |
| `is_enabled` | prints `<name> module enabled` **only when the answer is true** |
| `is_disabled` | prints only when true. Missing requirements print `<name> module disabled` and the list; a user-disabled module prints `<name> module disabled; to enable:` and `$<id> enable` |
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

**`register_command`** adds a command to this module's `$<id>` group. It returns `false` without a
message when the module has `has_cli_commands == false`. A name that is empty or contains
whitespace, or a command without a function, is refused with
`! $<id> command '<name>' <reason>: not registered`, through
[`Os::report_error`](os.md#report_error). The `Command` aggregate is
[the Cli's](../cli/commands.md): `{name, description, sample_usage, arg_count, function}`.

**`requirements_enabled`** reports whether every required module is enabled. With `verbose` it
walks all of them and prints the missing ones; without, it short-circuits on the first.

**`run_with_dots`** runs `work` repeatedly for `duration_ms` while printing a `.` every
`dot_interval_ms`, then a newline. It is a blocking progress indicator for a setup routine; never
call it from `loop()`. A `dot_interval_ms` of `0` is clamped to 1. It does not stop early: `work`
is called as fast as possible for the full duration.

## Generic CLI commands

With `has_cli_commands`, every module gets these for free:

| Command | Args | Description | Effect |
|---|---|---|---|
| `$<id> status` | 0 | Get module status | `status(true)` |
| `$<id> reset` | 0 | Reset the module | `reset(true, true)`: **wipes the namespace and reboots with no confirmation** |
| `$<id> enable` | 0 | Enable this module | `enable(true, true)`; only when `can_be_disabled` |
| `$<id> disable` | 0 | Disable this module | `disable(true, true)`; only when `can_be_disabled` |

With a [settings table](settings.md) it also gets `$<id> set <key> <value>`, `$<id> get <key>` and
`$<id> schema`, registered at `begin()`. A name the module registered itself (any case) is kept.

`$<id>`, `$<id> help` and `$help <id>` print the group's command table ([help](../cli/help.md)).

`$<id> reset` asks nothing. `$system reset` [overrides `reset`](system.md#reset) to add a
confirmation.

## Settings

```cpp
virtual xewe::Settings settings          ()                     const;  // default: {} (no table)
virtual void           schema_extra      (xewe::SchemaOut& out) const;  // default: no rows
void                   print_schema      (xewe::SchemaOut& out) const;
bool                   apply_setting     (std::string_view key, std::string_view value, bool verbose = false);
protected:
virtual void           on_setting_changed(const xewe::SettingDef& def);
```

Plain persistent settings declared as one `constexpr` table. The core loads them and provides
`set`/`get`/`schema`, the status lines and the rows of `$system schema`. See
[settings.md](settings.md).

## Listeners

`xewe::ListenerSet<Iface, N = 4>` (`src/XeWeCore/Utils/Listeners.h`) is the core's
module-to-module change notification. A module that has something to announce defines a listener
interface and owns a set; other modules, or the sketch, add themselves.

```cpp
struct FanListener {
    virtual void on_speed(uint8_t pct, const void* origin) = 0;
};

class Fan : public xewe::Module {
public:
    xewe::ListenerSet<FanListener> listeners;           // listeners.add(&x) / remove(&x)
    void set_speed(uint8_t pct, const void* origin = nullptr) {
        speed = pct;
        listeners.notify([&](FanListener& l) { l.on_speed(pct, origin); });
    }
};
```

| | |
|---|---|
| `add(p)` | `true` when added **or already present** (no duplicates); `false` for `nullptr` or when full |
| `remove(p)` | `true` when it was registered; the slot is reused by the next `add` |
| `contains(p)`, `size()`, `capacity()` | `capacity()` is `N`, `static constexpr` |
| `notify(fn)` | calls `fn(Iface&)` for every listener, in slot order, in the caller's task |

Fixed capacity, no heap, no locking: add and notify from the main loop. A render task or an ISR
must not call `notify`. **`remove` during `notify` is safe**: each slot is re-read before its
call, so a listener removed by an earlier callback is skipped. An `add` during `notify` may or may
not receive the current event.

**The origin rule.** Every event carries `const void* origin`. Whoever caused the change passes
itself (`this`); the CLI passes `nullptr`. A listener that also sets the value (a web UI, a
HomeKit bridge) skips events whose origin is itself, so its own change is not echoed back:

```cpp
void on_speed(uint8_t pct, const void* origin) override {
    if (origin == this) return;
    push_to_clients(pct);
}
```

Callbacks run synchronously inside the setter; keep them short (set a flag, push later).
[`examples/02_MyModule`](../../examples/02_MyModule/MyModule.h) shows a set and a listener.

## NVS keys

Each module stores its state in the NVS namespace named after its `id`:

| Key | Type | |
|---|---|---|
| `is_enabled` | `bool` | persisted enable state; what `status()` reads |
| `not_first_boot` | `bool` | set once the first-boot questions have been asked |
| `init_complete` | `bool` | set after `begin_routines_init()` completes, when `requires_init_setup` |

[Settings table](settings.md) keys and your own keys go in the same namespace, so `$<id> reset`
wipes exactly this module's data. `System` also stores `device_name`. The Os uses the separate
`root` namespace for `init_setup_flag`.

## Rules of thumb

* **The framework knows no concrete modules.** A firmware project keeps its own in
  `src/<Name>/<Name>.{h,cpp}` and assembles them in the `.ino`; reusable ones go to the
  `xewe-os-modules` repository (`modules/<slug>/`).
* **Dependencies are constructor references** plus `add_requirement`, not lookups.
* **Use the core services through the Os:** `os.serial`, `os.nvs` with
  `id` as the namespace, `os.cli.execute(line)`, `os.system`.
* **Name the constructor's Os parameter `host`, never `os`.** A parameter named `os` would hide
  the member `os`. With `host`, command handlers capture `[this]` only and write `os.serial...`
  directly. Do not use `[&]`: it silently binds any shadowing parameter.
* **`loop()` must not block.**
* **A public function of a module that can be disabled starts with `if (is_disabled()) return;`.**
  The module stays registered when disabled, and others may still call it.
* **Never write `cli(` or `cli (`;** the Arduino core defines `cli` as a function-like macro.
  The `os.cli` member is fine; initialise any `Cli` with braces.
