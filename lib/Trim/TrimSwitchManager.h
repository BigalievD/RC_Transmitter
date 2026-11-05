#pragma once
#include <Arduino.h>

enum class TrimDirection : uint8_t{
    T1_LEFT=0,
    T1_RIGHT,
    T2_DOWN,
    T2_UP,
    T3_DOWN,
    T3_UP,
    T4_LEFT,
    T4_RIGHT,
    COUNT
};

class TrimSwitchManager{
public:
    void reset();

    // Runtime state of a trim direction.
    // Later these calls can be fed from GPIO/MCP23017/etc.
    void setState(TrimDirection direction,bool state);
    bool getState(TrimDirection direction) const;

    int16_t getValue(TrimDirection direction) const;

private:
    bool states[(uint8_t)TrimDirection::COUNT]={};
};

extern TrimSwitchManager trimSwitchManager;
