#include "Source.h"
#include "LogicalSwitchManager.h"
#include "VarProcessor.h"
#include <Preferences.h>
SourceResolver sourceResolver;

// Hardware text fields: S1..S4, Ext1..Ext2, L1..L2, Slider1..Slider2.
// Keep logical SourceId values stable: labels only change presentation.
static constexpr AnalogInput HW_NAMED_ANALOG_INPUTS[]={
    AnalogInput::POT_1,AnalogInput::POT_2,AnalogInput::POT_3,AnalogInput::POT_4,
    AnalogInput::STICK_LZ,AnalogInput::STICK_RZ,
    AnalogInput::LINEAR_1,AnalogInput::LINEAR_2,
    AnalogInput::SLIDER_L,AnalogInput::SLIDER_R
};
static const char *HW_ANALOG_DEFAULT_NAMES[]={
    "S1","S2","S3","S4","Ext1","Ext2","L1","L2","Slider1","Slider2"
};
static char hwAnalogNames[10][33]={};
static bool hwAnalogNamesLoaded=false;

void SourceResolver::reloadHardwareAnalogNames(){
    Preferences prefs;
    bool opened=prefs.begin("hwsettings",true);
    for(uint8_t i=0;i<10;i++){
        char key[16];
        snprintf(key,sizeof(key),"aname%u",(unsigned)(i+1));
        String name=opened?prefs.getString(key,HW_ANALOG_DEFAULT_NAMES[i]):String(HW_ANALOG_DEFAULT_NAMES[i]);
        if(name.length()==0) name=HW_ANALOG_DEFAULT_NAMES[i];
        snprintf(hwAnalogNames[i],sizeof(hwAnalogNames[i]),"%s",name.c_str());
    }
    if(opened) prefs.end();
    hwAnalogNamesLoaded=true;
}

static const char *getHardwareAnalogName(uint8_t input){
    if(!hwAnalogNamesLoaded) sourceResolver.reloadHardwareAnalogNames();
    for(uint8_t i=0;i<10;i++){
        if(input==static_cast<uint8_t>(HW_NAMED_ANALOG_INPUTS[i])) return hwAnalogNames[i];
    }
    return nullptr;
}

int16_t SourceResolver::getValue(const SourceId &source,int16_t constantValue) const{
    switch(source.type){
        case SourceType::NONE:
            return 0;
        case SourceType::STICK_FUNCTION:
            if(source.index>=static_cast<uint8_t>(StickFunction::COUNT)) return 0;
            return inputManager.getStick(static_cast<StickFunction>(source.index));
        case SourceType::MODEL_INPUT:
            return 0;
        case SourceType::ANALOG_INPUT:
            if(source.index>=static_cast<uint8_t>(AnalogInput::COUNT)) return 0;
            return inputManager.getAnalog(static_cast<AnalogInput>(source.index));
        case SourceType::SWITCH_INPUT:
            if(source.index>=static_cast<uint8_t>(SwitchInput::COUNT)) return 0;
            return inputManager.getSwitch(static_cast<SwitchInput>(source.index));
        case SourceType::SENSOR_INPUT:
            return 0;
        case SourceType::LOGICAL_SWITCH:
            if(source.index>=MAX_LOGICAL_SWITCHES) return 0;
            return logicalSwitchManager.getValue(source.index);
        case SourceType::CHANNEL_OUTPUT:
            return 0;
        case SourceType::CONSTANT_VALUE:
            return constrain(constantValue,-1000,1000);
        case SourceType::VAR:
            return varProcessor.getValue(source.index);
    }
    return 0;
}
const char *SourceResolver::getName(const SourceId &source) const{
    switch(source.type){
        case SourceType::NONE:
            return "---";
        case SourceType::STICK_FUNCTION:
            switch(static_cast<StickFunction>(source.index)){
                case StickFunction::RUDDER: return "Rudder";
                case StickFunction::THROTTLE: return "Throttle";
                case StickFunction::AILERON: return "Aileron";
                case StickFunction::ELEVATOR: return "Elevator";
                default: return "Stick ?";
            }
        case SourceType::MODEL_INPUT:
            switch(source.index){
                case 0: return "Input Rudder";
                case 1: return "Input Throttle";
                case 2: return "Input Aileron";
                case 3: return "Input Elevator";
                case 4: return "Input Slider L";
                case 5: return "Input Slider R";
                default: return "Input ?";
            }
        case SourceType::ANALOG_INPUT:
            if(const char *custom=getHardwareAnalogName(source.index)) return custom;
            switch(static_cast<AnalogInput>(source.index)){
                case AnalogInput::STICK_LX: return "Left X";
                case AnalogInput::STICK_LY: return "Left Y";
                case AnalogInput::STICK_LZ: return "Left Z";
                case AnalogInput::STICK_RX: return "Right X";
                case AnalogInput::STICK_RY: return "Right Y";
                case AnalogInput::STICK_RZ: return "Right Z";
                case AnalogInput::SLIDER_L: return "Slider L";
                case AnalogInput::SLIDER_R: return "Slider R";
                case AnalogInput::LINEAR_1: return "Linear 1";
                case AnalogInput::LINEAR_2: return "Linear 2";
                case AnalogInput::POT_1: return "Pot 1";
                case AnalogInput::POT_2: return "Pot 2";
                case AnalogInput::POT_3: return "Pot 3";
                case AnalogInput::POT_4: return "Pot 4";
                default: return "Analog ?";
            }
        case SourceType::SWITCH_INPUT:
            switch(static_cast<SwitchInput>(source.index)){
                case SwitchInput::STICK_BTN_L: return "Stick Button L";
                case SwitchInput::STICK_BTN_R: return "Stick Button R";
                case SwitchInput::SW_3POS_1: return "3Pos 1";
                case SwitchInput::SW_3POS_2: return "3Pos 2";
                case SwitchInput::SW_3POS_3: return "3Pos 3";
                case SwitchInput::SW_3POS_4: return "3Pos 4";
                case SwitchInput::SW_3POS_5: return "3Pos 5";
                case SwitchInput::SW_3POS_6: return "3Pos 6";
                case SwitchInput::SW_2POS_1: return "2Pos 1";
                case SwitchInput::SW_2POS_2: return "2Pos 2";
                default: return "Switch ?";
            }
        case SourceType::SENSOR_INPUT:
            switch(static_cast<SensorSource>(source.index)){
                case SensorSource::ROLL: return "Roll";
                case SensorSource::PITCH: return "Pitch";
                case SensorSource::YAW: return "Yaw";
                case SensorSource::GYRO_X: return "Gyro X";
                case SensorSource::GYRO_Y: return "Gyro Y";
                case SensorSource::GYRO_Z: return "Gyro Z";
                case SensorSource::ACCEL_X: return "Accel X";
                case SensorSource::ACCEL_Y: return "Accel Y";
                case SensorSource::ACCEL_Z: return "Accel Z";
                default: return "Sensor ?";
            }
        case SourceType::LOGICAL_SWITCH:
            return "Logical Switch";
        case SourceType::CHANNEL_OUTPUT:
            return "Channel";
        case SourceType::CONSTANT_VALUE:
            return "Constant";
        case SourceType::VAR:
            return "Var";
    }
    return "?";
}