# Blob format and versioning

`src/XeWeCore/FlexData.h` — what `to_blob()` writes, and what happens when a struct changes.

## Layout

```
byte 0        : version    (kBlobVersion, currently 1)
bytes 1..n    : fields, in fields() tuple order
```

| Field type | Encoding |
|---|---|
| arithmetic | raw little-endian, `sizeof(T)` bytes |
| `std::string` | `uint32` byte length, then the bytes, no terminator |
| `std::vector<T>` | `uint32` element count, then each element |
| nested `FlexData` | its fields inline, **with no version byte of its own** |

Nesting therefore costs nothing, and the single leading byte is the only version marker in the
whole blob.

There are no field names, no type tags and no lengths except the two above. The reader relies
entirely on `fields()` matching what was written.

## from_blob

```cpp
bool from_blob(const std::vector<uint8_t>& bytes);
```

Returns `false` when the first byte is not `kBlobVersion`, and otherwise when the input ran out
mid-decode.

**A failed decode leaves the object partially overwritten.** Fields decoded before the failure
keep their new values; the rest keep whatever they had. `from_blob` does not work on a copy.

That is exactly the "I added a field and the old blob is still in flash" case:

```cpp
Settings s;                                  // constructed with defaults
if (!nvs.read_flex("app", "settings", s)) {
    s = Settings{};                          // discard the half-decoded object
}
```

Always reset on `false`. Treating the object as usable after a failed read is the most likely bug
in code using this library.

## Changing a struct

Any of these changes the binary layout and invalidates every stored blob:

* adding, removing or reordering a `fld` entry,
* changing a field's type, including `uint8_t` → `uint16_t`,
* changing a nested struct's fields.

Renaming a `fld` name changes only the JSON key, not the layout — the binary form does not store
names.

Because there is no schema in the blob, a layout change **is not detected**: the old bytes are
read against the new field list and produce plausible garbage, or run short and return `false`
after a partial write. The version byte only catches a change you make deliberately.

### kBlobVersion is not a per-struct version

`kBlobVersion` is a single constant on `xewe::FlexData` — the **library's** wire-format version,
shared by every struct. Two things follow, and both are easy to get wrong:

* **Shadowing it in your struct does nothing.** `to_blob()` resolves `kBlobVersion` in the base
  class scope, so a `static constexpr uint8_t kBlobVersion = 2;` in your own struct is silently
  ignored and version `1` is still written. It does not fail to compile; it just has no effect.
* **Raising it in the library invalidates every stored struct on every device**, not just the one
  you changed.

So version your own schema with a **field**, as the first entry in `fields()`, and check it after
reading:

```cpp
struct Settings : xewe::FlexData<Settings> {
    uint8_t     schema = 2;          // first field: bump when the layout changes
    uint8_t     brightness = 128;
    std::string label;

    static constexpr auto fields() {
        return std::make_tuple(fld("schema",     &Settings::schema),
                               fld("brightness", &Settings::brightness),
                               fld("label",      &Settings::label));
    }
};

Settings s;
if (!nvs.read_flex("app", "settings", s) || s.schema != Settings{}.schema) {
    s = Settings{};                  // missing, corrupt, or an older schema
}
```

Because `schema` is the first field it is the first thing decoded, so it is readable even when the
rest of the layout has moved underneath it.

There is no migration built in — a device with old data comes up with defaults. If the values have
to survive, read the blob with the old struct definition, copy the fields across, and write the
new one.

## Sizing

Fixed overhead is one byte for the version, plus four bytes per string and per vector. A struct of
two `uint8_t`s and a 10-character name is `1 + 1 + 1 + 4 + 10 = 17` bytes.

A blob is written as a single NVS entry, so its size is bounded by the NVS partition and by
ESP-IDF's own per-blob ceiling. Neither is a limit a settings struct is likely to approach; if you
are persisting something large — a table, a log — check the current limits in the ESP-IDF NVS
documentation rather than assuming it fits.
