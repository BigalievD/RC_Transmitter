#include "TrimProcessor.h"

TrimProcessor trimProcessor;

int16_t TrimProcessor::process(int16_t value,const ModelConfig &model,int8_t trimIndex,uint8_t flightModeIndex) const{
    int32_t result = value;
    result += getValue(model, trimIndex, flightModeIndex);
    return (int16_t)constrain(result, -1500L, 1500L);
}
int16_t TrimProcessor::getValue(const ModelConfig &model,int8_t trimIndex,uint8_t flightModeIndex) const{
    if (trimIndex < 0 || trimIndex >= MAX_MODEL_TRIMS)
        return 0;
    if (flightModeIndex >= MAX_FLIGHT_MODES)
        return 0;
    const TrimConfig &trim = model.trims[trimIndex];
    if (!trim.enabled)
        return 0;
    int16_t minValue = min(trim.min, trim.max);
    int16_t maxValue = max(trim.min, trim.max);
    return (int16_t)constrain((int32_t)model.flightModes[flightModeIndex].trims[trimIndex], (int32_t)minValue, (int32_t)maxValue);
}
bool TrimProcessor::setValue(ModelConfig &model,uint8_t trimIndex,int16_t value,uint8_t flightModeIndex) const{
    if (trimIndex >= MAX_MODEL_TRIMS || flightModeIndex >= MAX_FLIGHT_MODES)
        return false;
    TrimConfig &trim = model.trims[trimIndex];
    if (!trim.enabled)
        return false;
    int16_t minValue = min(trim.min, trim.max);
    int16_t maxValue = max(trim.min, trim.max);
    model.flightModes[flightModeIndex].trims[trimIndex] = (int16_t)constrain((int32_t)value, (int32_t)minValue, (int32_t)maxValue);
    return true;
}
bool TrimProcessor::changeValue(ModelConfig &model,uint8_t trimIndex,int8_t direction,uint8_t flightModeIndex) const{
    if (trimIndex >= MAX_MODEL_TRIMS || flightModeIndex >= MAX_FLIGHT_MODES || direction == 0)
        return false;
    const TrimConfig &trim = model.trims[trimIndex];
    if (!trim.enabled)
        return false;
    int32_t value = model.flightModes[flightModeIndex].trims[trimIndex];
    if (direction > 0)
        value += trim.step;
    else
        value -= trim.step;
    return setValue(model, trimIndex, (int16_t)value, flightModeIndex);
}
