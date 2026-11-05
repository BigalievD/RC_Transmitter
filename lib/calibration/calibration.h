#pragma once

#include "StickCalibration.h"
#include "StickCalibrationManager.h"
#include "ChannelProcessor.h"

extern StickCalibration sticks[4];
extern StickCalibrationManager calibrationManager;

int16_t getNormalizedStickValue(uint8_t ch);
void calibration_init();