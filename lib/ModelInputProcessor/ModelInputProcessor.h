#pragma once
#include <Arduino.h>
#include "ModelConfig.h"
class ModelInputProcessor{
public:
    void process(const ModelConfig &model);
    int16_t getValue(uint8_t index) const;
    void reset();
private:
    int16_t values[MAX_MODEL_INPUTS]={};
};
extern ModelInputProcessor modelInputProcessor;
