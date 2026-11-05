#pragma once
#include <Arduino.h>
#include "ModelConfig.h"
class OutputProcessor{
public:
    void process(const ModelConfig &model,const int16_t *mixerValues);
    int16_t getChannelUs(uint8_t channel) const;
    int16_t getChannelNormalized(uint8_t channel) const;
    const int16_t *getChannelsUs() const;
    int16_t calculateUs(int16_t normalizedValue,const OutputConfig &config) const;
    int16_t calculateUs(int16_t normalizedValue,const OutputConfig &config,const ModelConfig &model) const;
    void reset();
    void setBalancePositionOverride(uint8_t channel,bool enabled,int16_t position);
    void clearBalancePositionOverrides();
private:
    int16_t outputUs[MAX_MODEL_CHANNELS]={};
    int16_t outputNormalized[MAX_MODEL_CHANNELS]={};
    float slowValues[MAX_MODEL_CHANNELS]={};
    uint32_t slowLastTime[MAX_MODEL_CHANNELS]={};
    bool slowInitialized[MAX_MODEL_CHANNELS]={};
    bool balancePositionOverrideEnabled[MAX_MODEL_CHANNELS]={};
    int16_t balancePositionOverride[MAX_MODEL_CHANNELS]={};
    void processChannel(uint8_t channel,int16_t mixerValue,const OutputConfig &config,const ModelConfig &model,uint32_t now);
    int16_t applyCurve(int16_t value,const OutputConfig &config,const ModelConfig &model) const;
    int16_t applySlow(uint8_t channel,int16_t target,const OutputConfig &config,uint32_t now);
    int16_t finishNormalized(int16_t value,const OutputConfig &config) const;
    int16_t normalizedToUs(int16_t normalizedValue,const OutputConfig &config) const;
};
extern OutputProcessor outputProcessor;
