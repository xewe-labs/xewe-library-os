# xewe::FlexData\<T\>

`src/XeWeCore/FlexData.h` — declare a struct's fields once and get both a compact binary form for
flash and a JSON form for APIs.

```cpp
struct Settings : xewe::FlexData<Settings> {
    uint8_t     brightness = 128;
    std::string name;

    static constexpr auto fields() {
        return std::make_tuple(fld("brightness", &Settings::brightness),
                               fld("name",       &Settings::name));
    }
};
```

`FlexData` is CRTP: the struct passes **itself** as the template argument. Everything below is
inherited; the only thing a struct supplies is `fields()`.

## fields and fld

```cpp
template <typename C, typename M>
struct Field { const char* name; M C::* ptr; };

template <typename C, typename M>
constexpr Field<C, M> fld(const char* n, M C::* p);
```

`fields()` must be `static constexpr` and return a `std::make_tuple` of `fld` entries. The names
are the JSON keys; the order is the binary field order.

`fld` is found by ADL from inside a `FlexData`-derived struct, so it needs no `xewe::` prefix
there.

## Supported field types

| Supported | |
|---|---|
| any arithmetic type | `bool`, `char`, all `intN_t`/`uintN_t`, `float`, `double` |
| `std::string` | length-prefixed |
| a nested `FlexData` struct | inlined, field by field |
| `std::vector<T>` | where `T` is any of the above, including nested structs and further vectors |

**Not supported** — a hard compile error, `"no blob_write overload for this type"`: Arduino
`String`, enums and enum classes, `std::array`, pointers, `std::optional`, `std::map`. Use
`std::string` instead of `String`, and store an enum as its underlying integer.

## JSON

```cpp
void         to_json_object  (JsonObject o)      const;
bool         from_json_object(JsonVariantConst v);
JsonDocument as_json_doc     ()                  const;
std::string  as_json_str     ()                  const;
bool         update          (std::string_view json);
static Derived from_json     (std::string_view json);
```

| | |
|---|---|
| `to_json_object` | writes every field into an existing object |
| `from_json_object` | assigns **only the keys present and non-null** in the source, each only if its JSON type matches (see Type rules); `false` if any was rejected |
| `as_json_doc` | a `JsonDocument` by value (ArduinoJson 7 elastic document) |
| `as_json_str` | the serialized object |
| `update` | parse and merge — a partial update, leaving unmentioned fields alone; `false` on a parse error or a rejected field |
| `from_json` | static factory: default-construct, then merge |

```cpp
s.update(R"({"brightness": 200})");     // name is untouched
std::string json = s.as_json_str();
```

**Malformed JSON is not reported.** `update` returns `false` and the object is unchanged, but
nothing is printed. Type mismatches are reported (below).

### Type rules

Assignment is type-matched (`is<M>()` before `as<M>()`). A
present value of the wrong JSON type is **rejected**: that field keeps its previous value, the
other fields are still applied, the call returns `false`, and `xewe::flex_error_handler` gets
`! <Struct>.<field>: expected <type>, got <json type>`, e.g. `! Settings.name: expected string,
got integer`.

| Field type | Accepted JSON | Rejected (examples) |
|---|---|---|
| `bool` | `true` / `false` | `"false"`, `"yes"`, `""`, `0`, `1` |
| integers | an integer within the field's range | `1.5`, `"7"`, `-1` into unsigned, `300` into `uint8_t` |
| `float` / `double` | any number (integer or float) | `"1.5"`, `true` |
| `std::string` | a string | `5`, `true`, objects, arrays |
| nested struct | an object (its own fields follow these rules and report as `<Inner>.<field>`) | anything else |
| `std::vector<T>` | an array (elements converted with `as<T>()`, not type-checked) | anything else |

`xewe::flex_error_handler` is a `std::function<void(std::string_view)>`; `Os::begin` points it at
the console. Unset, rejections are silent but still happen. A rejection inside a nested struct is
reported but does not make the outer call return `false`.

### Field presence

```cpp
uint32_t present() const;
bool     has    (std::string_view field) const;
```

`from_json_object` (and so `update` and `from_json`) records which fields it assigned, so a loader
can tell a field that was **missing** from one that holds its **default**:

```cpp
Settings s;
s.update(json);
if (!s.has("schema")) { /* no schema key: not "version 1" */ }
```

| After loading | `has("schema")` | `schema` |
|---|---|---|
| `{"name":"x"}` (missing) | `false` | default, `1` |
| `{"schema":null}` (null) | `false` | default |
| `{"schema":"2"}` (wrong type, rejected) | `false` | unchanged |
| `{"schema":1}` (present, equal to the default) | `true` | `1` |
| `{"schema":3}` (present) | `true` | `3` |

* `present()` is the bitmask: bit *i* is the *i*-th entry of `fields()`. `has` of an unknown name
  is `false`.
* Each load **replaces** the mask: after two `update` calls it describes the second one only.
* A blob always holds every field, so a successful `from_blob` (and so `Nvs::read_flex`) marks
  **every** field present, and a failed one (wrong blob version, too short) marks none. A loader can
  therefore write `read_flex(...) && s.has("schema") && s.schema == SCHEMA` for blobs and JSON
  alike.
* `set_field` does not change it.
* Nested structs (and the elements of a `std::vector` of structs) track their own presence:
  `s.inner.has("a")`.
* **At most 32 fields:** `present()` and `has()` do not compile (`static_assert`) on a struct with
  more; such a struct still loads JSON and blobs.
* **Cost:** one `uint32_t` in every `FlexData` struct (4 bytes per instance, including each
  element of a vector of structs). It is not stored in the blob or the JSON, so stored data is
  unaffected.

## Field access by name

```cpp
template <typename V>
bool        set_field(std::string_view name, const V& value);
std::string get_field(std::string_view name) const;
```

`set_field` round-trips the value through a `JsonDocument` and applies the same
[type rules](#type-rules): it returns `false` when no field has that name or the value's type does
not match (`set_field("enabled", "true")` on a `bool` is rejected). `get_field` returns the value
**JSON-serialized** — strings come back quoted, nested structs as `{...}` — and the literal string
`"null"` for an unknown name.

## Binary form

```cpp
static constexpr uint8_t kBlobVersion = 1;

void                 write_fields(BlobWriter& w) const;
void                 read_fields (BlobReader& r);
std::vector<uint8_t> to_blob     () const;
bool                 from_blob   (const std::vector<uint8_t>& bytes);
```

`to_blob`/`from_blob` are what
[`Nvs::write_flex`/`read_flex`](nvs.md#write_flex-and-read_flex) call. `kBlobVersion` is the
library's format version, not a per-struct one. The wire format, the version byte and how to
version your own struct are in [blob-format.md](blob-format.md).

## Codec types

```cpp
struct BlobWriter { std::vector<uint8_t> bytes; void raw(const void* p, size_t n); };
struct BlobReader { const uint8_t* p; const uint8_t* end; bool ok = true; bool take(void* out, size_t n); };

template <typename T> void blob_write(BlobWriter&, const T&);                 // arithmetic or nested FlexData
inline                void blob_write(BlobWriter&, const std::string&);
template <typename T> void blob_write(BlobWriter&, const std::vector<T>&);
template <typename T> void blob_read (BlobReader&, T&);
inline                void blob_read (BlobReader&, std::string&);
template <typename T> void blob_read (BlobReader&, std::vector<T>&);
```

Public, and the extension point: adding an overload pair for your own type makes it usable as a
field.

## ArduinoJson converters

This header injects two specializations into `ArduinoJson`:

* `Converter<std::vector<T>>` — arrays of scalars, strings or nested `FlexData` structs.
* `Converter<T>` for any `T` deriving from `xewe::FlexData<T>` — which is what makes a nested
  struct work as an object *and* as a vector element.

They are global to the translation unit. If you serialize `std::vector<T>` with ArduinoJson
elsewhere in a sketch that includes `<XeWeCore.h>`, these are the converters that will be used.
