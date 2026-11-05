#pragma once
#include <Arduino.h>

enum class SystemEventId : uint8_t{
    THROTTLE_HOLD=0,
    THROTTLE_CUT,
    THROTTLE_ACTIVE,
    TELEMETRY_ACTIVE,
    RSSI_LOW,
    TRAINER_ACTIVE,
    FLIGHT_RESET,
    THROTTLE_IN_DEAD_BAND,
    COUNT
};

class SystemEventManager{
public:
    void reset();

    // Update events that can already be derived from the two connected sticks.
    void updateFromThrottle(int16_t throttleValue);

    // Runtime flags for subsystems that will be connected later.
    void setState(SystemEventId event,bool state);
    bool getState(SystemEventId event) const;

    // One-cycle/pulse event.
    void triggerFlightReset();
    void endCycle();

    void setThrottleActiveThreshold(int16_t value);
    void setThrottleDeadBand(int16_t value);

private:
    bool states[(uint8_t)SystemEventId::COUNT]={};
    int16_t throttleActiveThreshold=-950;
    int16_t throttleDeadBand=50;
};

extern SystemEventManager systemEventManager;
