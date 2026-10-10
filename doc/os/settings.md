# Settings table

`src/XeWeCore/Settings.h` (core 2.1.0). A module declares its plain persistent settings as one
`constexpr` table; the core then provides, from that table alone:

* the load at `begin()`: table default, then the value stored in NVS;
* `$<id> set <key> <value>`, `$<id> get <key>`, `$<id> schema`;
* one `key: value` line per setting in `status()`;
* the module's rows in `$system schema`.

A module that does not declare a table gets none of it, and pays nothing: no commands, no NVS
reads, no output. A firmware in which **no** module declares a table does not even link the
engine (`$system schema` still answers, with a header and no rows); measured on `15_Os` (ESP32-C3)
core 2.1 adds 4.0 KB of flash over 2.0.1. The first module that declares a table links the engine,
about 9 KB once; each further row costs its `SettingDef` (48 B) plus its strings.

## Declaring a table

```cpp
class MyModule : public xewe::Module {
public:
    xewe::Settings settings() const override {
        static constexpr xewe::SettingDef table[] = {
            xewe::setting<&MyModule::number>("number", 0, 1000, 0, "The remembered number"),
            xewe::setting<&MyModule::beat_s>("beat_s", 1, 3600, 10, "Heartbeat period, s",
                                             xewe::SettingDef::RESTART),
            xewe::setting<&MyModule::label> ("label", 16, "my", "Heartbeat prefix"),
            xewe::setting<&MyModule::pass>  ("pass", 63, "", "API token", xewe::SettingDef::SECRET),
        };
        return {table, this};
    }
private:
    uint16_t    number = 0;
    uint16_t    beat_s = 10;
    std::string label;
    std::string pass;
};
```

Each row binds a **member** of the module; the core reads and writes that member, and the module
uses it like any other field. `setting<&Class::member>` deduces the type from the member:

| Member type | `type` | `setting<>` arguments |
|---|---|---|
| `bool` | `bool` | `(key, default, doc = nullptr, flags = 0)` |
| `uint8_t` `int8_t` `uint16_t` `int16_t` `uint32_t` `int32_t` (`int`) | `u8` `i8` `u16` `i16` `u32` `i32` | `(key, min, max, default, doc = nullptr, flags = 0)` |
| `float` | `f32` | `(key, min, max, default, doc = nullptr, flags = 0)` |
| `std::string` | `str` | `(key, max_len, default, doc = nullptr, flags = 0)` |

Anything else (64-bit, `double`, enums, structs) does not compile; use `FlexData` for structures.

**The table is checked when it is built.** It must be `static constexpr`, and a row whose key is
empty, longer than 15 characters, contains whitespace, `"` or `\`, or is one of the core's keys
(`is_enabled`, `not_first_boot`, `init_complete`), or whose default is outside `[min, max]`, or
whose bounds are outside the member's type, stops the build with
`call to non-'constexpr' function 'setting_key_invalid'` (or `setting_range_invalid`).
Duplicate keys are not detected; the first row wins.

**`return {table, this};`** — rows must name members of the class that returns the table (the
core casts `this` back to that class). `settings()` is `const`; the core still writes the members
through it (that is what the table is for).

**The key is the NVS key** (namespace = module id) and the CLI name. One key per setting, at most
15 characters. A module that already stores a value under a key keeps its data by naming the row
with that key and the same type.

### Flags

| Flag | Effect |
|---|---|
| `SettingDef::SECRET` | the value is never printed: `get`, `status` and `schema` show `********` (or nothing when unset), the schema row has `"secret":true` and `"set":true\|false` and no `default`. A secret string is "set" when non-empty, a secret number when it differs from its default |
| `SettingDef::RESTART` | `set` applies and saves at once, then prints `Takes effect after $system restart`; the schema row has `"restart":true` |

## Lifecycle

* **`Module::begin()` step 0**, before everything else and also for a disabled module: every
  row's member gets the table default, then the NVS value under `<id>/<key>` if one is stored
  **with the row's type** (a value stored with another type reads as missing, silently, as every
  `Nvs::read` miss does). A stored value outside `[min, max]` (or a string longer than `max_len`)
  is replaced by the default and reported once: `! <id>/<key>: stored value outside u8 in [1, 200],
  using the default`. The table default wins over the member's in-class initializer. Then
  `$<id> set`, `get` and `schema` are registered, except any name the module registered itself.
* **`$<id> set`** validates, applies, persists and notifies immediately (no staging):
  parse by type, check the range, assign the member, `os.nvs.write` under `<id>/<key>`, then call
  `on_setting_changed(row)`. It works while the module is disabled; `on_setting_changed` should
  check `is_enabled()` before touching hardware.
* **`reset()`** (and therefore `disable()`) wipes the namespace and reloads the table: every
  member is back at its default.

## Module API

```cpp
virtual xewe::Settings settings          ()                       const;   // {} = no table
virtual void           schema_extra      (xewe::SchemaOut& out)   const;   // extra rows, see below
void                   print_schema      (xewe::SchemaOut& out)   const;   // table rows + schema_extra
bool                   apply_setting     (std::string_view key, std::string_view value,
                                          bool verbose = false);           // `set` without the CLI
protected:
virtual void           on_setting_changed(const xewe::SettingDef& def);    // after set/apply_setting
```

`apply_setting` returns `false` on an unknown key or a refused value; with `verbose` it prints
what `$<id> set` prints. A module may still write a member and `os.nvs.write` the same key itself
(e.g. a first-boot prompt); the next boot loads it.

## Commands

| Command | Output |
|---|---|
| `$<id> set <key> <value>` | `<key>=<value>` (secrets masked); `! $<id> set <key>: expected u16 in [0, 1000]`; `! $<id>: no setting '<key>' (see $<id> schema)` |
| `$<id> get <key>` | `<key>=<value>`, strings raw, secrets `********` (empty when unset) |
| `$<id> schema` | one JSON line per row, then `{"end":"<id>","count":N}` |
| `$system schema` | header line, every module's rows with `"module":"<id>"`, then `{"end":"system","count":N}` |

Values `set` accepts: `bool`: `true/false`, `on/off`, `yes/no`, `1/0` (any case). Integers:
decimal, optional sign. `f32`: `[+-]digits[.digits][e[+-]digits]` (no `nan`, `inf` or hex). `str`:
the argument as typed; quote it to keep spaces (`$wifi set ssid "My Net"`). Input lines are capped
at 254 characters.

**A module's own command wins.** If the module registered `set` (or `get`, `schema`) itself, the
core does not add its own; existing firmware keeps its syntax. Such a module can still call
`apply_setting` from its handler.

## Schema lines

JSON Lines, one object per row, fields in this order, absent ones omitted:

```json
{"key":"number","type":"u16","min":0,"max":1000,"default":0,"value":42,"doc":"The remembered number"}
{"key":"beat_s","type":"u16","min":1,"max":3600,"default":10,"value":10,"doc":"Heartbeat period, s","restart":true}
{"key":"label","type":"str","min":0,"max":16,"default":"my","value":"my","doc":"Heartbeat prefix"}
{"key":"pass","type":"str","min":0,"max":63,"value":"********","secret":true,"set":false,"doc":"API token"}
{"end":"my","count":4}
```

`type` is `bool u8 i8 u16 i16 u32 i32 f32 str`. `min`/`max` are absent for `bool` and are lengths
for `str`. `f32` numbers print with up to 7 significant digits.

`$system schema`:

```json
{"schema":1,"core":"2.1.0","device":"Kitchen Lights","modules":["system","my"]}
{"module":"my","key":"number","type":"u16","min":0,"max":1000,"default":0,"value":42,"doc":"The remembered number"}
{"end":"system","count":1}
```

`modules` lists every registered module, with or without settings. `schema` is the line format
version (raised only on an incompatible change); `core` is `XEWE_CORE_VERSION`.

### Extra rows: `schema_extra`

Settings that are not plain table rows (led mode parameters, keyed per mode, set through their
own command) can still appear in the schema. Override `schema_extra` and write one row per
parameter; `SchemaOut::row` adds the braces and, under `$system schema`, the module field:

```cpp
void Led::schema_extra(xewe::SchemaOut& out) const {
    // for each mode m and parameter p:
    out.row(R"("key":"speed","group":"mode:rainbow","type":"u8","min":1,"max":255,"value":40,)"
            R"("set":"$led mode param 5 speed <value>")");
}
```

`group` names the family (`mode:<name>`); `set` is the command that changes it. These rows are not
handled by `$<id> set/get`. A module with extra rows but no table registers its own `schema`
command (`print_schema` plus the end line).

## Versioning (`requires_core`)

The table, `Settings.h`, `Utils/Listeners.h` and `$system schema` are new in **2.1.0**. A module
that declares a table (or uses `ListenerSet`) declares `requires_core = ">=2.1.0,<3.0.0"` in its
manifest and `XeWeCore (>=2.1.0)` in `depends`. Modules that use neither keep `>=2.0.0,<3.0.0`.
`XEWE_CORE_VERSION_MAJOR/MINOR/PATCH` allow an `#if` in code shared across versions.
