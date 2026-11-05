#pragma once
#include <Arduino.h>
#include "ModelConfig.h"
class ModelRuntime{
public:
    void begin();
    void reset();
    void setActiveFlightMode(uint8_t mode);
    uint8_t getActiveFlightMode() const;
    uint32_t getGeneration() const;
    bool isInitialized() const;
private:
    uint8_t activeFlightMode=0;
    uint32_t generation=0;
    bool initialized=false;
};
extern ModelRuntime modelRuntime;