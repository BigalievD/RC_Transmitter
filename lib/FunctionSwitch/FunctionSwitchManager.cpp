#include "FunctionSwitchManager.h"

FunctionSwitchManager functionSwitchManager;

void FunctionSwitchManager::reset(){
    for(uint8_t i=0;i<FUNCTION_SWITCH_COUNT;i++)
        states[i]=false;
}

void FunctionSwitchManager::setState(uint8_t index,bool state){
    if(index>=FUNCTION_SWITCH_COUNT) return;
    states[index]=state;
}

void FunctionSwitchManager::toggle(uint8_t index){
    if(index>=FUNCTION_SWITCH_COUNT) return;
    states[index]=!states[index];
}

bool FunctionSwitchManager::getState(uint8_t index) const{
    if(index>=FUNCTION_SWITCH_COUNT) return false;
    return states[index];
}

int16_t FunctionSwitchManager::getValue(uint8_t index) const{
    return getState(index)?1000:-1000;
}
