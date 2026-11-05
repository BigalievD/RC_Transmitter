#include "StickCalibrationManager.h"
// CONSTRUCTOR
StickCalibrationManager::StickCalibrationManager(StickCalibration sticksArray[4]){
    for (uint8_t i = 0; i < 4; i++){
        sticks[i] = &sticksArray[i];
        capturedMin[i] = 0;
        capturedMax[i] = 0;
        currentRaw[i] = 0;
    }
    data.leftX.current = 0;
    data.leftX.min = 0;
    data.leftX.center = 0;
    data.leftX.max = 0;
    data.leftY.current = 0;
    data.leftY.min = 0;
    data.leftY.center = 0;
    data.leftY.max = 0;
    data.rightX.current = 0;
    data.rightX.min = 0;
    data.rightX.center = 0;
    data.rightX.max = 0;
    data.rightY.current = 0;
    data.rightY.min = 0;
    data.rightY.center = 0;
    data.rightY.max = 0;
    data.stage = IDLE;
    stage = IDLE;
    updateCallback = nullptr;
    hasSamples = false;
}
// BEGIN
bool StickCalibrationManager::begin(){
    bool result = true;
    for (uint8_t i = 0; i < 4; i++){
        if (!sticks[i]->begin()){
            result = false;
        }
    }
    stage = IDLE;
    hasSamples = false;
    data.stage = IDLE;
    return result;
}
// LOAD ALL
bool StickCalibrationManager::loadAll(){
    bool result = true;
    for (uint8_t i = 0; i < 4; i++){
        if (!sticks[i]->load()){
            result = false;
        }
    }
    return result;
}
// ALL CALIBRATED
bool StickCalibrationManager::allCalibrated() const{
    for (uint8_t i = 0; i < 4; i++){
        if (!sticks[i]->isCalibrated()){
            return false;
        }
    }
    return true;
}
// IS CALIBRATING
bool StickCalibrationManager::isCalibrating() const{
    return stage == CALIBRATING;
}
// GET STAGE
StickCalibrationManager::Stage
StickCalibrationManager::getStage() const{
    return stage;
}
// START
void StickCalibrationManager::start(){
    stage = CALIBRATING;
    hasSamples = false;
    for (uint8_t i = 0; i < 4; i++){
        capturedMin[i] = INT_MAX;
        capturedMax[i] = INT_MIN;
        currentRaw[i] = 0;
    }
    // GUI state
    data.stage = CALIBRATING;
    data.leftX.current = 0;
    data.leftX.min = 0;
    data.leftX.center = 0;
    data.leftX.max = 0;
    data.leftY.current = 0;
    data.leftY.min = 0;
    data.leftY.center = 0;
    data.leftY.max = 0;
    data.rightX.current = 0;
    data.rightX.min = 0;
    data.rightX.center = 0;
    data.rightX.max = 0;
    data.rightY.current = 0;
    data.rightY.min = 0;
    data.rightY.center = 0;
    data.rightY.max = 0;
    if (updateCallback){
        updateCallback(data);
    }
}
// UPDATE
// Во время калибровки сюда передаются:
// raw.leftX
// raw.leftY
// raw.rightX
// raw.rightY
// Менеджер автоматически собирает MIN/MAX.
// =====================================================
void StickCalibrationManager::update(const RawData& raw){
    if (stage != CALIBRATING) return;
    currentRaw[0] = raw.leftX;
    currentRaw[1] = raw.leftY;
    currentRaw[2] = raw.rightX;
    currentRaw[3] = raw.rightY;
    for (uint8_t i = 0; i < 4; i++){
        // MIN
        if (currentRaw[i] < capturedMin[i]){
            capturedMin[i] = currentRaw[i];
        }
        // MAX
        if (currentRaw[i] > capturedMax[i]){
            capturedMax[i] = currentRaw[i];
        }
    }
    hasSamples = true;
    // LEFT X
    data.leftX.current = currentRaw[0];
    data.leftX.min = capturedMin[0];
    data.leftX.max = capturedMax[0];
    data.leftX.center = (capturedMin[0] + capturedMax[0]) / 2;
    // LEFT Y
    data.leftY.current = currentRaw[1];
    data.leftY.min = capturedMin[1];
    data.leftY.max = capturedMax[1];
    data.leftY.center = (capturedMin[1] + capturedMax[1]) / 2;
    // RIGHT X
    data.rightX.current = currentRaw[2];
    data.rightX.min = capturedMin[2];
    data.rightX.max = capturedMax[2];
    data.rightX.center = (capturedMin[2] + capturedMax[2]) / 2; 
    // RIGHT Y
    data.rightY.current = currentRaw[3];
    data.rightY.min = capturedMin[3];
    data.rightY.max = capturedMax[3];
    data.rightY.center = (capturedMin[3] + capturedMax[3]) / 2;
    // CALLBACK → GUI
    if (updateCallback){
        updateCallback(data);
    }
}
// FINISH
// В момент DONE:
// currentRaw[] = CENTER
// capturedMin[] = MIN
// capturedMax[] = MAX
// После проверки все четыре калибровки
// устанавливаются одновременно и сохраняются.
// =====================================================
bool StickCalibrationManager::finish(){
    if (stage != CALIBRATING) return false;
    if (!hasSamples) return false;
    // ПРОВЕРКА
    for (uint8_t i = 0; i < 4; i++){
        if (capturedMin[i] >= currentRaw[i] || currentRaw[i] >= capturedMax[i]) return false;
    }
    // УСТАНОВКА НОВЫХ ЗНАЧЕНИЙ
    for (uint8_t i = 0; i < 4; i++){
        int center = (capturedMin[i] + capturedMax[i]) / 2;
        sticks[i]->setMin(capturedMin[i]);
        sticks[i]->setCenter(center);
        sticks[i]->setMax(capturedMax[i]);
    }
    // СОХРАНЕНИЕ ВСЕХ 4 ОСЕЙ
    for (uint8_t i = 0; i < 4; i++){
        if (!sticks[i]->save()) return false;
    }
    // ОБНОВЛЯЕМ GUI
    data.leftX.center = (capturedMin[0] + capturedMax[0]) / 2;
    data.leftY.center = (capturedMin[1] + capturedMax[1]) / 2;
    data.rightX.center = (capturedMin[2] + capturedMax[2]) / 2;
    data.rightY.center = (capturedMin[3] + capturedMax[3]) / 2;
    data.stage = COMPLETE;
    stage = COMPLETE;
    if (updateCallback)  updateCallback(data);
    return true;
}
// CANCEL
void StickCalibrationManager::cancel(){
    stage = IDLE;
    hasSamples = false;
    for (uint8_t i = 0; i < 4; i++){
        capturedMin[i] = 0;
        capturedMax[i] = 0;
        currentRaw[i] = 0;
    }
    data.stage = IDLE;
}
// CALLBACK
void StickCalibrationManager::setUpdateCallback(CalibrationUpdateCallback callback){
    updateCallback = callback;
}
// GET DATA
const StickCalibrationManager::CalibrationData&StickCalibrationManager::getData() const{
    return data;
}
// GET STICK
StickCalibration&StickCalibrationManager::getStick(uint8_t index){
    if (index > 3) index = 0;
    return *sticks[index];
}