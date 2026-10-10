# xewe::Nvs

`src/XeWeCore/Nvs.h` — typed key-value storage on the ESP32 NVS partition.

Every value lives under a **namespace** and a **key**, both limited to 15 characters. NVS is
initialised lazily on first use; there is nothing to call in `setup()`.

```cpp
xewe::Nvs nvs;

nvs.write<uint32_t>("app", "boots", 5);
uint32_t boots = nvs.read<uint32_t>("app", "boots", 0);    // 0 if missing
```

## set_error_handler

```cpp
using error_handler_t = std::function<void(std::string_view message)>;

void set_error_handler(error_handler_t handler);
```

Where error messages go. The default writes to the ESP log with tag `XeWeNvs`
(`ESP_LOGE`). Assigning an empty `std::function` restores the default, since the handler is only
used when it is non-empty.

```cpp
nvs.set_error_handler([&](std::string_view m) { serial.print(m); });
```

**What reaches the handler:** an invalid name (longer than 15 characters, or containing an
embedded NUL; an empty name is rejected silently), a failed `nvs_flash_init()` (reported once until init succeeds again), the automatic
partition erase (see Notes), a failed partition erase in `erase_all`, a failed namespace open (except a read-only open of a namespace that
was never written, which is the normal "missing" case), a failed `nvs_set_*`/erase, and a failed
commit. Read failures (missing key, wrong type) stay **silent**: `read` returns the default.

Messages, verbatim (`<ESP_ERR>` is `esp_err_to_name()` of the code):

```
Nvs: ERROR name '<name>' too long (<n> chars > 15 max); rejected
Nvs: ERROR name contains an embedded NUL; rejected
Nvs: ERROR nvs_flash_init failed; all reads return defaults, writes fail (<ESP_ERR>)
Nvs: ERROR partition unusable, erased all NVS data (<ESP_ERR>)
Nvs: ERROR partition erase failed (<ESP_ERR>)
Nvs: ERROR open namespace failed '<ns>' (<ESP_ERR>)
Nvs: ERROR write failed (<ESP_ERR>)        e.g. ESP_ERR_NVS_NOT_ENOUGH_SPACE, ESP_ERR_NVS_VALUE_TOO_LONG
Nvs: ERROR commit failed (<ESP_ERR>)
```

`read<T>` cannot tell "missing" from "stored with another type" (signedness and width are part of
the type; `bool` and `uint8_t` share one) or from "NVS unavailable": all return `default_value`.

## write

```cpp
template <typename T>
bool write(std::string_view ns, std::string_view key, const T& value);
```

Returns `true` when the value was stored and committed.

| `T` | Stored as |
|---|---|
| `std::string`, Arduino `String` | NVS string |
| `std::string_view` | NVS string (copied first) |
| anything convertible to `const char*` | NVS string; a null pointer stores `""` |
| `bool` | `u8`, `1`/`0` |
| signed integral, 1/2/4/8 bytes | `i8` / `i16` / `i32` / `i64` |
| unsigned integral, 1/2/4/8 bytes | `u8` / `u16` / `u32` / `u64` |
| `float`, `double` | **a blob** of `sizeof(value)` bytes |
| anything else | compile error: `"Unsupported Nvs::write<T>() type."` |

**Floats are blobs, not NVS numbers.** NVS has no float type, so they are stored raw. They are
invisible to `nvs_get_*` and to external tools that walk typed entries, and a change of type
(`float` → `double`) makes the stored value unreadable.

Every `write` commits immediately — one flash commit per call.

## read

```cpp
template <typename T>
T read(std::string_view ns, std::string_view key, T default_value = T());
```

Returns the stored value, or `default_value` when the key is missing, the name is invalid, or the
stored size does not match.

The supported types are the same as `write` **minus `std::string_view` and `const char*`**, which
are write-only and fail to compile here (`"Unsupported Nvs::read<T>() type."`). Read them back as
`std::string` or Arduino `String`.

A `float`/`double` is only accepted when the stored blob is exactly `sizeof(T)` bytes; otherwise
the default comes back.

## write_blob and read_blob

```cpp
bool write_blob(std::string_view ns, std::string_view key, const std::vector<uint8_t>& data);
bool write_blob(std::string_view ns, std::string_view key, std::span<const uint8_t>    data);

std::vector<uint8_t> read_blob(std::string_view ns, std::string_view key);
```

Raw bytes. The `vector` overload forwards to the `span` one, which writes the bytes directly.

**`read_blob` cannot distinguish "missing" from "empty"** — both return an empty vector. Store a
separate presence flag if that matters.

## write_flex and read_flex

```cpp
template <typename T> bool write_flex(std::string_view ns, std::string_view key, const T& obj);
template <typename T> bool read_flex (std::string_view ns, std::string_view key, T&       out);
```

Persist a whole struct. `T` must derive from `xewe::FlexData<T>` — see
[flexdata.md](flexdata.md) — or the call fails to compile. (The `static_assert` messages say
`Nvs::save<T>()` and `Nvs::load<T>()`; they mean these two.)

`write_flex` is `write_blob(ns, key, obj.to_blob())`. `read_flex` returns `false` when the key is
missing and when the stored blob does not decode. In the second case `out` is already **partially
overwritten**: reset it on `false`, as shown in [blob-format.md](blob-format.md#from_blob).

## remove, reset_ns and erase_all

```cpp
void remove   (std::string_view ns, std::string_view key);
void reset_ns (std::string_view ns);
bool erase_all();
```

| | |
|---|---|
| `remove` | deletes one key. Returns `void`; a missing key is not an error and skips the commit |
| `reset_ns` | erases every key in a namespace. Returns `void`, so there is no success signal |
| `erase_all` | erases the **entire NVS partition** and re-initialises. Returns `false` if either step fails; a failed erase goes to the error handler as `Nvs: ERROR partition erase failed (<esp error>)` |

`reset_ns` probes the namespace read-only first, so erasing one that was never written does not
create it.

**`erase_all()` wipes every namespace on the device**, including those belonging to other
libraries and to the ESP-IDF itself (Wi-Fi calibration data, for example). In XeWe OS it is what
`$system reset` calls for a factory reset.

## Notes

* **Lazy init with an automatic erase.** The first operation calls `nvs_flash_init()`. If that
  returns `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND`, the library
  **deinitialises and erases the whole NVS partition**, then initialises again. That recovers a
  full or format-changed partition automatically, at the cost of every stored value — a device
  that fills NVS loses its settings on the next boot; the erase is reported to the error handler.
* **The 15-character limit applies to namespaces as well as keys.** An over-long or empty name is rejected
  before any flash access: `write` returns `false`, `read` returns the default, `read_blob`
  returns empty, and `remove`/`reset_ns` do nothing.
* `Nvs` is default-constructible and copyable, but the readiness flag is per-instance while
  `nvs_flash_init()` is global. One instance is the intended use; XeWe OS exposes it as
  `os.nvs`.
* Handles are closed by an internal RAII wrapper on every path, including error returns.
