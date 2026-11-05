#ifndef STICK_CALIBRATION_MANAGER_H
#define STICK_CALIBRATION_MANAGER_H

#include <Arduino.h>
#include "StickCalibration.h"
class StickCalibrationManager
{
public:
    // CALIBRATION STAGES
    enum Stage{
        IDLE,
        CALIBRATING,
        COMPLETE
    };
    // ONE AXIS DATA
    struct AxisData{
        int current;
        int min;
        int center;
        int max;
    };
    // ALL CALIBRATION DATA
    struct CalibrationData{
        AxisData leftX;
        AxisData leftY;
        AxisData rightX;
        AxisData rightY;
        Stage stage;
    };
    // RAW DATA FROM ADS1115
    struct RawData{
        int leftX;
        int leftY;
        int rightX;
        int rightY;
    };
    // CALLBACK
    using CalibrationUpdateCallback = void (*)(const CalibrationData& data);
    // CONSTRUCTOR
    explicit StickCalibrationManager(StickCalibration sticks[4]);
    // INITIALIZATION
    bool begin();
    // LOAD NVS
    bool loadAll();
    // STATUS
    bool allCalibrated() const;
    bool isCalibrating() const;
    Stage getStage() const;
    // CALIBRATION
    void start();
    void update(const RawData& raw);
    bool finish();
    void cancel();
    // CALLBACK
    void setUpdateCallback(CalibrationUpdateCallback callback);
    // DATA FOR GUI
    const CalibrationData& getData() const;
    // ACCESS TO STICK
    StickCalibration& getStick(uint8_t index);

private:
    StickCalibration* sticks[4];
    // STATE
    Stage stage;
    // TEMPORARY CALIBRATION DATA
    int capturedMin[4];
    int capturedMax[4];
    int currentRaw[4];
    // GUI DATA
    CalibrationData data;
    // CALLBACK
    CalibrationUpdateCallback updateCallback;
    bool hasSamples;
};

#endif