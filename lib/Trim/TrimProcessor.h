#pragma once
#include <Arduino.h>
#include "ModelConfig.h"

class TrimProcessor {
public:
    int16_t process(int16_t value,const ModelConfig &model,int8_t trimIndex,uint8_t flightModeIndex = 0) const;
    int16_t getValue(const ModelConfig &model,int8_t trimIndex,uint8_t flightModeIndex = 0) const;
    bool setValue(ModelConfig &model,uint8_t trimIndex,int16_t value,uint8_t flightModeIndex = 0) const;
    bool changeValue(ModelConfig &model,uint8_t trimIndex,int8_t direction,uint8_t flightModeIndex = 0) const;
};

extern TrimProcessor trimProcessor;