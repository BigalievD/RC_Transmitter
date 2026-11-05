#pragma once
#include <Arduino.h>
#include "ModelConfig.h"

struct FailsafeChannelState{
    FailsafeMode mode=FailsafeMode::HOLD;
    int16_t valueUs=1500;
};

class FailsafeProcessor{
public:
    void process(const ModelConfig &model);
    FailsafeMode getMode(uint8_t channel) const;
    int16_t getValueUs(uint8_t channel) const;
    const FailsafeChannelState &getChannel(uint8_t channel) const;
    const FailsafeChannelState *getChannels() const;
    void reset();
private:
    FailsafeChannelState channels[MAX_MODEL_CHANNELS]={};
};

extern FailsafeProcessor failsafeProcessor;