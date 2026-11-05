#include "ModelManager.h"
#include "ModelRuntime.h"
#include "FlightModeManager.h"
#include "LogicalSwitchManager.h"
#include "FunctionSwitchManager.h"
#include "TrimSwitchManager.h"
#include "SystemEventManager.h"
#include "Mixer.h"
#include "OutputProcessor.h"
#include "FailsafeProcessor.h"
#include "ModelStorage.h"
#include <Preferences.h>
#include <new>
#include <esp_heap_caps.h>
ModelManager modelManager;
void ModelManager::begin(){
    modelCount=0;
    if(!ensureModels()){
        Serial.printf("ERROR: ModelConfig PSRAM allocation failed (%u bytes each)\n",(unsigned)sizeof(ModelConfig));
        abort();
    }
    Serial.printf("ModelManager activeModel in PSRAM: %u bytes\n",(unsigned)sizeof(ModelConfig));
    if(!ensureBackgroundSave())
        Serial.println("WARNING: background model save unavailable, synchronous fallback will be used");
    createDefaultModel();
    resetRuntime();
}
bool ModelManager::ensureModels(){
    if(activeModel==nullptr){
        void *mem=heap_caps_malloc(sizeof(ModelConfig),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(!mem) return false;
        activeModel=new (mem) ModelConfig();
    }
    if(scratchModel==nullptr){
        void *mem=heap_caps_malloc(sizeof(ModelConfig),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(!mem) return false;
        scratchModel=new (mem) ModelConfig();
    }
    return true;
}

bool ModelManager::ensureBackgroundSave(){
    if(saveTaskHandle&&saveMutex&&saveSnapshots[0]&&saveSnapshots[1]) return true;

    if(!saveMutex){
        saveMutex=xSemaphoreCreateMutex();
        if(!saveMutex) return false;
    }

    for(uint8_t i=0;i<2;i++){
        if(saveSnapshots[i]) continue;
        void *mem=heap_caps_malloc(sizeof(ModelConfig),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(!mem) return false;
        saveSnapshots[i]=new (mem) ModelConfig();
    }

    if(!saveTaskHandle){
        BaseType_t ok=xTaskCreate(
            saveTaskThunk,
            "model_save",
            6144,
            this,
            1,
            &saveTaskHandle
        );
        if(ok!=pdPASS){
            saveTaskHandle=nullptr;
            return false;
        }
    }

    return true;
}

void ModelManager::saveTaskThunk(void *arg){
    ModelManager *self=static_cast<ModelManager *>(arg);
    if(self) self->saveTaskLoop();
    vTaskDelete(nullptr);
}

void ModelManager::saveTaskLoop(){
    for(;;){
        ulTaskNotifyTake(pdTRUE,portMAX_DELAY);

        for(;;){
            uint8_t snapshot=0;

            xSemaphoreTake(saveMutex,portMAX_DELAY);
            if(!savePending){
                xSemaphoreGive(saveMutex);
                break;
            }

            snapshot=pendingSnapshot;
            savePending=false;
            saveTaskBusy=true;
            savingSnapshot=snapshot;
            xSemaphoreGive(saveMutex);

            bool ok=modelStorage.saveModel(*saveSnapshots[snapshot]);

            xSemaphoreTake(saveMutex,portMAX_DELAY);
            lastBackgroundSaveOk=ok;
            saveTaskBusy=false;
            xSemaphoreGive(saveMutex);

            if(ok)
                Serial.printf("Model %u saved in background\n",saveSnapshots[snapshot]->modelId);
            else
                Serial.printf("ERROR: background save failed for model %u\n",saveSnapshots[snapshot]->modelId);
        }
    }
}

void ModelManager::setEditorSaveDeferred(bool enabled){
    editorSaveDeferred=enabled;
}

bool ModelManager::flushDeferredEditorSave(){
    if(!editorModelDirty) return true;
    // Keep the dirty state until the save request has been accepted.
    bool previous=editorSaveDeferred;
    editorSaveDeferred=false;
    bool ok=requestSaveActiveModel();
    editorSaveDeferred=previous;
    if(ok) editorModelDirty=false;
    return ok;
}

// A runtime checkpoint must persist even while the editor postpones UI saves.
// Temporarily suspend deferral and take exactly one background snapshot.
bool ModelManager::checkpointActiveModel(){
    const bool deferred=editorSaveDeferred;
    editorSaveDeferred=false;
    const bool ok=requestSaveActiveModel();
    editorSaveDeferred=deferred;
    if(ok) editorModelDirty=false;
    return ok;
}

bool ModelManager::requestSaveActiveModel(){
    if(editorSaveDeferred){editorModelDirty=true;return true;}
    // First save of "New Model" changes modelId/model list, so keep it synchronous.
    if(active().modelId==0) return saveActiveModel();

    if(!ensureBackgroundSave())
        return saveActiveModel();

    xSemaphoreTake(saveMutex,portMAX_DELAY);

    uint8_t target=0;
    if(saveTaskBusy)
        target=(uint8_t)(1-savingSnapshot);
    else if(savePending)
        target=pendingSnapshot;

    // Publish the snapshot only after the copy is complete.
    *saveSnapshots[target]=active();
    pendingSnapshot=target;
    savePending=true;

    xSemaphoreGive(saveMutex);

    updateModelInfo(active().modelId,active().name);
    xTaskNotifyGive(saveTaskHandle);
    return true;
}

bool ModelManager::waitForBackgroundSave(uint32_t timeoutMs){
    if(!saveTaskHandle||!saveMutex) return true;

    uint32_t start=millis();

    for(;;){
        xSemaphoreTake(saveMutex,portMAX_DELAY);
        bool done=!saveTaskBusy&&!savePending;
        bool ok=lastBackgroundSaveOk;
        xSemaphoreGive(saveMutex);

        if(done) return ok;
        if(timeoutMs!=portMAX_DELAY&&(millis()-start)>=timeoutMs) return false;

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
ModelConfig &ModelManager::active(){
    return *activeModel;
}
const ModelConfig &ModelManager::active() const{
    return *activeModel;
}
ModelConfig &ModelManager::scratch(){
    return *scratchModel;
}
void ModelManager::saveActiveModelId(){
    Preferences prefs;
    prefs.begin("model_mgr",false);
    prefs.putUShort("active_id",active().modelId);
    prefs.end();
}
uint16_t ModelManager::loadActiveModelId() const{
    Preferences prefs;
    prefs.begin("model_mgr",true);
    uint16_t modelId=prefs.getUShort("active_id",0);
    prefs.end();
    return modelId;
}
bool ModelManager::loadLastActiveModel(){
    waitForBackgroundSave();
    uint16_t modelId=loadActiveModelId();
    if(modelId==0) return false;
    if(!modelStorage.modelExists(modelId)) return false;
    return loadModel(modelId);
}
ModelConfig &ModelManager::getActiveModel(){
    return active();
}
const ModelConfig &ModelManager::getActiveModel() const{
    return active();
}
uint16_t ModelManager::getActiveModelId() const{
    return active().modelId;
}
void ModelManager::activateModel(const ModelConfig &model){
    if(&model!=activeModel)
        active()=model;
    resetRuntime();
}
void ModelManager::resetRuntime(){
    modelRuntime.reset();
    flightModeManager.reset();
    logicalSwitchManager.reset();
    functionSwitchManager.reset();
    trimSwitchManager.reset();
    systemEventManager.reset();
    mixer.resetRuntime();
    outputProcessor.reset();
    failsafeProcessor.process(active());
}
void ModelManager::createDefaultModel(){
    buildDefaultModel(active());
    addModelInfo(active().modelId,active().name);
}
uint16_t ModelManager::findFreeModelId() const{
    for(uint32_t id=1;id<=65535;id++){
        uint16_t modelId=(uint16_t)id;
        if(!modelExists(modelId)&&!modelStorage.modelExists(modelId))
            return modelId;
    }
    return 0;
}
bool ModelManager::createModel(const char *name){
    waitForBackgroundSave();
    uint16_t modelId=findFreeModelId();
    if(modelId==0) return false;
    return createModel(modelId,name);
}
bool ModelManager::createModel(uint16_t modelId,const char *name){
    waitForBackgroundSave();
    if(modelId==0) return false;
    if(modelExists(modelId)) return false;
    if(modelStorage.modelExists(modelId)) return false;
    if(modelCount>=MAX_MODELS) return false;
    buildDefaultModel(scratch());
    scratch().modelId=modelId;
    if(name&&name[0]!='\0')
        snprintf(scratch().name,sizeof(scratch().name),"%s",name);
    else
        snprintf(scratch().name,sizeof(scratch().name),"Model %u",modelId);
    if(!modelStorage.saveModel(scratch())) return false;
    if(!addModelInfo(scratch().modelId,scratch().name)){
        modelStorage.deleteModel(modelId);
        return false;
    }
    active()=scratch();
    resetRuntime();
    saveActiveModelId();
    return true;
}
bool ModelManager::loadModel(uint16_t modelId){
    if(!flushDeferredEditorSave()) return false;
    if(!waitForBackgroundSave()) return false;
    if(modelId==0) return false;
    if(!modelStorage.loadModel(modelId,scratch())) return false;
    if(scratch().modelId!=modelId) return false;
    active()=scratch();
    resetRuntime();
    if(!modelExists(modelId))
        addModelInfo(active().modelId,active().name);
    else
        updateModelInfo(active().modelId,active().name);
    saveActiveModelId();
    return true;
}
bool ModelManager::selectModel(uint16_t modelId){
    if(!flushDeferredEditorSave()) return false;
    if(!waitForBackgroundSave()) return false;
    if(modelId==0){
        buildDefaultModel(active());
        resetRuntime();
        saveActiveModelId();
        return true;
    }
    if(modelId==active().modelId) return true;
    return loadModel(modelId);
}
bool ModelManager::saveActiveModel(){
    waitForBackgroundSave();
    if(active().modelId==0){
        if(modelCount>=MAX_MODELS) return false;
        uint16_t newId=findFreeModelId();
        if(newId==0) return false;
        char oldName[32];
        snprintf(oldName,sizeof(oldName),"%s",active().name);
        active().modelId=newId;
        if(active().name[0]=='\0'||strcmp(active().name,"New Model")==0)
            snprintf(active().name,sizeof(active().name),"Model %u",newId);
        if(!modelStorage.saveModel(active())){
            active().modelId=0;
            snprintf(active().name,sizeof(active().name),"%s",oldName);
            return false;
        }
        if(!addModelInfo(active().modelId,active().name)){
            modelStorage.deleteModel(active().modelId);
            active().modelId=0;
            snprintf(active().name,sizeof(active().name),"%s",oldName);
            return false;
        }
        saveActiveModelId();
        return true;
    }
    if(!modelStorage.saveModel(active())) return false;
    updateModelInfo(active().modelId,active().name);
    return true;
}
bool ModelManager::renameModel(uint16_t modelId,const char *name){
    waitForBackgroundSave();
    if(modelId==0) return false;
    if(!name||name[0]=='\0') return false;
    if(modelId==active().modelId){
        char oldName[32];
        snprintf(oldName,sizeof(oldName),"%s",active().name);
        snprintf(active().name,sizeof(active().name),"%s",name);
        if(!modelStorage.saveModel(active())){
            snprintf(active().name,sizeof(active().name),"%s",oldName);
            return false;
        }
        updateModelInfo(modelId,active().name);
        return true;
    }
    if(!modelStorage.loadModel(modelId,scratch())) return false;
    if(scratch().modelId!=modelId) return false;
    snprintf(scratch().name,sizeof(scratch().name),"%s",name);
    if(!modelStorage.saveModel(scratch())) return false;
    updateModelInfo(modelId,scratch().name);
    return true;
}
bool ModelManager::duplicateModel(uint16_t sourceModelId,const char *name){
    waitForBackgroundSave();
    if(sourceModelId==0) return false;
    if(modelCount>=MAX_MODELS) return false;
    uint16_t newModelId=findFreeModelId();
    if(newModelId==0) return false;
    if(sourceModelId==active().modelId)
        scratch()=active();
    else if(!modelStorage.loadModel(sourceModelId,scratch()))
        return false;
    scratch().modelId=newModelId;
    if(name&&name[0]!='\0')
        snprintf(scratch().name,sizeof(scratch().name),"%s",name);
    else
        snprintf(scratch().name,sizeof(scratch().name),"Copy %u",newModelId);
    if(!modelStorage.saveModel(scratch())) return false;
    if(!addModelInfo(scratch().modelId,scratch().name)){
        modelStorage.deleteModel(newModelId);
        return false;
    }
    return true;
}
bool ModelManager::deleteModel(uint16_t modelId){
    waitForBackgroundSave();
    if(modelId==0) return false;
    if(modelId==active().modelId) return false;
    if(!modelStorage.deleteModel(modelId)) return false;
    removeModelInfo(modelId);
    return true;
}
uint8_t ModelManager::getModelCount() const{
    return modelCount;
}
const ModelInfo *ModelManager::getModelInfo(uint8_t index) const{
    if(index>=modelCount) return nullptr;
    return &modelList[index];
}
const ModelInfo *ModelManager::findModelInfo(uint16_t modelId) const{
    for(uint8_t i=0;i<modelCount;i++){
        if(modelList[i].modelId==modelId) return &modelList[i];
    }
    return nullptr;
}
bool ModelManager::modelExists(uint16_t modelId) const{
    return findModelInfo(modelId)!=nullptr;
}
bool ModelManager::addModelInfo(uint16_t modelId,const char *name){
    if(modelExists(modelId)) return false;
    if(modelCount>=MAX_MODELS) return false;
    ModelInfo &info=modelList[modelCount];
    info.modelId=modelId;
    if(name&&name[0]!='\0')
        snprintf(info.name,sizeof(info.name),"%s",name);
    else
        snprintf(info.name,sizeof(info.name),"Model %u",modelId);
    modelCount++;
    return true;
}
bool ModelManager::removeModelInfo(uint16_t modelId){
    for(uint8_t i=0;i<modelCount;i++){
        if(modelList[i].modelId!=modelId) continue;
        for(uint8_t j=i;j+1<modelCount;j++)
            modelList[j]=modelList[j+1];
        modelList[modelCount-1]=ModelInfo{};
        modelCount--;
        return true;
    }
    return false;
}
void ModelManager::updateModelInfo(uint16_t modelId,const char *name){
    for(uint8_t i=0;i<modelCount;i++){
        if(modelList[i].modelId!=modelId) continue;
        if(name&&name[0]!='\0')
            snprintf(modelList[i].name,sizeof(modelList[i].name),"%s",name);
        return;
    }
}
void ModelManager::buildDefaultModel(ModelConfig &model) const{
    model.~ModelConfig();
    new (&model) ModelConfig();
    model.version=16;
    model.modelId=0;
    snprintf(model.name,sizeof(model.name),"New Model");
    model.channelCount=64;
    model.flightModes[0].enabled=true;
    snprintf(model.flightModes[0].name,sizeof(model.flightModes[0].name),"Default");
    model.trims[0].enabled=true;
    snprintf(model.trims[0].name,sizeof(model.trims[0].name),"Rudder");
    model.trims[1].enabled=true;
    snprintf(model.trims[1].name,sizeof(model.trims[1].name),"Throttle");
    model.trims[2].enabled=true;
    snprintf(model.trims[2].name,sizeof(model.trims[2].name),"Aileron");
    model.trims[3].enabled=true;
    snprintf(model.trims[3].name,sizeof(model.trims[3].name),"Elevator");
    model.trims[4].enabled=true;
    snprintf(model.trims[4].name,sizeof(model.trims[4].name),"Slider L");
    model.trims[5].enabled=true;
    snprintf(model.trims[5].name,sizeof(model.trims[5].name),"Slider R");
    model.inputs[0].enabled=true;
    snprintf(model.inputs[0].name,sizeof(model.inputs[0].name),"Rudder");
    model.inputs[0].source=Sources::stick(StickFunction::RUDDER);
    model.inputs[0].trimIndex=0;
    model.inputs[1].enabled=true;
    snprintf(model.inputs[1].name,sizeof(model.inputs[1].name),"Throttle");
    model.inputs[1].source=Sources::stick(StickFunction::THROTTLE);
    model.inputs[1].trimIndex=1;
    model.inputs[2].enabled=true;
    snprintf(model.inputs[2].name,sizeof(model.inputs[2].name),"Aileron");
    model.inputs[2].source=Sources::stick(StickFunction::AILERON);
    model.inputs[2].trimIndex=2;
    model.inputs[3].enabled=true;
    snprintf(model.inputs[3].name,sizeof(model.inputs[3].name),"Elevator");
    model.inputs[3].source=Sources::stick(StickFunction::ELEVATOR);
    model.inputs[3].trimIndex=3;
    model.inputs[4].enabled=true;
    snprintf(model.inputs[4].name,sizeof(model.inputs[4].name),"Slider L");
    model.inputs[4].source=Sources::analog(AnalogInput::SLIDER_L);
    model.inputs[4].trimIndex=4;
    model.inputs[5].enabled=true;
    snprintf(model.inputs[5].name,sizeof(model.inputs[5].name),"Slider R");
    model.inputs[5].source=Sources::analog(AnalogInput::SLIDER_R);
    model.inputs[5].trimIndex=5;
    snprintf(model.outputs[0].name,sizeof(model.outputs[0].name),"Rudder");
    snprintf(model.outputs[1].name,sizeof(model.outputs[1].name),"Throttle");
    snprintf(model.outputs[2].name,sizeof(model.outputs[2].name),"Aileron");
    snprintf(model.outputs[3].name,sizeof(model.outputs[3].name),"Elevator");
    model.mixes[0].enabled=true;
    model.mixes[0].destination=0;
    model.mixes[0].source=Sources::modelInput(0);
    model.mixes[0].weight=1000;
    model.mixes[0].operation=MixOperation::ADD;
    model.mixes[1].enabled=true;
    model.mixes[1].destination=1;
    model.mixes[1].source=Sources::modelInput(1);
    model.mixes[1].weight=1000;
    model.mixes[1].operation=MixOperation::ADD;
    model.mixes[2].enabled=true;
    model.mixes[2].destination=2;
    model.mixes[2].source=Sources::modelInput(2);
    model.mixes[2].weight=1000;
    model.mixes[2].operation=MixOperation::ADD;
    model.mixes[3].enabled=true;
    model.mixes[3].destination=3;
    model.mixes[3].source=Sources::modelInput(3);
    model.mixes[3].weight=1000;
    model.mixes[3].operation=MixOperation::ADD;
}
bool ModelManager::reloadModelList(){
    waitForBackgroundSave();
    modelCount=0;
    addModelInfo(0,"New Model");
    if(!modelStorage.isReady()) return false;
    uint16_t ids[MAX_MODELS];
    uint8_t count=modelStorage.getModelIds(ids,MAX_MODELS);
    for(uint8_t i=0;i<count;i++){
        if(!modelStorage.loadModel(ids[i],scratch())) continue;
        if(scratch().modelId!=ids[i]) continue;
        addModelInfo(scratch().modelId,scratch().name);
    }
    return true;
}