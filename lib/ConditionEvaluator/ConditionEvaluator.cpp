#include "ConditionEvaluator.h"
#include "Source.h"
#include "ModelInputProcessor.h"
#include "LogicalSwitchManager.h"
#include "FlightModeManager.h"
#include "FunctionSwitchManager.h"
#include "TrimSwitchManager.h"
#include "SystemEventManager.h"

ConditionEvaluator conditionEvaluator;

int16_t ConditionEvaluator::resolveSource(const SourceId &source) const{
    if (!source.isValid()) return 0;
    if (source.type == SourceType::MODEL_INPUT) return modelInputProcessor.getValue(source.index);
    if (source.type == SourceType::LOGICAL_SWITCH) return logicalSwitchManager.getValue(source.index);
    return sourceResolver.getValue(source);
}
int16_t ConditionEvaluator::resolvePreFlightSource(const SourceId &source,bool &valid) const{
    valid = false;
    if (!source.isValid()) return 0;
    switch (source.type){
        case SourceType::MODEL_INPUT:
        case SourceType::CHANNEL_OUTPUT:
            return 0;
        case SourceType::LOGICAL_SWITCH:
            if (!logicalSwitchManager.isPreFlight(source.index)) return 0;
            valid = true;
            return logicalSwitchManager.getValue(source.index);
        case SourceType::STICK_FUNCTION:
        case SourceType::ANALOG_INPUT:
        case SourceType::SWITCH_INPUT:
        case SourceType::SENSOR_INPUT:
        case SourceType::CONSTANT_VALUE:
            valid = true;
            return sourceResolver.getValue(source);
        case SourceType::NONE:
        default:
            return 0;
    }
}
bool ConditionEvaluator::evaluate(const ConditionConfig &c) const{
    if (c.type == ConditionType::ALWAYS)
        return c.inverted ? false : true;

    if (c.type == ConditionType::NEVER)
        return c.inverted ? true : false;

    if (c.type == ConditionType::FLIGHT_MODE){
        bool result =
            c.value1 >= 0 &&
            c.value1 < MAX_FLIGHT_MODES &&
            flightModeManager.getActiveMode() == (uint8_t)c.value1;

        return c.inverted ? !result : result;
    }

    if (c.type == ConditionType::FUNCTION_SWITCH){
        bool result =
            c.value1 >= 0 &&
            c.value1 < FUNCTION_SWITCH_COUNT &&
            functionSwitchManager.getState((uint8_t)c.value1);

        return c.inverted ? !result : result;
    }

    if (c.type == ConditionType::TRIM_POSITION){
        bool result =
            c.value1 >= 0 &&
            c.value1 < (int16_t)TrimDirection::COUNT &&
            trimSwitchManager.getState((TrimDirection)c.value1);

        return c.inverted ? !result : result;
    }

    if (c.type == ConditionType::SYSTEM_EVENT){
        bool result =
            c.value1 >= 0 &&
            c.value1 < (int16_t)SystemEventId::COUNT &&
            systemEventManager.getState((SystemEventId)c.value1);

        return c.inverted ? !result : result;
    }

    if (!c.isValid() || !c.sourceA.isValid()) return false;

    int32_t a = resolveSource(c.sourceA);
    bool result = false;
    switch (c.type){
        case ConditionType::SOURCE_POSITIVE:
            result = a > 0;
            break;
        case ConditionType::SOURCE_NEGATIVE:
            result = a < 0;
            break;
        case ConditionType::SOURCE_ABOVE:
            result = a > c.value1;
            break;
        case ConditionType::SOURCE_BELOW:
            result = a < c.value1;
            break;
        case ConditionType::SOURCE_EQUAL:{
            int32_t t = abs((int32_t)c.tolerance);
            result = abs(a - (int32_t)c.value1) <= t;
            break;
        }
        case ConditionType::SOURCE_RANGE:{
            int32_t lo = min((int32_t)c.value1, (int32_t)c.value2);
            int32_t hi = max((int32_t)c.value1, (int32_t)c.value2);
            result = a >= lo && a <= hi;
            break;
        }
        case ConditionType::SOURCE_ABOVE_SOURCE:
            if (!c.sourceB.isValid()) return false;
            result = a > resolveSource(c.sourceB);
            break;
        case ConditionType::SOURCE_BELOW_SOURCE:
            if (!c.sourceB.isValid()) return false;
            result = a < resolveSource(c.sourceB);
            break;
        case ConditionType::ALWAYS:
            result = true;
            break;
        case ConditionType::NEVER:
            result = false;
            break;
        case ConditionType::FLIGHT_MODE:
            result =
                c.value1 >= 0 &&
                c.value1 < MAX_FLIGHT_MODES &&
                flightModeManager.getActiveMode() == (uint8_t)c.value1;
            break;
        case ConditionType::NONE:
        default:
            return false;
    }
    return c.inverted ? !result : result;
}
bool ConditionEvaluator::evaluatePreFlight(const ConditionConfig &c) const{
    if (c.type == ConditionType::ALWAYS)
        return c.inverted ? false : true;

    if (c.type == ConditionType::NEVER)
        return c.inverted ? true : false;

    // Flight modes themselves are resolved before Mix processing.
    // A flight mode must not depend on the currently active flight mode.
    if (c.type == ConditionType::FLIGHT_MODE)
        return false;

    if (c.type == ConditionType::FUNCTION_SWITCH){
        bool result =
            c.value1 >= 0 &&
            c.value1 < FUNCTION_SWITCH_COUNT &&
            functionSwitchManager.getState((uint8_t)c.value1);

        return c.inverted ? !result : result;
    }

    if (c.type == ConditionType::TRIM_POSITION){
        bool result =
            c.value1 >= 0 &&
            c.value1 < (int16_t)TrimDirection::COUNT &&
            trimSwitchManager.getState((TrimDirection)c.value1);

        return c.inverted ? !result : result;
    }

    if (c.type == ConditionType::SYSTEM_EVENT){
        bool result =
            c.value1 >= 0 &&
            c.value1 < (int16_t)SystemEventId::COUNT &&
            systemEventManager.getState((SystemEventId)c.value1);

        return c.inverted ? !result : result;
    }

    if (!c.isValid() || !c.sourceA.isValid()) return false;

    bool validA = false;
    int32_t a = resolvePreFlightSource(c.sourceA, validA);
    if (!validA) return false;
    bool result = false;
    switch (c.type){
        case ConditionType::SOURCE_POSITIVE:
            result = a > 0;
            break;
        case ConditionType::SOURCE_NEGATIVE:
            result = a < 0;
            break;
        case ConditionType::SOURCE_ABOVE:
            result = a > c.value1;
            break;
        case ConditionType::SOURCE_BELOW:
            result = a < c.value1;
            break;
        case ConditionType::SOURCE_EQUAL:{
            int32_t t = abs((int32_t)c.tolerance);
            result = abs(a - (int32_t)c.value1) <= t;
            break;
        }
        case ConditionType::SOURCE_RANGE:{
            int32_t lo = min((int32_t)c.value1, (int32_t)c.value2);
            int32_t hi = max((int32_t)c.value1, (int32_t)c.value2);
            result = a >= lo && a <= hi;
            break;
        }
        case ConditionType::SOURCE_ABOVE_SOURCE:{
            if (!c.sourceB.isValid()) return false;
            bool validB = false;
            int32_t b = resolvePreFlightSource(c.sourceB, validB);
            if (!validB) return false;
            result = a > b;
            break;
        }
        case ConditionType::SOURCE_BELOW_SOURCE:{
            if (!c.sourceB.isValid()) return false;
            bool validB = false;
            int32_t b = resolvePreFlightSource(c.sourceB, validB);
            if (!validB) return false;
            result = a < b;
            break;
        }
        case ConditionType::ALWAYS:
            result = true;
            break;
        case ConditionType::NEVER:
            result = false;
            break;
        case ConditionType::FLIGHT_MODE:
            return false;
        case ConditionType::NONE:
        default:
            return false;
    }
    return c.inverted ? !result : result;
}