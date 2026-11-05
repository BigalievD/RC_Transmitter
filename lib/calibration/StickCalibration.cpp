#include "StickCalibration.h"
// CONSTRUCTOR
StickCalibration::StickCalibration(uint8_t id){
    this->id = id;
    rawMin = 0;
    rawCenter = 0;
    rawMax = 0;
    calibrated = false;
    initialized = false;
}
// BEGIN
bool StickCalibration::begin(){
    String namespaceName = "stick" + String(id);
    initialized = preferences.begin(namespaceName.c_str(), false);
    return initialized;
}
// LOAD
bool StickCalibration::load(){
    if (!initialized) return false;
    calibrated = preferences.getBool("calibrated", false);
    if (!calibrated){
        rawMin = 0;
        rawCenter = 0;
        rawMax = 0;
        return false;
    }
    rawMin = preferences.getInt("min", 0);
    rawCenter = preferences.getInt("center", 0);
    rawMax = preferences.getInt("max", 0);
    if (!isCalibrated()){
        calibrated = false;
        return false;
    }
    return true;
}
// SAVE
bool StickCalibration::save(){
    if (!initialized) return false;
    if (rawMin >= rawCenter || rawCenter >= rawMax){
        return false;
    }
    preferences.putInt("min", rawMin);
    preferences.putInt("center", rawCenter);
    preferences.putInt("max", rawMax);
    preferences.putBool("calibrated", true);
    calibrated = true;
    return true;
}
// CLEAR
void StickCalibration::clear(){
    if (!initialized) return;
    preferences.clear();
    rawMin = 0;
    rawCenter = 0;
    rawMax = 0;
    calibrated = false;
}
// SET MIN
void StickCalibration::setMin(int value){
    rawMin = value;
    calibrated = false;
}
// SET CENTER
void StickCalibration::setCenter(int value){
    rawCenter = value;
    calibrated = false;
}
// SET MAX
void StickCalibration::setMax(int value){
    rawMax = value;
    calibrated = false;
}
// GET MIN
int StickCalibration::getMin() const{
    return rawMin;
}
// GET CENTER
int StickCalibration::getCenter() const{
    return rawCenter;
}
// GET MAX
int StickCalibration::getMax() const{
    return rawMax;
}
// IS CALIBRATED
bool StickCalibration::isCalibrated() const{
    if (!calibrated) return false;
    if (rawMin >= rawCenter) return false;
    if (rawCenter >= rawMax) return false;
    return true;
}
// NORMALIZE
int StickCalibration::normalize(int raw) const{
    if (!isCalibrated()) return 0;
    // LEFT HALF
    if (raw <= rawCenter){
        long value = map(raw, rawMin, rawCenter, -1000, 0);
        return constrain((int)value, -1000, 0);
    }
    // RIGHT HALF
    long value = map(raw, rawCenter, rawMax, 0, 1000);
    return constrain((int)value, 0, 1000);
}
