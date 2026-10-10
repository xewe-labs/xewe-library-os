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

Drains everything currently in the RX buffer and assembles it into lines. Call it every iteration
of the sketch's `loop()`. It never blocks: it reads only what is already available, calling
`yield()` between characters.

Per character: `'\r'` is discarded, `'\n'` ends the line, anything else is appended. When `echo` is
on, the character is written back first. Each completed line is pushed onto a FIFO of up to four
lines (`INPUT_QUEUE_LINES`), so several lines arriving in one pass are all kept, in order.

## has_line

```cpp
bool has_line() const;
```

Whether at least one complete line is waiting in the queue. A plain read — it does not poll the port, so `loop()` has
to have run.

## read_line

```cpp
std::string read_line();
```

Removes and returns the oldest queued line. Returns `{}` when no line is ready. The returned
string does not include the terminating newline. A partially typed line is not affected.

## clear_input

```cpp
void clear_input();
```

Drains the hardware RX buffer and discards every queued line and any partially typed line. Every `get_*` prompt calls
this first, so a stray keystroke typed before the question does not answer it.

## Notes

* **A line holds 255 bytes** (`INPUT_BUFFER_SIZE`), 254 usable. When a 255th character arrives,
  the line is marked as overflowed and every further byte is discarded up to the newline; then
  the **whole line is dropped** and `! Input line too long (max 254 chars): dropped` is printed
  once. Nothing of an over-long line is queued or executed, and the next line starts clean
  (`clear_input()` also clears the overflow mark). Anything accepting long input — a Wi-Fi
  password, a URL, a JSON blob — must stay within 254 characters per line.
* **Up to four completed lines are queued** (`INPUT_QUEUE_LINES`). If a line completes while
  four are already waiting, the **newest** line is dropped, the queued ones are kept, and
  `! Input overflow: line dropped` is printed once per dropped line. The queue costs about 1 KB of
  static RAM per `SerialPort`.
* **One burst must fit the RX buffer** (`rx_buffer_size`, 1024 bytes by default, see
  [config](config.md)). On the ESP32's USB console (HWCDC) the driver has no flow control: bytes
  that arrive while its RX queue is full are discarded before `loop()` sees them. On an S3, one
  host write of 1024 junk bytes + `\n$system uid\n` (1037 bytes) lost the command at its end.
* **There is no line editing.** A backspace is stored as a literal `\b` character; arrow keys
  arrive as escape sequences. The echo is a raw echo, not a readline.
* `read_line()` returns a copy; the queue slot is reused immediately.
