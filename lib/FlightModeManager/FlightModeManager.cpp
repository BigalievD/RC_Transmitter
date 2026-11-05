#include "FlightModeManager.h"
#include "ConditionEvaluator.h"
#include "ModelRuntime.h"
FlightModeManager flightModeManager;
void FlightModeManager::begin(){
    reset();
}
void FlightModeManager::reset(){
    forcedMode=-1;
    modelRuntime.setActiveFlightMode(0);
}
void FlightModeManager::update(const ModelConfig &model){
    if(forcedMode>=0&&forcedMode<MAX_FLIGHT_MODES){
        modelRuntime.setActiveFlightMode((uint8_t)forcedMode);
        return;
    }
    modelRuntime.setActiveFlightMode(0);
    for(uint8_t i=1;i<MAX_FLIGHT_MODES;i++){
        const FlightModeConfig &mode=model.flightModes[i];
        if(!mode.enabled) continue;
        if(!mode.condition.isValid()) continue;
        if(conditionEvaluator.evaluatePreFlight(mode.condition)){
            modelRuntime.setActiveFlightMode(i);
            return;
        }
    }
}
uint8_t FlightModeManager::getActiveMode() const{
    return modelRuntime.getActiveFlightMode();
}
void FlightModeManager::forceMode(int8_t mode){
    if(mode>=0&&mode<MAX_FLIGHT_MODES){
        forcedMode=mode;
        modelRuntime.setActiveFlightMode((uint8_t)mode);
    }
}
void FlightModeManager::clearForcedMode(){
    forcedMode=-1;
}