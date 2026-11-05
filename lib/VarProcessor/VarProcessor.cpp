#include "VarProcessor.h"
#include "ConditionEvaluator.h"
#include "ModelManager.h"
#include <limits.h>
#include <string.h>

VarProcessor varProcessor;

static int32_t saturateVarIntermediate(int64_t v){
    if(v>INT32_MAX) return INT32_MAX;
    if(v<INT32_MIN) return INT32_MIN;
    return (int32_t)v;
}

// Value and parameter are in 0.1% and 0.1 units, respectively.
static int32_t applyVarAction(int32_t value,const VarActionConfig &action){
    const int32_t p=action.parameter;
    switch(action.function){
        case VarActionFunction::ASSIGN:return p;
        case VarActionFunction::ADD:return saturateVarIntermediate((int64_t)value+p);
        case VarActionFunction::SUBTRACT:return saturateVarIntermediate((int64_t)value-p);
        case VarActionFunction::MULTIPLY:return saturateVarIntermediate((int64_t)value*p/10);
        case VarActionFunction::DIVIDE:return p?saturateVarIntermediate((int64_t)value*10/p):value;
        case VarActionFunction::PERCENT:return saturateVarIntermediate((int64_t)value*p/1000);
        case VarActionFunction::MINIMUM:return value<p?value:p;
        case VarActionFunction::MAXIMUM:return value>p?value:p;
        case VarActionFunction::REPURPOSE:return value; // Not connected to trim runtime.
        default:return value;
    }
}

static int32_t clampVar(int32_t value,const VarConfig &var){
    int32_t lo=var.rangeMin,hi=var.rangeMax;
    if(lo>hi){int32_t tmp=lo;lo=hi;hi=tmp;}
    if(value<lo) return lo;
    if(value>hi) return hi;
    return value;
}

int16_t VarProcessor::evaluateBase(const VarConfig &var) const{
    if(!var.enabled||!var.valueCount||!var.values[0].enabled) return 0;
    int32_t result=var.values[0].value;
    const uint8_t count=var.valueCount<MAX_VAR_VALUES?var.valueCount:MAX_VAR_VALUES;
    for(uint8_t i=1;i<count;i++){
        const VarValueConfig &candidate=var.values[i];
        if(!candidate.enabled||!candidate.condition.isValid()) continue;
        if(!conditionEvaluator.evaluate(candidate.condition)) continue;
        result=candidate.value;
        break;
    }
    return (int16_t)result; // Base is NOT clamped to Range.
}

void VarProcessor::reset(){
    memset(values,0,sizeof(values));
    memset(slotValues,0,sizeof(slotValues));
    memset(configuredValues,0,sizeof(configuredValues));
    memset(initialized,0,sizeof(initialized));
    for(uint8_t i=0;i<MAX_MODEL_VARS;i++)
        for(uint8_t j=0;j<MAX_VAR_ACTIONS;j++) actionStates[i][j]=ActionState{};
    currentModelId=0;
    valuesNeedSave=false;
    saveAtRangeLimit=false;
    lastValueChange=0;
    lastValueCheckpoint=0;
}

void VarProcessor::process(ModelConfig &model){
    if(currentModelId!=model.modelId){reset();currentModelId=model.modelId;}
    const uint32_t now=millis();
    for(uint8_t i=0;i<MAX_MODEL_VARS;i++){
        VarConfig &var=model.vars[i];
        if(!var.enabled||!var.valueCount||!var.values[0].enabled){
            initialized[i]=false;
            values[i]=0;
            for(uint8_t slot=0;slot<MAX_VAR_VALUES;slot++){
                slotValues[i][slot]=0;
                configuredValues[i][slot]=0;
            }
            for(uint8_t j=0;j<MAX_VAR_ACTIONS;j++) actionStates[i][j]=ActionState{};
            continue;
        }
        const uint8_t valueCount=var.valueCount<MAX_VAR_VALUES?var.valueCount:MAX_VAR_VALUES;
        if(!initialized[i]){
            for(uint8_t slot=0;slot<MAX_VAR_VALUES;slot++){
                const int16_t configured=slot<valueCount&&var.values[slot].enabled?var.values[slot].value:0;
                slotValues[i][slot]=configured;
                configuredValues[i][slot]=configured;
            }
            initialized[i]=true;
        }else{
            // Preserve Action changes to each Value. User edits are applied as
            // a delta, so editing another Value never discards accumulation.
            for(uint8_t slot=0;slot<MAX_VAR_VALUES;slot++){
                const int16_t configured=slot<valueCount&&var.values[slot].enabled?var.values[slot].value:0;
                if(configured!=configuredValues[i][slot]){
                    slotValues[i][slot]=(int16_t)clampVar((int32_t)slotValues[i][slot]+configured-configuredValues[i][slot],var);
                    configuredValues[i][slot]=configured;
                }
            }
        }
        // Active Value selects the target for Var Actions; each Value has its
        // own independent accumulated value, including the default (slot 0).
        uint8_t activeSlot=0;
        for(uint8_t slot=1;slot<valueCount;slot++){
            const VarValueConfig &candidate=var.values[slot];
            if(!candidate.enabled||!candidate.condition.isValid()) continue;
            if(!conditionEvaluator.evaluate(candidate.condition)) continue;
            activeSlot=slot;
            break;
        }
        const uint8_t count=var.actionCount<MAX_VAR_ACTIONS?var.actionCount:MAX_VAR_ACTIONS;
        for(uint8_t j=0;j<MAX_VAR_ACTIONS;j++){
            ActionState &state=actionStates[i][j];
            if(j>=count){state=ActionState{};continue;}
            const VarActionConfig &action=var.actions[j];
            const bool active=action.enabled&&action.condition.isValid()&&conditionEvaluator.evaluate(action.condition);
            if(!active){state=ActionState{};continue;}
            bool execute=false;
            if(!state.wasActive){
                state.wasActive=true;
                state.activeSince=now;
                state.lastRepeat=now;
                execute=true;
            }else if(!action.edge){
                const uint32_t elapsed=(uint32_t)(now-state.activeSince);
                const uint32_t interval=elapsed>=5000U?100U:
                                          elapsed>=4000U?150U:
                                          elapsed>=3000U?250U:
                                          elapsed>=2000U?500U:1000U;
                if((uint32_t)(now-state.lastRepeat)>=interval){
                    state.lastRepeat=now;
                    execute=true;
                }
            }
            if(execute){
                const int16_t oldValue=slotValues[i][activeSlot];
                const int16_t newValue=(int16_t)clampVar(applyVarAction(oldValue,action),var);
                if(newValue!=oldValue){
                    // Commit to the in-memory model so saved models restore the
                    // accumulated Value, not its original configuration.
                    slotValues[i][activeSlot]=newValue;
                    configuredValues[i][activeSlot]=newValue;
                    var.values[activeSlot].value=newValue;
                    if(!valuesNeedSave) lastValueCheckpoint=now;
                    valuesNeedSave=true;
                    lastValueChange=now;
                    int32_t lo=var.rangeMin,hi=var.rangeMax;
                    if(lo>hi){int32_t tmp=lo;lo=hi;hi=tmp;}
                    if(newValue==lo||newValue==hi) saveAtRangeLimit=true;
                }
            }
        }
        // Range applies to the output, and to Action-updated slot values.
        // Unmodified configured Values may still lie outside Range.
        values[i]=(int16_t)clampVar(slotValues[i][activeSlot],var);
    }
}

int16_t VarProcessor::getValue(uint8_t index) const{
    return index<MAX_MODEL_VARS?values[index]:0;
}

int16_t VarProcessor::getSlotValue(uint8_t index,uint8_t slot) const{
    if(index>=MAX_MODEL_VARS||slot>=MAX_VAR_VALUES) return 0;
    return initialized[index]?slotValues[index][slot]:0;
}

// Called from the regular UI loop, not from the mixer's tight processing path.
// Never write flash on each Action tick. Coalesce writes after a pause and
// checkpoint continuous changes every 5 seconds.
void VarProcessor::servicePersistence(){
    if(!valuesNeedSave||!currentModelId) return;
    const uint32_t now=millis();
    const bool settled=(uint32_t)(now-lastValueChange)>=500U;
    const bool checkpoint=(uint32_t)(now-lastValueCheckpoint)>=5000U;
    if(!saveAtRangeLimit&&!settled&&!checkpoint) return;
    // Flush also when editor Save-on-Exit deferral is active, otherwise the
    // accumulated values could disappear on power-cycle before editor exit.
    const bool ok=modelManager.checkpointActiveModel();
    if(ok){
        valuesNeedSave=false;
        saveAtRangeLimit=false;
        lastValueCheckpoint=now;
    }
}
