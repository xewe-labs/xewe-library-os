# xewe::color

`src/XeWeCore/Utils/Color.h` — HSV ⇄ RGB conversion for 8-bit channels.

**Host-includable:** the header uses the standard library only (no `<Arduino.h>`), so pure code
such as effect math can include it and build in host tests. `tests/unit/run.sh` compiles it
without the Arduino shim to keep it that way.

**All six channels are `0-255`,** including hue and saturation. Hue is *not* 0-360 and saturation
and value are *not* 0-100 — this is the most common mistake with these two functions.

## hsv_to_rgb

```cpp
inline std::array<uint8_t, 3> hsv_to_rgb(const std::array<uint8_t, 3>& hsv);
```

`{hue, saturation, value}` → `{red, green, blue}`.

```cpp
auto rgb = xewe::color::hsv_to_rgb({0, 255, 255});      // full-saturation red
analogWrite(pin_r, rgb[0]);
```

## rgb_to_hsv

```cpp
inline std::array<uint8_t, 3> rgb_to_hsv(const std::array<uint8_t, 3>& rgb);
```

`{red, green, blue}` → `{hue, saturation, value}`. Returns hue `0` for any greyscale input
(`r == g == b`), since hue is undefined there.

## Notes

* **Conversion is lossy.** Both functions work in `float` internally and truncate with
  `static_cast<uint8_t>` on the way out, so a round trip does not always return the original
  triplet — expect drift of a unit or so, most visibly at hue boundaries.
* No clamping is needed or performed: `uint8_t` inputs are already in range.
* Both are `inline` and header-only; there is nothing to link.
* **`hsv_to_rgb` is pinned bit for bit** by `tests/unit` (`color_hsv_to_rgb_pinned`: spot values
  and a checksum over a 256 × 256 × 16 grid). Effect code, such as the led module's colour modes,
  calls it and depends on its exact output, so the arithmetic does not change (operation order
  included: an FMA-contracted build already gives different bytes).
