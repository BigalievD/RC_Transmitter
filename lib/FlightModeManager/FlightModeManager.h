#pragma once
#include <Arduino.h>
#include "ModelConfig.h"
class FlightModeManager{
public:
    void begin();
    void update(const ModelConfig &model);
    uint8_t getActiveMode() const;
    void forceMode(int8_t mode);
    void clearForcedMode();
    void reset();
private:
    int8_t forcedMode=-1;
};
extern FlightModeManager flightModeManager;