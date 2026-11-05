#pragma once
#include <Arduino.h>
#include "InputManager.h"
enum class SourceType:uint8_t{
    NONE=0,
    STICK_FUNCTION,
    MODEL_INPUT,
    ANALOG_INPUT,
    SWITCH_INPUT,
    SENSOR_INPUT,
    LOGICAL_SWITCH,
    CHANNEL_OUTPUT,
    CONSTANT_VALUE,
    VAR // Appended to preserve persisted SourceType numeric IDs.
};
struct SourceId{
    SourceType type=SourceType::NONE;
    uint8_t index=0;
    constexpr SourceId()=default;
    constexpr SourceId(SourceType sourceType,uint8_t sourceIndex=0):type(sourceType),index(sourceIndex){}
    bool isValid() const{
        return type!=SourceType::NONE;
    }
    bool operator==(const SourceId &other) const{
        return type==other.type&&index==other.index;
    }
    bool operator!=(const SourceId &other) const{
        return !(*this==other);
    }
};
enum class SensorSource:uint8_t{
    ROLL=0,
    PITCH,
    YAW,
    GYRO_X,
    GYRO_Y,
    GYRO_Z,
    ACCEL_X,
    ACCEL_Y,
    ACCEL_Z,
    COUNT
};
namespace Sources{
    inline SourceId stick(StickFunction function){
        return SourceId(SourceType::STICK_FUNCTION,static_cast<uint8_t>(function));
    }
    inline SourceId modelInput(uint8_t index){
        return SourceId(SourceType::MODEL_INPUT,index);
    }
    inline SourceId analog(AnalogInput input){
        return SourceId(SourceType::ANALOG_INPUT,static_cast<uint8_t>(input));
    }
    inline SourceId switchInput(SwitchInput input){
        return SourceId(SourceType::SWITCH_INPUT,static_cast<uint8_t>(input));
    }
    inline SourceId sensor(SensorSource sensor){
        return SourceId(SourceType::SENSOR_INPUT,static_cast<uint8_t>(sensor));
    }
    inline SourceId logicalSwitch(uint8_t index){
        return SourceId(SourceType::LOGICAL_SWITCH,index);
    }
    inline SourceId channel(uint8_t channelIndex){
        return SourceId(SourceType::CHANNEL_OUTPUT,channelIndex);
    }
    inline SourceId constant(){
        return SourceId(SourceType::CONSTANT_VALUE,0);
    }
}
class SourceResolver{
public:
    int16_t getValue(const SourceId &source,int16_t constantValue=0) const;
    const char *getName(const SourceId &source) const;
    void reloadHardwareAnalogNames();
};
extern SourceResolver sourceResolver;