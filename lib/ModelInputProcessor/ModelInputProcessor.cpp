#include "ModelInputProcessor.h"
#include "Source.h"
#include "CurveProcessor.h"
#include "TrimProcessor.h"
#include "FlightModeManager.h"
ModelInputProcessor modelInputProcessor;
void ModelInputProcessor::reset(){
    for(uint8_t i=0;i<MAX_MODEL_INPUTS;i++)
        values[i]=0;
}
void ModelInputProcessor::process(const ModelConfig &model){
    uint8_t flightMode=flightModeManager.getActiveMode();
    for(uint8_t i=0;i<MAX_MODEL_INPUTS;i++){
        const ModelInputConfig &cfg=model.inputs[i];
        if(!cfg.enabled||!cfg.source.isValid()){
            values[i]=0;
            continue;
        }
        int32_t value=sourceResolver.getValue(cfg.source);
        if(cfg.reversed) value=-value;
        value=curveProcessor.applyExpo((int16_t)constrain(value,-1000L,1000L),cfg.expo);
        value=(value*cfg.weight)/1000;
        value+=cfg.offset;
        if(cfg.curveIndex>=0&&cfg.curveIndex<MAX_MODEL_CURVES)
            value=curveProcessor.process((int16_t)constrain(value,-1000L,1000L),model.curves[cfg.curveIndex]);
        value=trimProcessor.process((int16_t)constrain(value,-1500L,1500L),model,cfg.trimIndex,flightMode);
        values[i]=(int16_t)constrain(value,-1500L,1500L);
    }
}
int16_t ModelInputProcessor::getValue(uint8_t index) const{
    if(index>=MAX_MODEL_INPUTS) return 0;
    return values[index];
}
