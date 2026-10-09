// Reference demo: string and validation helpers, AsyncTimer, Color and LockGuard.
// XeWeCore Utils: string and validation helpers, then AsyncTimer, Color and
// LockGuard working together. A non-blocking fade drives the LED while a second
// task advances a hue; the two share state through a mutex guarded by
// xewe::LockGuard.
//
// Enable the AsyncTimer debug flag to watch the timer:
//   arduino-cli compile --build-property "compiler.cpp.extra_flags=-DDEBUG_AsyncTimer=1" ...
#include <XeWeCore.h>

#define LED_PIN 8   // onboard LED on many dev boards; change for yours

xewe::AsyncTimer<uint8_t> fade(1500, 0, 255);

uint8_t             hue   = 0;      // 0-255, NOT 0-360
bool                ready = false;  // false if setup() bailed out

// Shared between loop() and the hue task. Never touched without the guard.
SemaphoreHandle_t   hue_mutex = nullptr;

static void hue_task(void*) {
    for (;;) {
        {
            xewe::LockGuard guard(hue_mutex);   // takes here...
            hue += 2;
        }                                       // ...and gives here, on every path
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void string_helpers() {
    Serial.println(xewe::str::capitalize("kitchen lights").c_str());   // Kitchen Lights

    uint16_t minutes = 0;
    if (xewe::str::parse_time("07:30", minutes)) {
        Serial.print(xewe::str::format("07:30 is %u minutes after midnight\n", minutes).c_str());
    }

    auto brightness = xewe::validate<int>("180", 0, 255);
    if (brightness) Serial.print(xewe::str::format("valid brightness: %d\n", *brightness).c_str());

    for (const auto& line : xewe::str::wrap_words("wrap this sentence into short lines", 12)) {
        Serial.println(line.c_str());
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    pinMode(LED_PIN, OUTPUT);

    string_helpers();

    hue_mutex = xSemaphoreCreateMutex();
    if (hue_mutex == nullptr) {
        Serial.println("could not create mutex");
        return;
    }
    xTaskCreate(hue_task, "hue", 2048, nullptr, 1, nullptr);

    fade.initiate();   // nothing advances until this is called
    ready = true;
    Serial.println("fading up");
}

void loop() {
    if (!ready) return;   // never take a guard over a mutex that failed to create

    // get_current_value() is what recomputes progress; get_progress() alone
    // returns a cached value and can be up to 5 ms stale.
    analogWrite(LED_PIN, fade.get_current_value());

    if (fade.is_done()) {
        const uint8_t from = fade.get_target_value();
        const uint8_t to   = (from == 0) ? 255 : 0;

        fade.reset(from, to);   // reset() clears `initiated` too...
        fade.initiate();        // ...so it has to be started again

        uint8_t h;
        {
            xewe::LockGuard guard(hue_mutex);
            h = hue;
        }

        // every channel is 0-255 here, including hue
        const auto rgb = xewe::color::hsv_to_rgb({h, 255, 255});
        Serial.print(xewe::str::format("fading %s  hue %3u -> rgb %3u,%3u,%3u\n",
                                       to == 255 ? "up  " : "down", h, rgb[0], rgb[1], rgb[2]).c_str());
    }

    delay(10);   // this sketch has nothing else to do; a real loop() would not block
}
