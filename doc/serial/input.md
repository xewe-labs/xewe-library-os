# Reading lines

`src/XeWeCore/Serial.h` — the non-blocking input path.

```cpp
void loop() {
    serial.loop();                                   // assemble lines, never blocks
    if (serial.has_line()) {
        std::string line = serial.read_line();
        serial.print("> " + line);
    }
}
```

## loop

```cpp
void loop();
```

Drains everything currently in the RX buffer and assembles it into a line. Call it every iteration
of the sketch's `loop()`. It never blocks: it reads only what is already available, calling
`yield()` between characters.

Per character: `'\r'` is discarded, `'\n'` ends the line, anything else is appended. When `echo` is
on, the character is written back first.

## has_line

```cpp
bool has_line() const;
```

Whether a complete line is waiting. A plain flag read — it does not poll the port, so `loop()` has
to have run.

## read_line

```cpp
std::string read_line();
```

Returns the pending line and clears it. Returns `{}` when no line is ready. The returned string
does not include the terminating newline.

## clear_input

```cpp
void clear_input();
```

Drains the hardware RX buffer and discards any partially typed line. Every `get_*` prompt calls
this first, so a stray keystroke typed before the question does not answer it.

## Notes

* **The line buffer is 255 bytes** (`INPUT_BUFFER_SIZE`), 254 usable. A line reaching that length
  is terminated as if a newline arrived: a longer line is **silently split into several lines**,
  with no error and no marker. Anything accepting long input — a Wi-Fi password, a URL, a JSON
  blob — must account for that.
* **Only one line is buffered.** If two lines arrive in one `loop()` and you do not call
  `read_line()` in between, the second overwrites the first. There is no queue.
* **There is no line editing.** A backspace is stored as a literal `\b` character; arrow keys
  arrive as escape sequences. The echo is a raw echo, not a readline.
* `read_line()` returns a copy; the internal buffer is reused immediately.
