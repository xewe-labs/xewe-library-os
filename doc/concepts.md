# Concepts

What XeWeCore is, and why it is built the way it is. The reference pages say *how*; this page
says *what for*.

## One library, usable in parts

A device firmware needs the same few things every time: a console to talk to it, a way to run
commands, storage that survives a reboot, and a structure that keeps features apart. XeWeCore is
those things in one Arduino library with one include, `<XeWeCore.h>`.

The parts stand on their own. `xewe::SerialPort`, `xewe::Cli`, `xewe::Nvs`, `xewe::FlexData` and
the `xewe::str` helpers work without the module framework. `xewe::Os` (alias `XeWeOs`) puts them
together and adds modules.

## Modules

A **module** is one feature: a fan, a Wi-Fi link, a sensor. It derives from `xewe::Module`, and
its **id** is three things at once:

* the **command group**: `$fan …`,
* the **NVS namespace** where its state lives,
* its name in `$system status` and the settings schema.

That is why an id is at most 15 characters: the NVS namespace limit.

A module registers itself from its constructor, so the order in which modules are declared in the
sketch is the order in which they begin and loop. Dependencies are explicit: a module that needs
another takes it by reference and calls `add_requirement`. Disabling a module disables everything
that depends on it.

The core walks every module through a fixed boot sequence: first-boot questions, a one-time setup
that must complete once, the regular start on every later boot. A module overrides only the steps
it needs. The core itself knows no concrete module except `System`, the built-in one that holds
the device name, restart, info and factory reset. Features live in their own modules, in the
firmware or in the modules repository.

See [os/os.md](os/os.md) and [os/module.md](os/module.md).

## The console: `$group verb args`

Every action has one textual form: `$group command arguments`, with quoting for spaces.
`$help` lists everything; `$<group>` lists one group. The same line runs whether a person types
it, a button fires it, a schedule triggers it or a web page sends it (`os.cli.execute(line)`), so
there is one grammar to learn and one place where arguments are checked.

The console is the user interface, so it is kept clean and predictable: ESP log output is
silenced, every reply ends in CRLF, error strings are stable enough to grep for, input never
blocks the loop, and a confirmation question gives up after two short attempts instead of
freezing an unattended device.

See [cli/commands.md](cli/commands.md) and [serial/prompts.md](serial/prompts.md).

## Storage: NVS and FlexData

State that must survive a reboot goes to NVS, the ESP32's key-value flash store, under the
module's namespace. `xewe::Nvs` makes it typed: `write<uint16_t>`, `read<std::string>` with a
default. A missing key is the normal case on a new device, so a read miss is silent and returns
the default.

Structured data, such as a list of schedules, is a `xewe::FlexData` struct: list its fields once
and get both a compact binary form for NVS and a JSON form for the console and the web. JSON
input is type-checked field by field, so a wrong type is rejected and reported instead of being
silently coerced.

The stored format is a promise to devices already in the field: key names, types and struct
layouts do not change casually, because the data is still there after a reflash.

See [nvs/nvs.md](nvs/nvs.md) and [nvs/flexdata.md](nvs/flexdata.md).

## Settings and the schema

Most module settings are plain values with limits: a period in seconds, a name, a switch. A module
declares them once, as a `constexpr` table, and the core does the rest: loads them at boot,
validates and saves `$<id> set`, prints them in `$<id> get` and `status`, and describes them in
`$<id> schema`. Bad keys and out-of-range defaults stop the build.

`$system schema` prints every module's settings as JSON Lines: key, type, limits, default,
current value, help text. It is the device's **machine-readable surface**. A web page, a phone
app or a test can discover what a device offers and how to change it without knowing the firmware.
Secrets appear as set or unset, never as values.

See [os/settings.md](os/settings.md).

## Listeners

Modules talk to each other through listener sets: a module that has something to announce owns a
`xewe::ListenerSet`, and others add themselves. Every event carries its **origin**, so a module
that both sets and listens (a web UI, a bridge) can skip the echo of its own change.

See [os/module.md#listeners](os/module.md#listeners).

## Principles

* **Pay only for what you use.** A firmware without a settings table does not link the settings
  engine; the GPIO registry and the test hooks cost nothing until they are used.
* **No exceptions.** Malformed input is normal input: parsers return `false` or an empty optional.
* **Never block the loop.** Modules share one loop; blocking prompts belong in setup routines only.
* **Stay reachable.** A device with broken NVS keeps running and answers on the console instead of
  boot-looping.
* **Host-testable where possible.** The hardware-free parts compile and run on a PC, so most
  behaviour is checked without a board.
