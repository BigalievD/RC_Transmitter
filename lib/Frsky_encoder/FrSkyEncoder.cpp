#include "FrSkyEncoder.h"
#include "driver/gpio.h"

FrSkyEncoder::FrSkyEncoder(uint8_t pinA, uint8_t pinB, uint8_t pinSW): pinA(pinA), pinB(pinB), pinSW(pinSW) {}

void FrSkyEncoder::begin() {
    pinMode(pinA, INPUT_PULLUP);
    pinMode(pinB, INPUT_PULLUP);
    pinMode(pinSW, INPUT_PULLUP);
    const uint32_t now = millis();
    portENTER_CRITICAL(&inputMux);
    previousAB = (gpio_get_level((gpio_num_t)pinA) << 1) | gpio_get_level((gpio_num_t)pinB);
    partialStep = 0;
    rotarySynced = previousAB == 0 || previousAB == 3;
    pendingLeft = 0;
    pendingRight = 0;
    buttonHead = 0;
    buttonTail = 0;
    buttonOverflow = false;
    portEXIT_CRITICAL(&inputMux);
    queuedLeft = 0;
    queuedRight = 0;
    encoderPosition = 0;
    rawButtonState = digitalRead(pinSW);
    // Starting with a held button is processed as a fresh debounced press.
    buttonState = HIGH;
    rawButtonSince = now;
    pressStartTime = now;
    buttonEvents = 0;
    longPressEvents = 0;
    releaseEvents = 0;
    longPressTriggered = false;
    attachInterruptArg(pinA, encoderISR, this, CHANGE);
    attachInterruptArg(pinB, encoderISR, this, CHANGE);
    attachInterruptArg(pinSW, buttonISR, this, CHANGE);
}

void ARDUINO_ISR_ATTR FrSkyEncoder::encoderISR(void *arg) {
    static_cast<FrSkyEncoder *>(arg)->handleEncoderISR();
}

void ARDUINO_ISR_ATTR FrSkyEncoder::buttonISR(void *arg) {
    static_cast<FrSkyEncoder *>(arg)->handleButtonISR();
}

void ARDUINO_ISR_ATTR FrSkyEncoder::handleEncoderISR() {
    portENTER_CRITICAL_ISR(&inputMux);
    const uint8_t ab = (gpio_get_level((gpio_num_t)pinA) << 1) | gpio_get_level((gpio_num_t)pinB);
    const uint8_t changed = previousAB ^ ab;
    if (changed == 0) {
        portEXIT_CRITICAL_ISR(&inputMux);
        return;
    }
    if (changed == 3) {
        // Both bits changed: a missed edge is not a valid step.
        partialStep = 0;
        rotarySynced = ab == 0 || ab == 3;
        previousAB = ab;
        portEXIT_CRITICAL_ISR(&inputMux);
        return;
    }
    const uint8_t transition = (previousAB << 2) | ab;
    previousAB = ab;
    if (!rotarySynced) {
        rotarySynced = ab == 0 || ab == 3;
        partialStep = 0;
        portEXIT_CRITICAL_ISR(&inputMux);
        return;
    }
    // RIGHT: 00 -> 10 -> 11 -> 01 -> 00. Bounce reverses and cancels.
    switch (transition) {
        case 2: case 11: case 13: case 4: ++partialStep; break;
        default: --partialStep; break;
    }
    // Accept two valid edges per step, as in the original half-step setup.
    // Never emit from an intermediate contact state (01 or 10).
    if (ab == 0 || ab == 3) {
        if (partialStep >= 2) ++pendingRight;
        if (partialStep <= -2) ++pendingLeft;
        partialStep = 0;
    }
    portEXIT_CRITICAL_ISR(&inputMux);
}

void ARDUINO_ISR_ATTR FrSkyEncoder::handleButtonISR() {
    portENTER_CRITICAL_ISR(&inputMux);
    const uint8_t next = (buttonHead + 1) % BUTTON_QUEUE_SIZE;
    if (next == buttonTail) {
        buttonOverflow = true;
    } else {
        buttonTimes[buttonHead] = millis();
        buttonLevels[buttonHead] = gpio_get_level((gpio_num_t)pinSW);
        buttonHead = next;
    }
    portEXIT_CRITICAL_ISR(&inputMux);
}

void FrSkyEncoder::checkLongPress(uint32_t time) {
    if (buttonState == LOW && !longPressTriggered && (uint32_t)(time - pressStartTime) >= longPressTime) {
        longPressTriggered = true;
        ++longPressEvents;
    }
}

void FrSkyEncoder::settleButton(uint32_t time) {
    if (rawButtonState != buttonState && (uint32_t)(time - rawButtonSince) >= BUTTON_DEBOUNCE_MS) {
        const uint32_t acceptedTime = rawButtonSince + BUTTON_DEBOUNCE_MS;
        if (rawButtonState == LOW) {
            buttonState = LOW;
            pressStartTime = acceptedTime;
            longPressTriggered = false;
            ++buttonEvents;
        } else {
            checkLongPress(acceptedTime);
            buttonState = HIGH;
            // A long press must not produce an additional short-click release.
            if (!longPressTriggered) ++releaseEvents;
        }
    }
    checkLongPress(time);
}

void FrSkyEncoder::update() {
    // All public methods, including update(), must run on one task.
    portENTER_CRITICAL(&inputMux);
    const uint32_t movedLeft = pendingLeft;
    const uint32_t movedRight = pendingRight;
    pendingLeft = 0;
    pendingRight = 0;
    portEXIT_CRITICAL(&inputMux);
    queuedLeft += movedLeft;
    queuedRight += movedRight;
    encoderPosition += (int32_t)movedRight - (int32_t)movedLeft;

    // Snapshot the head so continuous bounce cannot keep update() spinning.
    portENTER_CRITICAL(&inputMux);
    const uint8_t end = buttonHead;
    const uint32_t now = millis();
    const bool overflow = buttonOverflow;
    buttonOverflow = false;
    portEXIT_CRITICAL(&inputMux);
    while (true) {
        portENTER_CRITICAL(&inputMux);
        if (buttonTail == end) {
            portEXIT_CRITICAL(&inputMux);
            break;
        }
        const uint32_t time = buttonTimes[buttonTail];
        const bool level = buttonLevels[buttonTail];
        buttonTail = (buttonTail + 1) % BUTTON_QUEUE_SIZE;
        portEXIT_CRITICAL(&inputMux);
        settleButton(time);
        if (level != rawButtonState) {
            rawButtonState = level;
            rawButtonSince = time;
        }
    }
    if (overflow) {
        // Recover the physical level after excessive noise/long loop blocking.
        rawButtonState = digitalRead(pinSW);
        rawButtonSince = now;
    }
    settleButton(now);
}

bool FrSkyEncoder::left() {
    if (queuedLeft == 0) return false;
    --queuedLeft;
    return true;
}

bool FrSkyEncoder::right() {
    if (queuedRight == 0) return false;
    --queuedRight;
    return true;
}

bool FrSkyEncoder::pressed() {
    if (buttonEvents == 0) return false;
    --buttonEvents;
    return true;
}

bool FrSkyEncoder::longPressed() {
    if (longPressEvents == 0) return false;
    --longPressEvents;
    return true;
}

bool FrSkyEncoder::released() {
    if (releaseEvents == 0) return false;
    --releaseEvents;
    return true;
}

bool FrSkyEncoder::isPressed() {
    return buttonState == LOW;
}

unsigned long FrSkyEncoder::pressDuration() {
    return buttonState == LOW ? (uint32_t)(millis() - pressStartTime) : 0;
}

void FrSkyEncoder::setLongPressTime(uint32_t time) {
    longPressTime = time;
}

uint32_t FrSkyEncoder::getLongPressTime() {
    return longPressTime;
}

int FrSkyEncoder::position() {
    return encoderPosition;
}

void FrSkyEncoder::reset() {
    portENTER_CRITICAL(&inputMux);
    pendingLeft = 0;
    pendingRight = 0;
    previousAB = (gpio_get_level((gpio_num_t)pinA) << 1) | gpio_get_level((gpio_num_t)pinB);
    partialStep = 0;
    rotarySynced = previousAB == 0 || previousAB == 3;
    portEXIT_CRITICAL(&inputMux);
    queuedLeft = 0;
    queuedRight = 0;
    encoderPosition = 0;
}
