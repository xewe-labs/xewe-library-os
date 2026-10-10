# Migrating from the 1.0.0 libraries

XeWeCore replaces XeWeUtils, XeWeSerial, XeWeCli, XeWeNvs and XeWeOS (all 1.0.0). Behaviour, NVS
keys, module ids and CLI commands are the same, so a device keeps its stored data when it is
reflashed with XeWeCore. Only names, includes and the layout differ:

| 1.0.0 | XeWeCore |
|---|---|
| `#include <XeWeOS.h>` (and the other four) | `#include <XeWeCore.h>` |
| `xewe::os::ModuleController`, `ModuleControllerConfig` | `xewe::Os` (alias `XeWeOs`), `xewe::OsConfig` |
| `xewe::os::Module`, `xewe::os::System` | `xewe::Module`, `xewe::System` |
| `controller.xewe_cli` | `os.cli` |
| protected `Module::controller` | `Module::os`; name the constructor's Os parameter `host` so `[this]` handlers use `os` directly (a parameter named `os` would hide the member) |
| global `AsyncTimer<T>` | `xewe::AsyncTimer<T>` |
| `depends_libraries=XeWeOS (>=0.1.0)` | `XeWeCore (>=2.0.0)` |

Two behaviours differ:

* The default `OsConfig::url` printed in the boot header points to
  `https://github.com/xewe-labs/xewe-os-core`. Set your own or clear it.
* `xewe::validate<T>` rejects trailing characters and `0x` forms (`"12abc"`, `"1.5"` into an
  integer, `"0x1F"`); 1.0.0 used `std::stoll`, which accepted the leading digits. See
  [utils/validator.md](utils/validator.md).
