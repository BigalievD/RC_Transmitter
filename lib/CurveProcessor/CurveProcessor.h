#pragma once
#include <Arduino.h>
#include "ModelConfig.h"
class CurveProcessor{
public:
    int16_t process(int16_t value,const CurveConfig &curve) const;
    int16_t applyExpo(int16_t value,int8_t expo) const;
private:
    int16_t processExpo(int16_t value,const CurveConfig &curve) const;
    int16_t processFunction(int16_t value,const CurveConfig &curve) const;
    int16_t processCustom(int16_t value,const CurveConfig &curve) const;
    int16_t applyOffset(int16_t value,int16_t offset) const;
    int16_t getPointX(const CurveConfig &curve,uint8_t index) const;
    int16_t interpolateLinear(int16_t x,int16_t x1,int16_t y1,int16_t x2,int16_t y2) const;
    int16_t interpolateSmooth(int16_t x,int16_t x1,int16_t x2,int16_t y0,int16_t y1,int16_t y2,int16_t y3) const;
};
extern CurveProcessor curveProcessor;
