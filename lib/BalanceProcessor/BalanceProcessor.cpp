#include "BalanceProcessor.h"

BalanceProcessor balanceProcessor;

int16_t BalanceProcessor::process(int16_t value,const BalanceConfig &balance) const{
    return process(value,value,balance);
}

int16_t BalanceProcessor::process(int16_t value,int16_t position,const BalanceConfig &balance) const{
    value=constrain(value,-1000,1000);
    position=constrain(position,-1000,1000);
    if(!balance.enabled) return value;

    int16_t correction=getCorrection(position,balance);
    int32_t result=(int32_t)value+correction;
    return (int16_t)constrain(result,-1000L,1000L);
}
int16_t BalanceProcessor::getCorrection(int16_t value,const BalanceConfig &balance) const{
    value = constrain(value, -1000, 1000);
    if (!balance.enabled)
        return 0;
    if (balance.pointCount < 2 || balance.pointCount > MAX_BALANCE_POINTS)
        return 0;
    uint8_t count = balance.pointCount;
    if (value <= -1000)
        return balance.correction[0];
    if (value >= 1000)
        return balance.correction[count - 1];
    for (uint8_t i = 0; i < count - 1; i++){
        int16_t x1 = -1000 + (2000L * i) / (count - 1);
        int16_t x2 = -1000 + (2000L * (i + 1)) / (count - 1);
        if (value < x1 || value > x2)
            continue;
        if (!balance.smooth)
            return interpolateLinear(value, x1, balance.correction[i], x2, balance.correction[i + 1]);
        int16_t y0 = balance.correction[(i == 0) ? i : i - 1];
        int16_t y1 = balance.correction[i];
        int16_t y2 = balance.correction[i + 1];
        int16_t y3 = balance.correction[(i + 2 >= count) ? i + 1 : i + 2];
        return interpolateSmooth(value, x1, x2, y0, y1, y2, y3);
    }
    return 0;
}
int16_t BalanceProcessor::interpolateLinear(int16_t x,int16_t x1,int16_t y1,int16_t x2,int16_t y2) const{
    if (x2 == x1)
        return y1;
    int32_t result = y1 + ((int32_t)(x - x1) * (y2 - y1)) / (x2 - x1);
    return (int16_t)constrain(result, -1000L, 1000L);
}
int16_t BalanceProcessor::interpolateSmooth(int16_t x,int16_t x1,int16_t x2,int16_t y0,int16_t y1,int16_t y2,int16_t y3) const{
    if (x2 == x1)
        return y1;
    float t = (float)(x - x1) / (float)(x2 - x1);
    float t2 = t * t;
    float t3 = t2 * t;
    float result = 0.5f * (
        (2.0f * y1) +
        (-y0 + y2) * t +
        (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3) * t2 +
        (-y0 + 3.0f * y1 - 3.0f * y2 + y3) * t3
    );
    return (int16_t)constrain((int32_t)lroundf(result), -1000L, 1000L);
}