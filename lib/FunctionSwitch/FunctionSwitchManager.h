#pragma once
#include <Arduino.h>

static constexpr uint8_t FUNCTION_SWITCH_COUNT=6;

class FunctionSwitchManager{
public:
    void reset();
    void setState(uint8_t index,bool state);
    void toggle(uint8_t index);
    bool getState(uint8_t index) const;
    int16_t getValue(uint8_t index) const;

private:
    bool states[FUNCTION_SWITCH_COUNT]={};
};

extern FunctionSwitchManager functionSwitchManager;
