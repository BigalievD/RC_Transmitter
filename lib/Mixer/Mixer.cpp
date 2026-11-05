#include "Mixer.h"
#include "ModelInputProcessor.h"
#include "CurveProcessor.h"
#include "ConditionEvaluator.h"
#include "TrimProcessor.h"
#include "FlightModeManager.h"
#include "HalfRangeTestInput.h"
#include "VarProcessor.h"

Mixer mixer;

static constexpr int32_t MIX_INTERNAL_LIMIT=30000L;
static constexpr uint8_t MIX_SOURCE_HALF_RANGE_FLAG=0x80;
static constexpr uint16_t MIX_AILERON_TYPE_FLAG=0x8000u;

static SourceId getMixRuntimeBaseSource(const MixLine &mix){
    SourceId source=mix.source;
    source.index&=(uint8_t)~MIX_SOURCE_HALF_RANGE_FLAG;
    return source;
}

static bool mixRuntimeHalfRangeEnabled(const MixLine &mix){
    SourceId source=getMixRuntimeBaseSource(mix);
    bool supported=false;
    if(source.type==SourceType::LOGICAL_SWITCH){
        supported=source.index<MAX_LOGICAL_SWITCHES;
    }else if(source.type==SourceType::SWITCH_INPUT){
        // Hardware may configure any SA..SN as 2-POS or Momentary.
        // The option bit is set only after the UI validates the switch type.
        const uint8_t first=static_cast<uint8_t>(SwitchInput::SW_3POS_1);
        supported=source.index>=first&&source.index<first+14;
    }
    // Half range is an option stored in the Mix Source, not the GPIO level.
    return supported&&(mix.source.index&MIX_SOURCE_HALF_RANGE_FLAG)!=0;
}

static int32_t applyMixRuntimeSourceOptions(int32_t value,const MixLine &mix){
    // Convert to half range first; Invert changes its sign but keeps 0%.
    if(mixRuntimeHalfRangeEnabled(mix)){
        value=constrain(value,-1000L,1000L);
        value=(value+1000L)/2L;
    }
    if(mix.reversed) value=-value;
    return value;
}

static bool mixRuntimeHasValidOutput(const MixLine &mix,uint8_t channelCount){
    uint8_t count=mix.outputCount;
    if(count<1) count=1;
    if(count>MAX_MIX_OUTPUTS) count=MAX_MIX_OUTPUTS;

    bool hasAssignedOutput=false;

    for(uint8_t i=0;i<count;i++){
        uint8_t channel=mix.mixOutputs[i].channel;

        if(channel>=MAX_MODEL_CHANNELS)
            continue;

        hasAssignedOutput=true;

        if(channel<channelCount)
            return true;
    }

    return !hasAssignedOutput&&mix.destination<channelCount;
}

void Mixer::reset(){
    for(uint8_t i=0;i<MAX_MODEL_CHANNELS;i++){
        channelValues[i]=0;
        channelStates[i]=ChannelState::UNVISITED;
    }

    for(uint8_t i=0;i<MAX_MIX_LINES;i++)
        mixValues[i]=0;

    channelCycleDetected=false;
}

void Mixer::resetRuntime(uint8_t mixIndex){
    if(mixIndex>=MAX_MIX_LINES) return;
    mixValues[mixIndex]=0;
    slowValues[mixIndex]=0.0f;
    slowLastTime[mixIndex]=0;
    slowInitialized[mixIndex]=false;

    for(uint8_t actionIndex=0;actionIndex<MAX_MIX_ACTIONS;actionIndex++){
        actionSlowValues[mixIndex][actionIndex]=0.0f;
        actionSlowLastTime[mixIndex][actionIndex]=0;
        actionSlowInitialized[mixIndex][actionIndex]=false;
    }

    delayInitialized[mixIndex]=false;
    delayOutputState[mixIndex]=false;
    delayPending[mixIndex]=false;
    delayPendingState[mixIndex]=false;
    delayStartTime[mixIndex]=0;
}

void Mixer::resetRuntime(){
    for(uint8_t i=0;i<MAX_MIX_LINES;i++)
        resetRuntime(i);
    inputLockEnabled=false;
    for(uint8_t i=0;i<MAX_MODEL_CHANNELS;i++)
        lockedChannelValues[i]=0;
    reset();
}

void Mixer::setInputLock(bool locked){
    if(locked==inputLockEnabled) return;

    if(locked){
        for(uint8_t i=0;i<MAX_MODEL_CHANNELS;i++)
            lockedChannelValues[i]=channelValues[i];
    }

    inputLockEnabled=locked;
}

bool Mixer::isInputLocked() const{
    return inputLockEnabled;
}

int16_t Mixer::resolveSource(const SourceId &source,const ModelConfig &model){
    if(source.type==SourceType::MODEL_INPUT)
        return modelInputProcessor.getValue(source.index);
    if(source.type==SourceType::VAR)
        return varProcessor.getValue(source.index);
    if(source.type==SourceType::CHANNEL_OUTPUT){
        if(source.index>=currentChannelCount) return 0;
        return calculateChannel(source.index);
    }
    return sourceResolver.getValue(source);
}

bool Mixer::sourceIsInvalid(const SourceId &source) const{
    if(source.type!=SourceType::CHANNEL_OUTPUT) return false;
    if(source.index>=currentChannelCount) return true;
    return channelStates[source.index]==ChannelState::INVALID;
}

bool Mixer::checkCondition(const MixLine &mix,const ModelConfig &model) const{
    if(!mix.condition.isValid()) return true;
    return conditionEvaluator.evaluate(mix.condition);
}

bool Mixer::checkFlightMode(const MixLine &mix) const{
    uint16_t flightMask=mix.flightModeMask&((1u<<MAX_FLIGHT_MODES)-1u);
    if(flightMask==0) return true;

    uint8_t mode=flightModeManager.getActiveMode();
    if(mode>=MAX_FLIGHT_MODES) return false;

    return (flightMask&(1U<<mode))!=0;
}

bool Mixer::applyDelay(uint8_t mixIndex,bool requestedState,const MixLine &mix,uint32_t now){
    if(mixIndex>=MAX_MIX_LINES) return requestedState;
    if(!delayInitialized[mixIndex]){
        delayInitialized[mixIndex]=true;
        delayOutputState[mixIndex]=requestedState;
        delayPending[mixIndex]=false;
        return requestedState;
    }
    if(requestedState==delayOutputState[mixIndex]){
        delayPending[mixIndex]=false;
        return delayOutputState[mixIndex];
    }
    uint32_t delayTime=requestedState?mix.delayUp:mix.delayDown;
    if(delayTime==0){
        delayOutputState[mixIndex]=requestedState;
        delayPending[mixIndex]=false;
        return requestedState;
    }
    if(!delayPending[mixIndex]||delayPendingState[mixIndex]!=requestedState){
        delayPending[mixIndex]=true;
        delayPendingState[mixIndex]=requestedState;
        delayStartTime[mixIndex]=now;
        return delayOutputState[mixIndex];
    }
    if((uint32_t)(now-delayStartTime[mixIndex])>=delayTime){
        delayOutputState[mixIndex]=requestedState;
        delayPending[mixIndex]=false;
    }
    return delayOutputState[mixIndex];
}

int32_t Mixer::applySlow(uint8_t mixIndex,int32_t target,const MixLine &mix,uint32_t now){
    if(mixIndex>=MAX_MIX_LINES) return target;
    if(!slowInitialized[mixIndex]){
        slowValues[mixIndex]=(float)target;
        slowLastTime[mixIndex]=now;
        slowInitialized[mixIndex]=true;
        return target;
    }
    uint32_t dtMs=now-slowLastTime[mixIndex];
    slowLastTime[mixIndex]=now;
    if(dtMs>200) dtMs=200;
    float current=slowValues[mixIndex];
    float targetValue=(float)target;
    if(targetValue>current){
        if(mix.slowUp>0){
            float step=(1000.0f*dtMs)/mix.slowUp;
            current+=step;
            if(current>targetValue) current=targetValue;
        }else current=targetValue;
    }else if(targetValue<current){
        if(mix.slowDown>0){
            float step=(1000.0f*dtMs)/mix.slowDown;
            current-=step;
            if(current<targetValue) current=targetValue;
        }else current=targetValue;
    }
    slowValues[mixIndex]=current;
    return (int32_t)roundf(current);
}


int32_t Mixer::applyDifferential(int32_t value,int16_t differential) const{
    int32_t diff=constrain((int32_t)differential,-1000L,1000L);

    if(diff>0&&value<0)
        value=(value*(1000L-diff))/1000L;
    else if(diff<0&&value>0)
        value=(value*(1000L+diff))/1000L;

    return value;
}



int32_t Mixer::applyActionSlow(uint8_t mixIndex,uint8_t actionIndex,int32_t target,uint16_t slowUp,uint16_t slowDown,uint32_t now){
    if(mixIndex>=MAX_MIX_LINES||actionIndex>=MAX_MIX_ACTIONS) return target;

    if(!actionSlowInitialized[mixIndex][actionIndex]){
        actionSlowValues[mixIndex][actionIndex]=(float)target;
        actionSlowLastTime[mixIndex][actionIndex]=now;
        actionSlowInitialized[mixIndex][actionIndex]=true;
        return target;
    }

    uint32_t dtMs=now-actionSlowLastTime[mixIndex][actionIndex];
    actionSlowLastTime[mixIndex][actionIndex]=now;
    if(dtMs>200) dtMs=200;

    float current=actionSlowValues[mixIndex][actionIndex];
    float targetValue=(float)target;

    if(targetValue>current){
        if(slowUp>0){
            float step=(1000.0f*dtMs)/slowUp;
            current+=step;
            if(current>targetValue) current=targetValue;
        }else current=targetValue;
    }else if(targetValue<current){
        if(slowDown>0){
            float step=(1000.0f*dtMs)/slowDown;
            current-=step;
            if(current<targetValue) current=targetValue;
        }else current=targetValue;
    }

    actionSlowValues[mixIndex][actionIndex]=current;
    return (int32_t)roundf(current);
}

bool Mixer::actionConditionActive(const MixAction &action,int32_t sourceValue) const{
    if(!action.condition.isValid()) return true;

    // Direction is an Action-only condition. The UI stores the visible
    // Right/Left/Top/Bottom member in value1 and a marker in value2.
    // Right/Top are source > 0, Left/Bottom are source < 0.
    static constexpr int16_t ACTION_DIRECTION_MARKER=0x4449;

    if(action.condition.value2==ACTION_DIRECTION_MARKER&&
       (action.condition.type==ConditionType::SOURCE_POSITIVE||
        action.condition.type==ConditionType::SOURCE_NEGATIVE)){
        bool result=false;

        if(action.condition.type==ConditionType::SOURCE_POSITIVE)
            result=sourceValue>0;
        else
            result=sourceValue<0;

        return action.condition.inverted?!result:result;
    }

    return conditionEvaluator.evaluate(action.condition);
}

const MixActionVariant *Mixer::selectActionVariant(const MixAction &action,int32_t sourceValue) const{
    uint8_t count=action.variantCount;
    if(count==0) return nullptr;
    if(count>MAX_MIX_ACTION_VARIANTS) count=MAX_MIX_ACTION_VARIANTS;

    // Slots 1..N are conditional rates/curves. First matching condition wins.
    // Пока полноценный Condition Selector не подключен, пустой ConditionConfig
    // у дополнительного variant означает "Always on".
    for(uint8_t i=1;i<count;i++){
        const MixActionVariant &variant=action.variants[i];
        if(!variant.enabled) continue;

        if(!variant.condition.isValid()) return &variant;

        static constexpr int16_t ACTION_DIRECTION_MARKER=0x4449;
        if(variant.condition.value2==ACTION_DIRECTION_MARKER&&
           (variant.condition.type==ConditionType::SOURCE_POSITIVE||
            variant.condition.type==ConditionType::SOURCE_NEGATIVE)){
            bool result=variant.condition.type==ConditionType::SOURCE_POSITIVE?
                sourceValue>0:
                sourceValue<0;
            if(variant.condition.inverted) result=!result;
            if(result) return &variant;
            continue;
        }

        if(conditionEvaluator.evaluate(variant.condition)) return &variant;
    }

    // Slot 0 is Otherwise.
    const MixActionVariant &fallback=action.variants[0];
    return fallback.enabled?&fallback:nullptr;
}

int32_t Mixer::applyActions(uint8_t mixIndex,int32_t value,const MixLine &mix,const ModelConfig &model,uint32_t now){
    // Direction is evaluated against the original Free Mix source value,
    // before any Action in the ordered chain changes it.
    const int32_t sourceValue=value;

    uint8_t count=mix.actionCount;
    if(count>MAX_MIX_ACTIONS) count=MAX_MIX_ACTIONS;

    for(uint8_t actionIndex=0;actionIndex<count;actionIndex++){
        const MixAction &action=mix.actions[actionIndex];

        if(!action.enabled){
            actionSlowInitialized[mixIndex][actionIndex]=false;
            continue;
        }

        bool active=actionConditionActive(action,sourceValue);

        switch(action.type){
            case MixActionType::WEIGHT:{
                if(!active) break;
                const MixActionVariant *variant=selectActionVariant(action,sourceValue);
                if(!variant) break;
                int32_t weight=constrain((int32_t)variant->value,-5000L,5000L);
                value=(value*weight)/1000L;
                break;
            }

            case MixActionType::CURVE:{
                if(!active) break;
                const MixActionVariant *variant=selectActionVariant(action,sourceValue);
                if(!variant) break;

                if(variant->curveIndex==OUTPUT_CURVE_EXPO){
                    int8_t expo=(int8_t)constrain((int32_t)variant->expo,-100L,100L);
                    value=curveProcessor.applyExpo(
                        (int16_t)constrain(value,-1000L,1000L),
                        expo
                    );
                }else if(variant->curveIndex>=0&&variant->curveIndex<MAX_MODEL_CURVES){
                    const CurveConfig &curve=model.curves[(uint8_t)variant->curveIndex];
                    if(curve.enabled)
                        value=curveProcessor.process(
                            (int16_t)constrain(value,-1000L,1000L),
                            curve
                        );
                }
                break;
            }

            case MixActionType::DIFFERENTIAL:
                if(active)
                    value=applyDifferential(value,action.value1);
                break;

            case MixActionType::OFFSET:{
                int32_t offset=active?action.value1:action.value2;
                value+=constrain(offset,-2000L,2000L);
                break;
            }

            case MixActionType::SLOW:
                if(active){
                    uint16_t slowUp=(uint16_t)constrain((int32_t)action.value1,0L,25000L);
                    uint16_t slowDown=(uint16_t)constrain((int32_t)action.value2,0L,25000L);
                    value=applyActionSlow(mixIndex,actionIndex,value,slowUp,slowDown,now);
                }else{
                    actionSlowInitialized[mixIndex][actionIndex]=false;
                }
                break;

            case MixActionType::TRIM:
                if(active&&action.trimIndex>=0&&action.trimIndex<MAX_MODEL_TRIMS)
                    value=trimProcessor.process(
                        (int16_t)constrain(value,-1500L,1500L),
                        model,
                        action.trimIndex,
                        flightModeManager.getActiveMode()
                    );
                break;
        }

        // Preserve the mathematical Mix result. Physical channel limits are
        // applied later by OutputProcessor.
        value=constrain(value,-MIX_INTERNAL_LIMIT,MIX_INTERNAL_LIMIT);
    }

    return value;
}


int16_t Mixer::calculateChannel(uint8_t channel){
    if(!currentModel) return 0;
    if(channel>=currentChannelCount) return 0;
    if(channelStates[channel]==ChannelState::DONE)
        return channelValues[channel];
    if(channelStates[channel]==ChannelState::INVALID)
        return 0;
    if(channelStates[channel]==ChannelState::PROCESSING){
        channelCycleDetected=true;
        channelStates[channel]=ChannelState::INVALID;
        channelValues[channel]=0;
        return 0;
    }
    channelStates[channel]=ChannelState::PROCESSING;
    int32_t channelValue=0;
    for(uint8_t i=0;i<MAX_MIX_LINES;i++){
        const MixLine &mix=currentModel->mixes[i];
        if(!mix.enabled){
            resetRuntime(i);
            continue;
        }
        uint8_t outputCount=mix.outputCount;
        if(outputCount<1) outputCount=1;
        if(outputCount>MAX_MIX_OUTPUTS) outputCount=MAX_MIX_OUTPUTS;

        bool hasAssignedOutput=false;
        uint8_t matchingOutputs=0;

        for(uint8_t outputIndex=0;outputIndex<outputCount;outputIndex++){
            const MixOutput &output=mix.mixOutputs[outputIndex];

            if(output.channel>=MAX_MODEL_CHANNELS)
                continue;

            hasAssignedOutput=true;

            if(output.channel==channel)
                matchingOutputs++;
        }

        bool legacyDestinationMatch=
            !hasAssignedOutput&&mix.destination==channel;

        if(matchingOutputs==0&&!legacyDestinationMatch)
            continue;

        bool requestedState=checkFlightMode(mix)&&checkCondition(mix,*currentModel);
        bool active=applyDelay(i,requestedState,mix,currentProcessTime);
        if(!active){
            slowInitialized[i]=false;
            mixValues[i]=0;
            continue;
        }
        SourceId source=getMixRuntimeBaseSource(mix);
        int32_t sourceValue=resolveSource(source,*currentModel);
        if(sourceIsInvalid(source)){
            channelStates[channel]=ChannelState::INVALID;
            channelValues[channel]=0;
            mixValues[i]=0;
            return 0;
        }

        sourceValue=applyMixRuntimeSourceOptions(sourceValue,mix);
        int32_t value=sourceValue;

        if(mix.actionCount>0){
            // New v8 path: actions execute exactly in the order stored in the mix.
            value=applyActions(i,value,mix,*currentModel,currentProcessTime);
        }else{
            // Legacy v2..v7 fixed chain kept for migrated models.
            value=applySlow(i,value,mix,currentProcessTime);

            if(mix.curveIndex>=0&&mix.curveIndex<MAX_MODEL_CURVES)
                value=curveProcessor.process(
                    (int16_t)constrain(value,-1000L,1000L),
                    currentModel->curves[mix.curveIndex]
                );

            value=(value*static_cast<int32_t>(mix.weight))/1000;
            value=applyDifferential(value,mix.differential);
            value+=mix.offset;

            if(mix.trimIndex>=0&&mix.trimIndex<MAX_MODEL_TRIMS)
                value=trimProcessor.process(
                    (int16_t)constrain(value,-1500L,1500L),
                    *currentModel,
                    mix.trimIndex,
                    flightModeManager.getActiveMode()
                );
        }

        value=constrain(value,-MIX_INTERNAL_LIMIT,MIX_INTERNAL_LIMIT);

        // Store the common Mix result before per-output Reverse.
        mixValues[i]=(int16_t)value;

        auto applyOutputContribution=[&](uint8_t outputIndex,bool reversed)->bool{
            int32_t outputValue=value;
            if((mix.flightModeMask&MIX_AILERON_TYPE_FLAG)!=0){
                // Alternating left/right surfaces: output 1/3 and 2/4 move oppositely.
                if(outputCount>1&&((outputIndex&1u)!=0)) outputValue=-outputValue;
                if(outputCount>1) outputValue=applyDifferential(outputValue,mix.differential);
            }
            if(reversed) outputValue=-outputValue;

            switch(mix.operation){
                case MixOperation::ADD:
                    channelValue+=outputValue;
                    break;

                case MixOperation::MULTIPLY:
                    channelValue=channelValue*outputValue/1000;
                    break;

                case MixOperation::REPLACE:
                    channelValue=outputValue;
                    break;

                case MixOperation::LOCK:
                    channelValue=outputValue;
                    channelValue=constrain(
                        channelValue,
                        -MIX_INTERNAL_LIMIT,
                        MIX_INTERNAL_LIMIT
                    );
                    channelValues[channel]=(int16_t)channelValue;
                    channelStates[channel]=ChannelState::DONE;
                    return true;
            }

            channelValue=constrain(
                channelValue,
                -MIX_INTERNAL_LIMIT,
                MIX_INTERNAL_LIMIT
            );
            return false;
        };

        if(legacyDestinationMatch){
            if(applyOutputContribution(0,false))
                return channelValues[channel];
        }else{
            // ETHOS treats every matching Output as a separate contribution,
            // even when several Outputs of the same Free Mix target the same CH.
            // Reverse belongs to the individual Output, so it changes the sign
            // of that contribution before it is combined into the channel.
            for(uint8_t outputIndex=0;outputIndex<outputCount;outputIndex++){
                const MixOutput &output=mix.mixOutputs[outputIndex];

                if(output.channel!=channel)
                    continue;

                if(applyOutputContribution(outputIndex,output.reversed))
                    return channelValues[channel];
            }
        }
        if(channelStates[channel]==ChannelState::INVALID){
            channelValues[channel]=0;
            return 0;
        }
    }
    if(channelStates[channel]==ChannelState::INVALID){
        channelValues[channel]=0;
        return 0;
    }
    channelValues[channel]=(int16_t)channelValue;
    channelStates[channel]=ChannelState::DONE;
    return channelValues[channel];
}

void Mixer::process(const ModelConfig &model){
    currentChannelCount=constrain(model.channelCount,0,MAX_MODEL_CHANNELS);

    if(inputLockEnabled){
        channelCycleDetected=false;
        for(uint8_t channel=0;channel<MAX_MODEL_CHANNELS;channel++){
            if(channel<currentChannelCount){
                channelValues[channel]=lockedChannelValues[channel];
                channelStates[channel]=ChannelState::DONE;
            }else{
                channelValues[channel]=0;
                channelStates[channel]=ChannelState::UNVISITED;
            }
        }
        currentModel=nullptr;
        return;
    }

    reset();
    currentModel=&model;
    currentProcessTime=millis();
    for(uint8_t i=0;i<MAX_MIX_LINES;i++){
        if(!model.mixes[i].enabled||
           !mixRuntimeHasValidOutput(model.mixes[i],currentChannelCount))
            resetRuntime(i);
    }
    for(uint8_t channel=0;channel<currentChannelCount;channel++)
        calculateChannel(channel);
    currentModel=nullptr;
}

int16_t Mixer::getChannel(uint8_t channel) const{
    if(channel>=MAX_MODEL_CHANNELS) return 0;
    return channelValues[channel];
}

const int16_t *Mixer::getChannels() const{
    return channelValues;
}

int16_t Mixer::getMixValue(uint8_t mixIndex) const{
    if(mixIndex>=MAX_MIX_LINES) return 0;
    return mixValues[mixIndex];
}

bool Mixer::hasChannelCycle() const{
    return channelCycleDetected;
}