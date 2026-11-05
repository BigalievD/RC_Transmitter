#include "CurveProcessor.h"
#include <math.h>
CurveProcessor curveProcessor;
int16_t CurveProcessor::process(int16_t value,const CurveConfig &curve) const{
    value=(int16_t)constrain((int32_t)value,-1000L,1000L);
    if(!curve.enabled) return value;
    switch(curve.type){
        case CurveType::EXPO:return processExpo(value,curve);
        case CurveType::FUNCTION:return processFunction(value,curve);
        case CurveType::CUSTOM:return processCustom(value,curve);
        default:return value;
    }
}
int16_t CurveProcessor::applyExpo(int16_t value,int8_t expo) const{
    int32_t x=constrain((int32_t)value,-1000L,1000L);
    int32_t e=constrain((int32_t)expo,-100L,100L);
    if(e==0) return (int16_t)x;
    if(e>0){
        int64_t cubic=(int64_t)x*x*x/1000000LL;
        int64_t result=(int64_t)(100-e)*x+(int64_t)e*cubic;
        return (int16_t)constrain((int32_t)(result/100),-1000L,1000L);
    }
    int32_t ax=abs(x);
    int32_t remaining=1000-ax;
    int64_t remainingCube=(int64_t)remaining*remaining*remaining/1000000LL;
    int32_t sharp=1000-(int32_t)remainingCube;
    if(x<0) sharp=-sharp;
    int32_t amount=-e;
    int64_t result=(int64_t)(100-amount)*x+(int64_t)amount*sharp;
    return (int16_t)constrain((int32_t)(result/100),-1000L,1000L);
}
int16_t CurveProcessor::processExpo(int16_t value,const CurveConfig &curve) const{
    int32_t shaped=applyExpo(value,curve.expo);
    int64_t weighted=(int64_t)shaped*(int64_t)curve.weight/1000LL;
    int32_t result=(int32_t)constrain(weighted,-32768LL,32767LL);
    return applyOffset((int16_t)result,curve.offset);
}
int16_t CurveProcessor::processFunction(int16_t value,const CurveConfig &curve) const{
    int16_t result=0;
    switch(curve.function){
        case CurveFunction::NONE:result=0;break;
        case CurveFunction::X_POSITIVE:result=value>0?value:0;break;
        case CurveFunction::X_NEGATIVE:result=value<0?value:0;break;
        case CurveFunction::ABS_X:result=(int16_t)abs((int)value);break;
        case CurveFunction::F_POSITIVE:result=value>0?1000:0;break;
        case CurveFunction::F_NEGATIVE:result=value<0?-1000:0;break;
        case CurveFunction::ABS_F:result=value>0?1000:(value<0?-1000:0);break;
        default:result=0;break;
    }
    return applyOffset(result,curve.offset);
}
int16_t CurveProcessor::processCustom(int16_t value,const CurveConfig &curve) const{
    uint8_t count=curve.pointCount;
    if(count<2||count>MAX_CURVE_POINTS) return value;
    int16_t firstX=getPointX(curve,0);
    int16_t lastX=getPointX(curve,count-1);
    if(value<=firstX) return applyOffset((int16_t)constrain((int32_t)curve.y[0],-1000L,1000L),curve.offset);
    if(value>=lastX) return applyOffset((int16_t)constrain((int32_t)curve.y[count-1],-1000L,1000L),curve.offset);
    for(uint8_t i=0;i<count-1;i++){
        int16_t x1=getPointX(curve,i);
        int16_t x2=getPointX(curve,i+1);
        if(x2<=x1) continue;
        if(value<x1||value>x2) continue;
        int16_t result;
        if(!curve.smooth){
            result=interpolateLinear(value,x1,curve.y[i],x2,curve.y[i+1]);
        }else{
            int16_t y0=curve.y[i==0?i:i-1];
            int16_t y1=curve.y[i];
            int16_t y2=curve.y[i+1];
            int16_t y3=curve.y[i+2>=count?i+1:i+2];
            result=interpolateSmooth(value,x1,x2,y0,y1,y2,y3);
        }
        return applyOffset(result,curve.offset);
    }
    return value;
}
int16_t CurveProcessor::applyOffset(int16_t value,int16_t offset) const{
    int32_t result=(int32_t)value+(int32_t)offset;
    return (int16_t)constrain(result,-1000L,1000L);
}
int16_t CurveProcessor::getPointX(const CurveConfig &curve,uint8_t index) const{
    if(curve.pointCount<2) return 0;
    if(curve.easyMode) return (int16_t)(-1000L+(2000L*index)/(curve.pointCount-1));
    return (int16_t)constrain((int32_t)curve.x[index],-1000L,1000L);
}
int16_t CurveProcessor::interpolateLinear(int16_t x,int16_t x1,int16_t y1,int16_t x2,int16_t y2) const{
    if(x2==x1) return (int16_t)constrain((int32_t)y1,-1000L,1000L);
    int32_t result=(int32_t)y1+((int32_t)(x-x1)*(y2-y1))/(x2-x1);
    return (int16_t)constrain(result,-1000L,1000L);
}
int16_t CurveProcessor::interpolateSmooth(int16_t x,int16_t x1,int16_t x2,int16_t y0,int16_t y1,int16_t y2,int16_t y3) const{
    if(x2==x1) return (int16_t)constrain((int32_t)y1,-1000L,1000L);
    float t=(float)(x-x1)/(float)(x2-x1);
    float t2=t*t;
    float t3=t2*t;
    float result=0.5f*((2.0f*y1)+(-y0+y2)*t+(2.0f*y0-5.0f*y1+4.0f*y2-y3)*t2+(-y0+3.0f*y1-3.0f*y2+y3)*t3);
    return (int16_t)constrain((int32_t)lroundf(result),-1000L,1000L);
}
