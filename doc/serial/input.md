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

Whether at least one complete line is waiting in the queue. It does not poll the port: `loop()`
must have run.

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

Drains the hardware RX buffer and discards every queued line, any partially typed line and the
overflow mark. Every `get_*` prompt calls it first, so a keystroke typed before the question does
not answer it.

## Notes

* **A line holds 255 bytes** (`INPUT_BUFFER_SIZE`), 254 usable. From the 255th character on, the
  rest of the line is discarded up to the newline. Then the **whole line is dropped** and
  `! Input line too long (max 254 chars): dropped` is printed once. Nothing of it is queued or
  executed. Long input (a Wi-Fi password, a URL, a JSON blob) must fit in 254 characters.
* **Up to four completed lines are queued** (`INPUT_QUEUE_LINES`). A line that completes while
  four are waiting is dropped; the queued ones are kept. Each dropped line prints
  `! Input overflow: line dropped`.
* **One burst must fit the RX buffer** (`rx_buffer_size`, 1024 bytes by default, see
  [config](config.md)). The ESP32 USB console (HWCDC) has no flow control: bytes that arrive while
  its RX queue is full are lost before `loop()` sees them. On an S3, a command sent after 1024 other
  bytes in one host write is lost.
* **There is no line editing.** A backspace is stored as a literal `\b`; arrow keys arrive as
  escape sequences. The echo is raw.
* `read_line()` returns a copy; the queue slot is reused immediately.
