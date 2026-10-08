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
void         from_json_object(JsonVariantConst v);
JsonDocument as_json_doc     ()                  const;
std::string  as_json_str     ()                  const;
void         update          (std::string_view json);
static Derived from_json     (std::string_view json);
```

| | |
|---|---|
| `to_json_object` | writes every field into an existing object |
| `from_json_object` | assigns **only the keys present and non-null** in the source |
| `as_json_doc` | a `JsonDocument` by value (ArduinoJson 7 elastic document) |
| `as_json_str` | the serialized object |
| `update` | parse and merge — a partial update, leaving unmentioned fields alone |
| `from_json` | static factory: default-construct, then merge |

```cpp
s.update(R"({"brightness": 200})");     // name is untouched
std::string json = s.as_json_str();
```

**`update` fails silently.** Malformed JSON is dropped with no error and no return value; the
object is unchanged. Validate before calling it if the input is untrusted.

## Field access by name

```cpp
template <typename V>
bool        set_field(std::string_view name, const V& value);
std::string get_field(std::string_view name) const;
```

`set_field` round-trips the value through a `JsonDocument`, so it applies JSON-style type
coercion, and returns `false` when no field has that name. `get_field` returns the value
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
**library's** format version, not yours — shadowing it in your struct compiles but has no
effect, because `to_blob()` looks it up in the base class scope. The wire format, the
version byte and the migration hazard are in [blob-format.md](blob-format.md).

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
