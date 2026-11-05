#ifndef FRSKY_ENCODER_H
#define FRSKY_ENCODER_H

#include <Arduino.h>

class FrSkyEncoder {
public:
    FrSkyEncoder(uint8_t pinA, uint8_t pinB, uint8_t pinSW);
    void begin();
    void update();
    bool left();
    bool right();
    bool pressed();
    bool longPressed();
    bool released();
    bool isPressed();
    unsigned long pressDuration();
    void setLongPressTime(uint32_t time);
    uint32_t getLongPressTime();
    int position();
    void reset();

private:
    uint8_t pinA, pinB, pinSW;
    portMUX_TYPE inputMux = portMUX_INITIALIZER_UNLOCKED;
    uint8_t previousAB = 0;
    int8_t partialStep = 0;
    bool rotarySynced = false;
    volatile uint32_t pendingLeft = 0;
    volatile uint32_t pendingRight = 0;
    uint32_t queuedLeft = 0;
    uint32_t queuedRight = 0;
    int32_t encoderPosition = 0;

    // Interrupts capture raw edges; update() debounces their timestamps.
    static constexpr uint8_t BUTTON_QUEUE_SIZE = 64;
    static constexpr uint32_t BUTTON_DEBOUNCE_MS = 15;
    volatile uint32_t buttonTimes[BUTTON_QUEUE_SIZE] = {};
    volatile uint8_t buttonLevels[BUTTON_QUEUE_SIZE] = {};
    volatile uint8_t buttonHead = 0;
    volatile uint8_t buttonTail = 0;
    volatile bool buttonOverflow = false;
    bool rawButtonState = HIGH;
    bool buttonState = HIGH;
    uint32_t rawButtonSince = 0;
    uint32_t pressStartTime = 0;
    uint32_t buttonEvents = 0;
    uint32_t longPressEvents = 0;
    uint32_t releaseEvents = 0;
    bool longPressTriggered = false;
    uint32_t longPressTime = 800;

    static void ARDUINO_ISR_ATTR encoderISR(void *arg);
    static void ARDUINO_ISR_ATTR buttonISR(void *arg);
    void ARDUINO_ISR_ATTR handleEncoderISR();
    void ARDUINO_ISR_ATTR handleButtonISR();
    void settleButton(uint32_t time);
    void checkLongPress(uint32_t time);
};

#endif
