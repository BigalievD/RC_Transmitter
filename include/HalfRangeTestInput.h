#pragma once
#include <Arduino.h>

// Temporary physical SG switch input on ESP32-P4 GPIO32.
// Open (internal pull-up): -100%; connected to GND: +100%.
// Half range is a separate per-mix setting, not controlled by this pin.
static constexpr uint8_t HALF_RANGE_TEST_GPIO=32;

inline int16_t gpio32SgSwitchValue(){
    return digitalRead(HALF_RANGE_TEST_GPIO)==LOW?1000:-1000;
}
