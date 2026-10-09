# XeWeCore examples

## Three levels

| Level | You need | Start with |
|---|---|---|
| 1 | Arduino IDE + XeWeCore from the Library Manager | [`01_Hello`](01_Hello) |
| 2 | the same, plus one module of your own in the sketch folder | [`02_MyModule`](02_MyModule) |
| 3 | the project template with its build tools, repo modules (Wi-Fi, web interface, …) and board tests | [xewe-os](https://github.com/xewe-labs/xewe-os) |

## Teaching sketches

- [`01_Hello`](01_Hello/01_Hello.ino): the Os alone; a serial console with `$help`, `$system …` and a device name kept in NVS.
- [`02_MyModule`](02_MyModule/02_MyModule.ino): your own `$my …` commands, a setting in NVS, enable/disable. Copy `MyModule.h/.cpp` to start a module.

## Reference demos

- [`11_Utils`](11_Utils/11_Utils.ino): string and validation helpers, AsyncTimer, Color, LockGuard.
- [`12_Serial`](12_Serial/12_Serial.ino): SerialPort output, typed prompts, non-blocking line input.
- [`13_Cli`](13_Cli/13_Cli.ino): a standalone Cli with groups, quoting and argument checks.
- [`14_Nvs`](14_Nvs/14_Nvs.ino): typed NVS values and a FlexData struct across reboots.
- [`15_Os`](15_Os/15_Os.ino): two modules where one requires the other.
