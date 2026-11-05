#pragma once
#include <Arduino.h>
#include "ModelConfig.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
static constexpr uint8_t MAX_MODELS=32;
struct ModelInfo{
    uint16_t modelId=0;
    char name[32]="";
};
class ModelManager{
public:
    void begin();
    ModelConfig &getActiveModel();
    const ModelConfig &getActiveModel() const;
    uint16_t getActiveModelId() const;
    void createDefaultModel();
    bool createModel(const char *name);
    bool createModel(uint16_t modelId,const char *name);
    bool loadModel(uint16_t modelId);
    bool selectModel(uint16_t modelId);
    bool saveActiveModel();
    bool requestSaveActiveModel();
    void setEditorSaveDeferred(bool enabled);
    bool flushDeferredEditorSave();
    bool checkpointActiveModel();
    bool waitForBackgroundSave(uint32_t timeoutMs=10000);
    bool renameModel(uint16_t modelId,const char *name);
    bool duplicateModel(uint16_t sourceModelId,const char *name);
    bool deleteModel(uint16_t modelId);
    void activateModel(const ModelConfig &model);
    void resetRuntime();
    uint8_t getModelCount() const;
    const ModelInfo *getModelInfo(uint8_t index) const;
    const ModelInfo *findModelInfo(uint16_t modelId) const;
    bool modelExists(uint16_t modelId) const;
    bool reloadModelList();
    bool loadLastActiveModel();
private:
    void buildDefaultModel(ModelConfig &model) const;
    uint16_t findFreeModelId() const;
    bool addModelInfo(uint16_t modelId,const char *name);
    bool removeModelInfo(uint16_t modelId);
    void updateModelInfo(uint16_t modelId,const char *name);
    void saveActiveModelId();
    uint16_t loadActiveModelId() const;
    bool ensureModels();
    bool ensureBackgroundSave();
    static void saveTaskThunk(void *arg);
    void saveTaskLoop();
    ModelConfig &active();
    const ModelConfig &active() const;
    ModelConfig &scratch();
    ModelConfig *activeModel=nullptr;
    ModelConfig *scratchModel=nullptr;
    ModelConfig *saveSnapshots[2]={nullptr,nullptr};
    TaskHandle_t saveTaskHandle=nullptr;
    SemaphoreHandle_t saveMutex=nullptr;
    volatile bool saveTaskBusy=false;
    volatile bool savePending=false;
    uint8_t savingSnapshot=0;
    uint8_t pendingSnapshot=0;
    bool lastBackgroundSaveOk=true;
    bool editorSaveDeferred=false;
    bool editorModelDirty=false;
    ModelInfo modelList[MAX_MODELS]={};
    uint8_t modelCount=0;
};
extern ModelManager modelManager;
