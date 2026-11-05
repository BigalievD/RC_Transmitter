#include "ModelRuntime.h"
ModelRuntime modelRuntime;
void ModelRuntime::begin(){
    generation=0;
    reset();
}
void ModelRuntime::reset(){
    activeFlightMode=0;
    generation++;
    initialized=true;
}
void ModelRuntime::setActiveFlightMode(uint8_t mode){
    if(mode>=MAX_FLIGHT_MODES) mode=0;
    activeFlightMode=mode;
}
uint8_t ModelRuntime::getActiveFlightMode() const{
    return activeFlightMode;
}
uint32_t ModelRuntime::getGeneration() const{
    return generation;
}
bool ModelRuntime::isInitialized() const{
    return initialized;
}