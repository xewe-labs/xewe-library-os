# XeWeCore board tests

pytest files run on a real board through the `xewe-os-tools` pytest plugin (fixtures
`compiled`, `firmware`, `serial`). `tests/` is not compiled by the Arduino builder.

| File | Covers |
|---|---|
| `test_cli_parsing.py` | `Cli::execute`/`tokenize`/`print_help`: help shape, case, errors, quotes, whitespace, 254-char line runs, over-long lines dropped whole |
| `test_serial_input.py` | `SerialPort::loop`/`push_line` queue: bursts of 2/4/5/6 lines, overflow notice, CRLF, binary garbage, input during a long reply |
| `test_system.py` | `System::status` table, info/uid/mac, uptime and heap (from `$web_interface status`), `$system restart` |
| `test_prompts.py` | `get_yn`/`get_core` via `$buttons disable`: n, y (+ enable), one typo re-prompts / second invalid answer cancels, 2 × 15 s timeout cancels, type-ahead discard |
| `test_nvs_persistence.py` | button mapping, schedule and enable flags across `$system restart` |
| `test_soak_short.py` | 200 mixed commands, heap sampled every 20 |
| `test_hooks_*` | `$test` hooks: NVS, FlexData, parsers, prompts, tokenizer, heap; need `XEWE_HWTEST_DEFINES=XEWE_TESTING=1` |
| `test_recovery_*`, `test_soak_mixed.py` | input flood, restart cycles, NVS wipe (destructive, `XEWE_HWTEST_DESTRUCTIVE=1`), mixed soak; see their docstrings |

`conftest.py` adds the `cli` fixture (run a command and collect the reply, raw writes, heap/uptime,
restart + boot wait, clear button/schedule tables) and `csv_writer`.

## Run (from an xewe project)

`xewe test` always collects the project's and the modules' `tests/`; pass the files after `--`
and deselect the module tests by node-id prefix (`--ignore` does not drop explicit roots):

```sh
H=<xewe project with a board attached>
D=<this repository>/tests/board
cd $H
XEWE_HWTEST_LOG_DIR=/path/to/logs \
flock $H/.board.lock build/tools/.venv/bin/python -m xewe test --chip s3 --require-board -- \
  $D/test_cli_parsing.py $D/test_serial_input.py $D/test_system.py \
  $D/test_prompts.py $D/test_nvs_persistence.py $D/test_soak_short.py \
  --deselect=build/modules -v -s --junitxml=/path/to/logs/junit.xml
```

`XEWE_HWTEST_LOG_DIR` (optional) receives heap/uptime CSVs (`<HHMM>-heap-*.csv`).

Preconditions: board provisioned; buttons, pins, wifi, time, scheduler and web-interface enabled
(other modules may be present: the status/help tests check order and the required set, not an
exact list); Wi-Fi connected; GPIO `XEWE_TEST_BUTTONS_PIN` (default 14) free. Not 4 on an S3 image
with the fan module: the fan claims GPIO 4-7 at boot. The tests restart the board several times
(`$system restart`, `$buttons disable`/`enable`) and remove **all** button mappings and
schedules they find. They never run `$system reset` or touch Wi-Fi settings.
Runtime about 4 min 10 s on an ESP32-S3.
