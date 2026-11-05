#pragma once
#include <Arduino.h>
#include "ModelConfig.h"

class ConditionEvaluator{
public:
    bool evaluate(const ConditionConfig &condition) const;
    bool evaluatePreFlight(const ConditionConfig &condition) const;

private:
    int16_t resolveSource(const SourceId &source) const;
    int16_t resolvePreFlightSource(const SourceId &source,bool &valid) const;
};

extern ConditionEvaluator conditionEvaluator;