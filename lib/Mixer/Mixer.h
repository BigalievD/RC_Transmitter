#pragma once
#include <Arduino.h>
#include "ModelConfig.h"
#include "Source.h"

class Mixer {
public:
    void process(const ModelConfig &model);
    int16_t getChannel(uint8_t channel) const;
    const int16_t *getChannels() const;
    int16_t getMixValue(uint8_t mixIndex) const;
    void reset();
    void resetRuntime();
    void setInputLock(bool locked);
    bool isInputLocked() const;
    bool hasChannelCycle() const;
private:
    enum class ChannelState:uint8_t{UNVISITED=0,PROCESSING,DONE,INVALID};
    int16_t channelValues[MAX_MODEL_CHANNELS]={};
    int16_t lockedChannelValues[MAX_MODEL_CHANNELS]={};
    int16_t mixValues[MAX_MIX_LINES]={};
    // One Mix result per processing frame, shared by all of its Outputs.
    // Never key the cache by Source: duplicate Sources are independent Mixes.
    int32_t frameMixValues[MAX_MIX_LINES]={};
    bool frameMixCalculated[MAX_MIX_LINES]={};
    bool frameMixActive[MAX_MIX_LINES]={};
    bool frameMixInvalid[MAX_MIX_LINES]={};
    ChannelState channelStates[MAX_MODEL_CHANNELS]={};
    bool channelCycleDetected=false;
    bool inputLockEnabled=false;
    float slowValues[MAX_MIX_LINES]={};
    uint32_t slowLastTime[MAX_MIX_LINES]={};
    bool slowInitialized[MAX_MIX_LINES]={};

    float actionSlowValues[MAX_MIX_LINES][MAX_MIX_ACTIONS]={};
    uint32_t actionSlowLastTime[MAX_MIX_LINES][MAX_MIX_ACTIONS]={};
    bool actionSlowInitialized[MAX_MIX_LINES][MAX_MIX_ACTIONS]={};
    bool delayInitialized[MAX_MIX_LINES]={};
    bool delayOutputState[MAX_MIX_LINES]={};
    bool delayPending[MAX_MIX_LINES]={};
    bool delayPendingState[MAX_MIX_LINES]={};
    uint32_t delayStartTime[MAX_MIX_LINES]={};
    uint8_t currentChannelCount=0;
    uint32_t currentProcessTime=0;
    const ModelConfig *currentModel=nullptr;
    int16_t resolveSource(const SourceId &source,const ModelConfig &model);
    bool sourceIsInvalid(const SourceId &source) const;
    bool checkCondition(const MixLine &mix,const ModelConfig &model) const;
    bool checkFlightMode(const MixLine &mix) const;
    bool applyDelay(uint8_t mixIndex,bool requestedState,const MixLine &mix,uint32_t now);
    int32_t applySlow(uint8_t mixIndex,int32_t target,const MixLine &mix,uint32_t now);
    int32_t applyActionSlow(uint8_t mixIndex,uint8_t actionIndex,int32_t target,uint16_t slowUp,uint16_t slowDown,uint32_t now);
    int32_t applyDifferential(int32_t value,int16_t differential) const;
    bool actionConditionActive(const MixAction &action,int32_t sourceValue) const;
    const MixActionVariant *selectActionVariant(const MixAction &action,int32_t sourceValue) const;
    int32_t applyActions(uint8_t mixIndex,int32_t value,const MixLine &mix,const ModelConfig &model,uint32_t now);
    void resetRuntime(uint8_t mixIndex);
    int16_t calculateChannel(uint8_t channel);   
};

extern Mixer mixer;