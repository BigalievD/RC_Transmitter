#pragma once
#include <Arduino.h>
#include "ModelConfig.h"

class BalanceProcessor {
public:
    int16_t process(int16_t value,const BalanceConfig &balance) const;
    int16_t process(int16_t value,int16_t position,const BalanceConfig &balance) const;
    int16_t getCorrection(int16_t value,const BalanceConfig &balance) const;
private:
    int16_t interpolateLinear(int16_t x,int16_t x1,int16_t y1,int16_t x2,int16_t y2) const;
    int16_t interpolateSmooth(int16_t x,int16_t x1,int16_t x2,int16_t y0,int16_t y1,int16_t y2,int16_t y3) const;
};

extern BalanceProcessor balanceProcessor;