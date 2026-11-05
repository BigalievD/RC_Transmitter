#include "LogicalSwitchManager.h"
#include "Source.h"
#include "ModelInputProcessor.h"

LogicalSwitchManager logicalSwitchManager;

void LogicalSwitchManager::begin(){
    reset();
}
void LogicalSwitchManager::reset(){
    currentModel=nullptr;
    phase=EvaluationPhase::PRE;
    for(uint8_t i=0;i<MAX_LOGICAL_SWITCHES;i++){
        states[i]=false;
        preFlight[i]=false;
        evaluating[i]=false;
        evaluated[i]=false;
    }
}
void LogicalSwitchManager::updatePre(const ModelConfig &model){
    currentModel = &model;
    phase = EvaluationPhase::PRE;
    bool visiting[MAX_LOGICAL_SWITCHES] = {};
    for (uint8_t i = 0; i < MAX_LOGICAL_SWITCHES; i++)
        preFlight[i] = determinePreFlight(i, visiting);
    for (uint8_t i = 0; i < MAX_LOGICAL_SWITCHES; i++){
        evaluating[i] = false;
        evaluated[i] = false;
        if (!preFlight[i])
            states[i] = false;
    }
    for (uint8_t i = 0; i < MAX_LOGICAL_SWITCHES; i++){
        if (preFlight[i])
            states[i] = evaluateSwitch(i);
    }
    currentModel = nullptr;
}
void LogicalSwitchManager::updatePost(const ModelConfig &model){
    currentModel = &model;
    phase = EvaluationPhase::POST;
    for (uint8_t i = 0; i < MAX_LOGICAL_SWITCHES; i++){
        evaluating[i] = false;
        evaluated[i] = false;
    }
    for (uint8_t i = 0; i < MAX_LOGICAL_SWITCHES; i++)
        states[i] = evaluateSwitch(i);
    currentModel = nullptr;
}
bool LogicalSwitchManager::getState(uint8_t index) const{
    if (index >= MAX_LOGICAL_SWITCHES) return false;
    return states[index];
}
int16_t LogicalSwitchManager::getValue(uint8_t index) const{
    if (index >= MAX_LOGICAL_SWITCHES) return 0;
    return states[index] ? 1000 : -1000;
}
bool LogicalSwitchManager::isPreFlight(uint8_t index) const{
    if (index >= MAX_LOGICAL_SWITCHES) return false;
    return preFlight[index];
}
bool LogicalSwitchManager::determinePreFlight(uint8_t index,bool visiting[]){
    if (!currentModel) return false;
    if (index >= MAX_LOGICAL_SWITCHES) return false;
    const LogicalSwitchConfig &cfg = currentModel->logicalSwitches[index];
    if (!cfg.enabled) return false;
    if (visiting[index]) return false;
    visiting[index] = true;
    bool result = conditionIsPreFlight(cfg.conditionA, visiting);
    if (result){
        switch (cfg.operation){
            case LogicalSwitchOperation::CONDITION:
            case LogicalSwitchOperation::NOT:
                break;
            case LogicalSwitchOperation::AND:
            case LogicalSwitchOperation::OR:
            case LogicalSwitchOperation::XOR:
                result = conditionIsPreFlight(cfg.conditionB, visiting);
                break;
        }
    }
    visiting[index] = false;
    return result;
}
bool LogicalSwitchManager::conditionIsPreFlight(const ConditionConfig &condition,bool visiting[]){
    if (!condition.isValid()) return false;
    if (!condition.sourceA.isValid()) return false;
    auto sourceIsPreFlight = [&](const SourceId &source)->bool{
        switch (source.type){
            case SourceType::NONE:
                return false;
            case SourceType::MODEL_INPUT:
            case SourceType::CHANNEL_OUTPUT:
                return false;
            case SourceType::LOGICAL_SWITCH:
                return determinePreFlight(source.index, visiting);
            case SourceType::STICK_FUNCTION:
            case SourceType::ANALOG_INPUT:
            case SourceType::SWITCH_INPUT:
            case SourceType::SENSOR_INPUT:
            case SourceType::CONSTANT_VALUE:
                return true;
        }

        return false;
    };

    if (!sourceIsPreFlight(condition.sourceA))
        return false;
    if (condition.type == ConditionType::SOURCE_ABOVE_SOURCE ||
        condition.type == ConditionType::SOURCE_BELOW_SOURCE){
        if (!condition.sourceB.isValid())
            return false;
        if (!sourceIsPreFlight(condition.sourceB))
            return false;
    }
    return true;
}
bool LogicalSwitchManager::evaluateSwitch(uint8_t index){
    if (!currentModel) return false;
    if (index >= MAX_LOGICAL_SWITCHES) return false;
    if (phase == EvaluationPhase::PRE && !preFlight[index])
        return false;
    if (evaluated[index])
        return states[index];
    if (evaluating[index])
        return false;
    const LogicalSwitchConfig &cfg = currentModel->logicalSwitches[index];
    if (!cfg.enabled){
        states[index] = false;
        evaluated[index] = true;
        return false;
    }
    evaluating[index] = true;
    bool a = evaluateCondition(cfg.conditionA);
    bool result = false;
    switch (cfg.operation){
        case LogicalSwitchOperation::CONDITION:
            result = a;
            break;
        case LogicalSwitchOperation::AND:
            result = a && evaluateCondition(cfg.conditionB);
            break;
        case LogicalSwitchOperation::OR:
            result = a || evaluateCondition(cfg.conditionB);
            break;
        case LogicalSwitchOperation::XOR:
            result = a != evaluateCondition(cfg.conditionB);
            break;
        case LogicalSwitchOperation::NOT:
            result = !a;
            break;
    }
    evaluating[index] = false;
    evaluated[index] = true;
    states[index] = result;
    return result;
}
int16_t LogicalSwitchManager::resolveSource(const SourceId &source){
    if (!source.isValid()) return 0;
    if (source.type == SourceType::MODEL_INPUT){
        if (phase == EvaluationPhase::PRE)
            return 0;
        return modelInputProcessor.getValue(source.index);
    }
    if (source.type == SourceType::LOGICAL_SWITCH){
        if (source.index >= MAX_LOGICAL_SWITCHES)
            return 0;
        if (phase == EvaluationPhase::PRE && !preFlight[source.index])
            return 0;
        return evaluateSwitch(source.index) ? 1000 : -1000;
    }
    return sourceResolver.getValue(source);
}

bool LogicalSwitchManager::evaluateCondition(const ConditionConfig &condition){
    if (!condition.isValid()) return false;
    if (!condition.sourceA.isValid()) return false;
    int32_t a = resolveSource(condition.sourceA);
    bool result = false;
    switch (condition.type){
        case ConditionType::SOURCE_POSITIVE:
            result = a > 0;
            break;
        case ConditionType::SOURCE_NEGATIVE:
            result = a < 0;
            break;
        case ConditionType::SOURCE_ABOVE:
            result = a > condition.value1;
            break;
        case ConditionType::SOURCE_BELOW:
            result = a < condition.value1;
            break;
        case ConditionType::SOURCE_EQUAL:{
            int32_t tolerance = condition.tolerance;
            if (tolerance < 0) tolerance = -tolerance;
            int32_t difference = a - condition.value1;
            if (difference < 0) difference = -difference;
            result = difference <= tolerance;
            break;
        }
        case ConditionType::SOURCE_RANGE:{
            int32_t low = min((int32_t)condition.value1, (int32_t)condition.value2);
            int32_t high = max((int32_t)condition.value1, (int32_t)condition.value2);
            result = a >= low && a <= high;
            break;
        }
        case ConditionType::SOURCE_ABOVE_SOURCE:
            if (!condition.sourceB.isValid())
                return false;
            result = a > resolveSource(condition.sourceB);
            break;
        case ConditionType::SOURCE_BELOW_SOURCE:
            if (!condition.sourceB.isValid())
                return false;
            result = a < resolveSource(condition.sourceB);
            break;
        case ConditionType::NONE:
        default:
            return false;
    }
    return condition.inverted ? !result : result;
}