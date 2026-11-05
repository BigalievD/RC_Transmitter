#ifndef STICK_CALIBRATION_H
#define STICK_CALIBRATION_H

#include <Arduino.h>
#include <Preferences.h>

class StickCalibration
{
public:
    // CONSTRUCTOR
    explicit StickCalibration(uint8_t id);
    // INITIALIZATION
    bool begin();
    // NVS
    bool load();
    bool save();
    void clear();
    // CALIBRATION DATA
    void setMin(int value);
    void setCenter(int value);
    void setMax(int value);
    int getMin() const;
    int getCenter() const;
    int getMax() const;
    // STATUS
    bool isCalibrated() const;
    // NORMALIZE
    // =================================================
    // rawMin    -> -1000
    // rawCenter ->     0
    // rawMax    -> +1000
    // -1000 = -100%
    //     0 =    0%
    // +1000 = +100%
    // =================================================
    int normalize(int raw) const;
private:
    Preferences preferences;
    uint8_t id;
    int rawMin;
    int rawCenter;
    int rawMax;
    bool calibrated;
    bool initialized;
};

#endif