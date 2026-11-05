#include "TrimSwitchManager.h"

TrimSwitchManager trimSwitchManager;

void TrimSwitchManager::reset(){
    for(uint8_t i=0;i<(uint8_t)TrimDirection::COUNT;i++)
        states[i]=false;
}

void TrimSwitchManager::setState(TrimDirection direction,bool state){
    uint8_t index=(uint8_t)direction;
    if(index>=(uint8_t)TrimDirection::COUNT) return;

    states[index]=state;
}

bool TrimSwitchManager::getState(TrimDirection direction) const{
    uint8_t index=(uint8_t)direction;
    if(index>=(uint8_t)TrimDirection::COUNT) return false;

    return states[index];
}

int16_t TrimSwitchManager::getValue(TrimDirection direction) const{
    return getState(direction)?1000:-1000;
}
