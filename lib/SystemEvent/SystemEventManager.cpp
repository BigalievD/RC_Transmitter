#include "SystemEventManager.h"

SystemEventManager systemEventManager;

void SystemEventManager::reset(){
    for(uint8_t i=0;i<(uint8_t)SystemEventId::COUNT;i++)
        states[i]=false;
}

void SystemEventManager::updateFromThrottle(int16_t throttleValue){
    states[(uint8_t)SystemEventId::THROTTLE_ACTIVE]=
        throttleValue>throttleActiveThreshold;

    states[(uint8_t)SystemEventId::THROTTLE_IN_DEAD_BAND]=
        abs((int32_t)throttleValue)<=abs((int32_t)throttleDeadBand);
}

void SystemEventManager::setState(SystemEventId event,bool state){
    uint8_t index=(uint8_t)event;
    if(index>=(uint8_t)SystemEventId::COUNT) return;
    states[index]=state;
}

bool SystemEventManager::getState(SystemEventId event) const{
    uint8_t index=(uint8_t)event;
    if(index>=(uint8_t)SystemEventId::COUNT) return false;
    return states[index];
}

void SystemEventManager::triggerFlightReset(){
    states[(uint8_t)SystemEventId::FLIGHT_RESET]=true;
}

void SystemEventManager::endCycle(){
    states[(uint8_t)SystemEventId::FLIGHT_RESET]=false;
}

void SystemEventManager::setThrottleActiveThreshold(int16_t value){
    throttleActiveThreshold=constrain(value,-1000,1000);
}

void SystemEventManager::setThrottleDeadBand(int16_t value){
    throttleDeadBand=constrain((int16_t)abs((int32_t)value),0,1000);
}
