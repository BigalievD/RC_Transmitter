#pragma once
#include <Arduino.h>
#include "ModelConfig.h"

class LogicalSwitchManager{
public:
    void begin();
    void reset();
    void updatePre(const ModelConfig &model);
    void updatePost(const ModelConfig &model);
    bool getState(uint8_t index) const;
    int16_t getValue(uint8_t index) const;
    bool isPreFlight(uint8_t index) const;

private:
    enum class EvaluationPhase : uint8_t{
        PRE = 0,
        POST
    };
    bool states[MAX_LOGICAL_SWITCHES] = {};
    bool preFlight[MAX_LOGICAL_SWITCHES] = {};
    bool evaluating[MAX_LOGICAL_SWITCHES] = {};
    bool evaluated[MAX_LOGICAL_SWITCHES] = {};
    const ModelConfig *currentModel = nullptr;
    EvaluationPhase phase = EvaluationPhase::PRE;
    bool evaluateSwitch(uint8_t index);
    bool evaluateCondition(const ConditionConfig &condition);
    int16_t resolveSource(const SourceId &source);
    bool determinePreFlight(uint8_t index,bool visiting[]);
    bool conditionIsPreFlight(const ConditionConfig &condition,bool visiting[]);
};

extern LogicalSwitchManager logicalSwitchManager;