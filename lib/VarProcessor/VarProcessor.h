#pragma once
#include <Arduino.h>
#include "ModelConfig.h"

// Runtime values use 0.1% units. Mutable values live in RAM and are not
// part of ModelConfig: storing one Var Action never causes a Flash write.
class VarProcessor{
public:
    void process(ModelConfig &model);
    int16_t getValue(uint8_t index) const;
    int16_t getSlotValue(uint8_t index,uint8_t slot) const;
    int16_t evaluateBase(const VarConfig &var) const;
    void reset();
    void servicePersistence();
private:
    struct ActionState{
        bool wasActive=false;
        uint32_t activeSince=0;
        uint32_t lastRepeat=0;
    };
    int16_t values[MAX_MODEL_VARS]={};
    int16_t slotValues[MAX_MODEL_VARS][MAX_VAR_VALUES]={};
    int16_t configuredValues[MAX_MODEL_VARS][MAX_VAR_VALUES]={};
    bool initialized[MAX_MODEL_VARS]={};
    ActionState actionStates[MAX_MODEL_VARS][MAX_VAR_ACTIONS]={};
    uint16_t currentModelId=0;
    bool valuesNeedSave=false;
    bool saveAtRangeLimit=false;
    uint32_t lastValueChange=0;
    uint32_t lastValueCheckpoint=0;
};
extern VarProcessor varProcessor;
