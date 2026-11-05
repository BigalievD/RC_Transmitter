#include "battery.h"

BatteryMonitor::BatteryMonitor(uint8_t adcPin)
    : _adcPin(adcPin){
}

void BatteryMonitor::begin(){
    pinMode(_adcPin,INPUT);
    analogReadResolution(12);
}

float BatteryMonitor::readVoltage(){
    uint32_t sumMv=0;

    for(uint8_t i=0;i<SAMPLE_COUNT;i++){
        sumMv+=analogReadMilliVolts(_adcPin);
        delayMicroseconds(250);
    }

    float adcVoltage=(sumMv/(float)SAMPLE_COUNT)/1000.0f;
    return adcVoltage*DIVIDER_RATIO;
}

uint8_t BatteryMonitor::voltageToPercent(float v) const{
    if(v>=4.20f) return 100;
    if(v<=3.30f) return 0;

    if(v>=4.10f)
        return (uint8_t)roundf(90.0f+(v-4.10f)*100.0f);

    if(v>=4.00f)
        return (uint8_t)roundf(80.0f+(v-4.00f)*100.0f);

    if(v>=3.90f)
        return (uint8_t)roundf(65.0f+(v-3.90f)*150.0f);

    if(v>=3.80f)
        return (uint8_t)roundf(45.0f+(v-3.80f)*200.0f);

    if(v>=3.70f)
        return (uint8_t)roundf(25.0f+(v-3.70f)*200.0f);

    if(v>=3.60f)
        return (uint8_t)roundf(10.0f+(v-3.60f)*150.0f);

    return (uint8_t)roundf((v-3.30f)*(10.0f/0.30f));
}

BatteryStatus BatteryMonitor::read(bool charging){
    BatteryStatus status;

    status.voltage=readVoltage();
    status.percent=voltageToPercent(status.voltage);
    status.charging=charging;

    return status;
}
