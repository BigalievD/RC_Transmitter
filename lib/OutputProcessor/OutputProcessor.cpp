#include "OutputProcessor.h"
#include "BalanceProcessor.h"
#include "CurveProcessor.h"
#include <math.h>
OutputProcessor outputProcessor;
static uint32_t ch1DebugLastMs=0;
static bool outputCurveUsesExpo(const OutputConfig &config){
    if(config.curveIndex==OUTPUT_CURVE_EXPO) return true;
    if(config.curveIndex==OUTPUT_CURVE_LEGACY&&config.expo!=0) return true;
    return false;
}
void OutputProcessor::reset(){
    for(uint8_t i=0;i<MAX_MODEL_CHANNELS;i++){
        outputUs[i]=1500;
        outputNormalized[i]=0;
        slowValues[i]=0.0f;
        slowLastTime[i]=0;
        slowInitialized[i]=false;
    }
}

void OutputProcessor::setBalancePositionOverride(uint8_t channel,bool enabled,int16_t position){
    if(channel>=MAX_MODEL_CHANNELS) return;
    balancePositionOverrideEnabled[channel]=enabled;
    balancePositionOverride[channel]=(int16_t)constrain((int32_t)position,-1000L,1000L);
}

void OutputProcessor::clearBalancePositionOverrides(){
    for(uint8_t i=0;i<MAX_MODEL_CHANNELS;i++){
        balancePositionOverrideEnabled[i]=false;
        balancePositionOverride[i]=0;
    }
}
int16_t OutputProcessor::applyCurve(int16_t value,const OutputConfig &config,const ModelConfig &model) const{
    if(config.curveIndex<0||config.curveIndex>=MAX_MODEL_CURVES) return value;
    const CurveConfig &curve=model.curves[config.curveIndex];
    if(!curve.enabled) return value;
    return curveProcessor.process(value,curve);
}
int16_t OutputProcessor::applySlow(uint8_t channel,int16_t target,const OutputConfig &config,uint32_t now){
    if(channel>=MAX_MODEL_CHANNELS) return target;
    if(!slowInitialized[channel]){
        slowValues[channel]=(float)target;
        slowLastTime[channel]=now;
        slowInitialized[channel]=true;
        return target;
    }
    uint32_t dtMs=now-slowLastTime[channel];
    slowLastTime[channel]=now;
    if(dtMs>200) dtMs=200;
    float current=slowValues[channel];
    float targetValue=(float)target;
    if(targetValue>current){
        if(config.slowUp>0){
            float step=(1000.0f*(float)dtMs)/(float)config.slowUp;
            current+=step;
            if(current>targetValue) current=targetValue;
        }else current=targetValue;
    }else if(targetValue<current){
        if(config.slowDown>0){
            float step=(1000.0f*(float)dtMs)/(float)config.slowDown;
            current-=step;
            if(current<targetValue) current=targetValue;
        }else current=targetValue;
    }
    slowValues[channel]=current;
    return (int16_t)constrain((int32_t)lroundf(current),-1500L,1500L);
}
int16_t OutputProcessor::finishNormalized(int16_t input,const OutputConfig &config) const{
    int32_t value=constrain((int32_t)input,-1500L,1500L);
    if(config.reversed) value=-value;
    if(config.balance.enabled)
        value=balanceProcessor.process((int16_t)value,config.balance);
    int32_t minLimit=constrain((int32_t)config.min,-1500L,0L);
    int32_t maxLimit=constrain((int32_t)config.max,0L,1500L);
    int32_t subtrim=constrain((int32_t)config.subtrim,-1000L,1000L);
    int32_t output;
    if(value<0)
        output=map(value,-1000,0,minLimit,subtrim);
    else
        output=map(value,0,1000,subtrim,maxLimit);
    output=constrain(output,minLimit,maxLimit);
    return (int16_t)output;
}
int16_t OutputProcessor::normalizedToUs(int16_t normalizedValue,const OutputConfig &config) const{
    int32_t pwmCenter=constrain((int32_t)config.pwmCenterUs,500L,2500L);
    int32_t us=pwmCenter+((int32_t)normalizedValue*512L)/1000L;
    return (int16_t)constrain(us,0L,32767L);
}
int16_t OutputProcessor::calculateUs(int16_t normalizedValue,const OutputConfig &config) const{
    int16_t value=normalizedValue;
    if(outputCurveUsesExpo(config)) value=curveProcessor.applyExpo(value,config.expo);
    int16_t output=finishNormalized(value,config);
    return normalizedToUs(output,config);
}
int16_t OutputProcessor::calculateUs(int16_t normalizedValue,const OutputConfig &config,const ModelConfig &model) const{
    int16_t value=normalizedValue;
    if(outputCurveUsesExpo(config)) value=curveProcessor.applyExpo(value,config.expo);
    else if(config.curveIndex>=0) value=applyCurve(value,config,model);
    int16_t output=finishNormalized(value,config);
    return normalizedToUs(output,config);
}
void OutputProcessor::process(const ModelConfig &model,const int16_t *mixerValues){
    if(mixerValues==nullptr){
        reset();
        return;
    }
    uint32_t now=millis();
    uint8_t channelCount=constrain(model.channelCount,0,MAX_MODEL_CHANNELS);
    for(uint8_t ch=0;ch<channelCount;ch++)
        processChannel(ch,mixerValues[ch],model.outputs[ch],model,now);
    for(uint8_t ch=channelCount;ch<MAX_MODEL_CHANNELS;ch++){
        outputUs[ch]=1500;
        outputNormalized[ch]=0;
        slowValues[ch]=0.0f;
        slowLastTime[ch]=0;
        slowInitialized[ch]=false;
    }
}
void OutputProcessor::processChannel(uint8_t channel,int16_t mixerValue,const OutputConfig &config,const ModelConfig &model,uint32_t now){
    if(channel>=MAX_MODEL_CHANNELS) return;
    int16_t value=(int16_t)constrain((int32_t)mixerValue,-1500L,1500L);
    if(outputCurveUsesExpo(config)) value=curveProcessor.applyExpo(value,config.expo);
    else if(config.curveIndex>=0) value=applyCurve(value,config,model);
    value=applySlow(channel,value,config,now);

    // Position ON: выбранный analog временно становится реальным положением канала
    // на этапе Balance, поэтому модель движется тем же X, что и курсор графика.
    if(balancePositionOverrideEnabled[channel])
        value=balancePositionOverride[channel];

    int16_t output=finishNormalized(value,config);
    outputNormalized[channel]=output;
    outputUs[channel]=normalizedToUs(output,config);

    // if(channel==0&&now-ch1DebugLastMs>=100){
    //     ch1DebugLastMs=now;
    //     Serial.printf(
    //         "CH1 mixer=%d value=%d posOn=%d posX=%d output=%d us=%d\n",
    //         mixerValue,
    //         value,
    //         balancePositionOverrideEnabled[channel]?1:0,
    //         balancePositionOverride[channel],
    //         outputNormalized[channel],
    //         outputUs[channel]
    //     );
    // }
}
int16_t OutputProcessor::getChannelUs(uint8_t channel) const{
    if(channel>=MAX_MODEL_CHANNELS) return 1500;
    return outputUs[channel];
}
int16_t OutputProcessor::getChannelNormalized(uint8_t channel) const{
    if(channel>=MAX_MODEL_CHANNELS) return 0;
    return outputNormalized[channel];
}
const int16_t *OutputProcessor::getChannelsUs() const{
    return outputUs;
}
