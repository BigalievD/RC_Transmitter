#include "ModelStorage.h"
#include "sd_card.h"
#include <FS.h>
#include <SD_MMC.h>
#include <string.h>
#include <esp_heap_caps.h>
#include <new>
ModelStorage modelStorage;
class SdDmaGuard{
public:
    SdDmaGuard(){
        sd_card_release_dma_reserve();
    }
    ~SdDmaGuard(){
        sd_card_restore_dma_reserve();
    }
};
template<typename T>
static T *psramNew(){
    void *mem=heap_caps_malloc(sizeof(T),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!mem) return nullptr;
    return new (mem) T();
}
template<typename T>
static void psramDelete(T *ptr){
    if(!ptr) return;
    ptr->~T();
    heap_caps_free(ptr);
}
bool ModelStorage::begin(){
    ready=false;
    if(!sd_card_is_ready()) return false;
    if(!SD_MMC.exists(MODELS_DIR)&&!SD_MMC.mkdir(MODELS_DIR)) return false;
    ready=true;
    return true;
}
bool ModelStorage::isReady() const{return ready&&sd_card_is_ready();}
void ModelStorage::makeModelPath(uint16_t modelId,char *buffer,size_t size) const{snprintf(buffer,size,"%s/model_%u.bin",MODELS_DIR,modelId);}
void ModelStorage::makeTempPath(uint16_t modelId,char *buffer,size_t size) const{snprintf(buffer,size,"%s/model_%u.tmp",MODELS_DIR,modelId);}
void ModelStorage::makeBackupPath(uint16_t modelId,char *buffer,size_t size) const{snprintf(buffer,size,"%s/model_%u.bak",MODELS_DIR,modelId);}
uint32_t ModelStorage::calculateCRC32(const uint8_t *data,size_t size) const{
    uint32_t crc=0xFFFFFFFF;
    for(size_t i=0;i<size;i++){
        crc^=data[i];
        for(uint8_t bit=0;bit<8;bit++) crc=(crc>>1)^((crc&1)?0xEDB88320:0);
    }
    return ~crc;
}
bool ModelStorage::validateFile(const char *path,uint16_t expectedModelId) const{
    if(!isReady()||!path) return false;
    File file=SD_MMC.open(path,FILE_READ);
    if(!file||file.isDirectory()){if(file) file.close();return false;}
    ModelFileHeader header{};
    if(file.size()<sizeof(header)||!sd_card_read(file,&header,sizeof(header))){file.close();return false;}
    if(header.magic!=FILE_MAGIC||header.formatVersion!=FORMAT_VERSION||header.headerSize!=sizeof(ModelFileHeader)||header.modelId!=expectedModelId){file.close();return false;}
    bool known=
        (header.modelVersion==16&&header.payloadSize==sizeof(ModelConfig))||
        (header.modelVersion==15&&header.payloadSize==sizeof(LegacyModelConfigV15))||
        ((header.modelVersion>=9&&header.modelVersion<=14)&&header.payloadSize==sizeof(LegacyModelConfigV14))||
        (header.modelVersion==8&&header.payloadSize==sizeof(LegacyModelConfigV8))||
        (header.modelVersion==7&&header.payloadSize==sizeof(LegacyModelConfigV7))||
        (header.modelVersion==6&&header.payloadSize==sizeof(LegacyModelConfigV6))||
        (header.modelVersion==5&&header.payloadSize==sizeof(LegacyModelConfigV5))||
        (header.modelVersion==4&&header.payloadSize==sizeof(LegacyModelConfigV4))||
        (header.modelVersion==3&&header.payloadSize==sizeof(LegacyModelConfigV3))||
        (header.modelVersion==2&&header.payloadSize==sizeof(LegacyModelConfigV2));
    if(!known||file.size()!=sizeof(ModelFileHeader)+header.payloadSize){file.close();return false;}
    uint32_t crc=0xFFFFFFFF;
    uint8_t buffer[256];
    uint32_t remaining=header.payloadSize;
    while(remaining){
        size_t chunk=remaining>sizeof(buffer)?sizeof(buffer):remaining;
        if(!sd_card_read(file,buffer,chunk)){file.close();return false;}
        size_t count=chunk;
        for(size_t i=0;i<count;i++){
            crc^=buffer[i];
            for(uint8_t bit=0;bit<8;bit++) crc=(crc>>1)^((crc&1)?0xEDB88320:0);
        }
        remaining-=count;
    }
    file.close();
    return ~crc==header.crc32;
}
bool ModelStorage::saveModel(const ModelConfig &model){
    if(!isReady()||model.modelId==0||model.version!=CURRENT_MODEL_VERSION) return false;
    SdDmaGuard dmaGuard;
    Serial.printf("BEFORE SAVE: free DMA=%u, largest DMA=%u\n",
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    char path[48],tempPath[48],backupPath[48];
    makeModelPath(model.modelId,path,sizeof(path));
    makeTempPath(model.modelId,tempPath,sizeof(tempPath));
    makeBackupPath(model.modelId,backupPath,sizeof(backupPath));
    if(SD_MMC.exists(tempPath)) SD_MMC.remove(tempPath);
    if(SD_MMC.exists(backupPath)) SD_MMC.remove(backupPath);
    ModelFileHeader header{};
    header.magic=FILE_MAGIC;
    header.formatVersion=FORMAT_VERSION;
    header.headerSize=sizeof(ModelFileHeader);
    header.modelVersion=model.version;
    header.payloadSize=sizeof(ModelConfig);
    header.crc32=calculateCRC32(reinterpret_cast<const uint8_t *>(&model),sizeof(ModelConfig));
    header.modelId=model.modelId;
    File file=SD_MMC.open(tempPath,FILE_WRITE);
    if(!file) return false;
    bool hw=sd_card_write(file,&header,sizeof(header));
    Serial.printf("SAVE header write: %s\n",hw?"OK":"FAIL");
    bool pw=hw&&sd_card_write(file,&model,sizeof(ModelConfig));
    Serial.printf("SAVE payload write: %s\n",pw?"OK":"FAIL");
    file.flush();
    Serial.printf("SAVE flush done\n");
    file.close();
    bool validTmp=validateFile(tempPath,model.modelId);
    Serial.printf("SAVE tmp validate: %s\n",validTmp?"OK":"FAIL");
    if(!hw||!pw||!validTmp){
        SD_MMC.remove(tempPath);
        return false;
    }
    bool hadOld=SD_MMC.exists(path);
    if(hadOld&&!SD_MMC.rename(path,backupPath)){SD_MMC.remove(tempPath);return false;}
    Serial.println("SAVE rename tmp->bin");
    if(!SD_MMC.rename(tempPath,path)){
        if(hadOld&&SD_MMC.exists(backupPath)) SD_MMC.rename(backupPath,path);
        if(SD_MMC.exists(tempPath)) SD_MMC.remove(tempPath);
        return false;
    }
    // temp-файл уже полностью проверен CRC до rename.
    // rename не меняет содержимое файла, поэтому повторное полное чтение final
    // только удваивает время SD-операции.
    if(SD_MMC.exists(backupPath)) SD_MMC.remove(backupPath);
    return true;
}
void ModelStorage::migrateLegacyCurves(const LegacyCurveConfigV4 *oldCurves,CurveConfig *newCurves) const{
    for(uint8_t i=0;i<MAX_MODEL_CURVES;i++) newCurves[i]=CurveConfig{};
    for(uint8_t i=0;i<LEGACY_MAX_CURVES;i++){
        const LegacyCurveConfigV4 &oldCurve=oldCurves[i];
        CurveConfig &curve=newCurves[i];
        curve.enabled=oldCurve.enabled;
        memcpy(curve.name,oldCurve.name,sizeof(curve.name));
        curve.name[sizeof(curve.name)-1]='\0';
        curve.type=CurveType::CUSTOM;
        curve.weight=1000;
        curve.offset=0;
        curve.expo=40;
        curve.function=CurveFunction::X_POSITIVE;
        curve.smooth=false;
        curve.easyMode=oldCurve.type==0;
        curve.pointCount=(uint8_t)constrain((int32_t)oldCurve.pointCount,2L,(int32_t)LEGACY_MAX_CURVE_POINTS);
        for(uint8_t p=0;p<LEGACY_MAX_CURVE_POINTS;p++){
            curve.x[p]=oldCurve.x[p];
            curve.y[p]=oldCurve.y[p];
        }
        if(curve.easyMode){
            for(uint8_t p=0;p<curve.pointCount;p++)
                curve.x[p]=(int16_t)(-1000L+(2000L*p)/(curve.pointCount-1));
        }
    }
}
void ModelStorage::migrateLegacyMix(const LegacyMixLineV6 &oldMix,MixLine &mix) const{
    mix=MixLine{};
    mix.enabled=oldMix.enabled;
    mix.destination=oldMix.destination;
    mix.source=oldMix.source;
    mix.weight=oldMix.weight;
    mix.offset=oldMix.offset;
    mix.operation=(oldMix.operation<=static_cast<uint8_t>(MixOperation::REPLACE))
        ? static_cast<MixOperation>(oldMix.operation)
        : MixOperation::ADD;
    mix.curveIndex=oldMix.curveIndex;
    mix.condition=oldMix.condition;
    mix.slowUp=oldMix.slowUp;
    mix.slowDown=oldMix.slowDown;
    mix.delayUp=oldMix.delayUp;
    mix.delayDown=oldMix.delayDown;

    // New v7 fields use ETHOS-like defaults.
    mix.flightModeMask=0;
    mix.differential=0;
    mix.trimIndex=-1;
    mix.reversed=false;
    mix.outputCount=1;
    mix.mixOutputs[0].channel=oldMix.destination;
    mix.mixOutputs[0].reversed=false;
}


void ModelStorage::migrateLegacyMixV7(const LegacyMixLineV7 &oldMix,MixLine &mix) const{
    mix=MixLine{};
    mix.enabled=oldMix.enabled;
    memcpy(mix.name,oldMix.name,sizeof(mix.name));
    mix.name[sizeof(mix.name)-1]='\0';
    mix.destination=oldMix.destination;
    mix.source=oldMix.source;
    mix.operation=oldMix.operation;
    mix.condition=oldMix.condition;
    mix.flightModeMask=oldMix.flightModeMask;
    mix.slowUp=oldMix.slowUp;
    mix.slowDown=oldMix.slowDown;
    mix.curveIndex=oldMix.curveIndex;
    mix.weight=oldMix.weight;
    mix.differential=oldMix.differential;
    mix.offset=oldMix.offset;
    mix.trimIndex=oldMix.trimIndex;
    mix.reversed=oldMix.reversed;
    mix.delayUp=oldMix.delayUp;
    mix.delayDown=oldMix.delayDown;
    mix.actionCount=0;
    mix.outputCount=1;
    mix.mixOutputs[0].channel=oldMix.destination;
    mix.mixOutputs[0].reversed=oldMix.reversed;
}



void ModelStorage::migrateLegacyMixV8(const LegacyMixLineV8 &oldMix,MixLine &mix) const{
    mix=MixLine{};
    mix.enabled=oldMix.enabled;
    memcpy(mix.name,oldMix.name,sizeof(mix.name));
    mix.name[sizeof(mix.name)-1]='\0';
    mix.destination=oldMix.destination;
    mix.source=oldMix.source;
    mix.operation=oldMix.operation;
    mix.condition=oldMix.condition;
    mix.flightModeMask=oldMix.flightModeMask;
    mix.actionCount=oldMix.actionCount>MAX_MIX_ACTIONS?MAX_MIX_ACTIONS:oldMix.actionCount;
    for(uint8_t i=0;i<mix.actionCount;i++) mix.actions[i]=oldMix.actions[i];
    mix.slowUp=oldMix.slowUp;
    mix.slowDown=oldMix.slowDown;
    mix.curveIndex=oldMix.curveIndex;
    mix.weight=oldMix.weight;
    mix.differential=oldMix.differential;
    mix.offset=oldMix.offset;
    mix.trimIndex=oldMix.trimIndex;
    mix.reversed=oldMix.reversed;
    mix.delayUp=oldMix.delayUp;
    mix.delayDown=oldMix.delayDown;

    mix.outputCount=1;
    mix.mixOutputs[0].channel=oldMix.destination;
    mix.mixOutputs[0].reversed=oldMix.reversed;
}

bool ModelStorage::migrateV2(const LegacyModelConfigV2 &oldModel,ModelConfig &model) const{
    model=ModelConfig{};
    model.version=16;
    model.modelId=oldModel.modelId;
    memcpy(model.name,oldModel.name,sizeof(model.name));
    model.name[sizeof(model.name)-1]='\0';
    model.channelCount=64;
    for(uint8_t i=0;i<48;i++){
        memcpy(model.outputs[i].name,oldModel.outputs[i].name,sizeof(model.outputs[i].name));
        model.outputs[i].name[sizeof(model.outputs[i].name)-1]='\0';
        model.outputs[i].reversed=oldModel.outputs[i].reversed;
        model.outputs[i].expo=oldModel.outputs[i].expo;
        model.outputs[i].curveIndex=-1;
        model.outputs[i].min=oldModel.outputs[i].min;
        model.outputs[i].max=oldModel.outputs[i].max;
        model.outputs[i].subtrim=oldModel.outputs[i].subtrim;
        model.outputs[i].pwmCenterUs=oldModel.outputs[i].pwmCenterUs;
        model.outputs[i].slowUp=0;
        model.outputs[i].slowDown=0;
        model.outputs[i].failsafeMode=oldModel.outputs[i].failsafeMode;
        model.outputs[i].failsafeValue=oldModel.outputs[i].failsafeValue;
        model.outputs[i].balance=oldModel.outputs[i].balance;
    }
    for(uint8_t i=0;i<MAX_MODEL_INPUTS;i++) model.inputs[i]=oldModel.inputs[i];
    for(uint8_t i=0;i<MAX_MODEL_TRIMS;i++) model.trims[i]=oldModel.trims[i];
    for(uint8_t i=0;i<MAX_MIX_LINES;i++) migrateLegacyMix(oldModel.mixes[i],model.mixes[i]);
    migrateLegacyCurves(oldModel.curves,model.curves);
    for(uint8_t i=0;i<MAX_FLIGHT_MODES;i++) model.flightModes[i]=oldModel.flightModes[i];
    for(uint8_t i=0;i<MAX_LOGICAL_SWITCHES;i++) model.logicalSwitches[i]=oldModel.logicalSwitches[i];
    normalizeLegacyMixActiveConditions(model);
    return true;
}
bool ModelStorage::migrateV3(const LegacyModelConfigV3 &oldModel,ModelConfig &model) const{
    model=ModelConfig{};
    model.version=16;
    model.modelId=oldModel.modelId;
    memcpy(model.name,oldModel.name,sizeof(model.name));
    model.name[sizeof(model.name)-1]='\0';
    model.channelCount=64;
    for(uint8_t i=0;i<48;i++){
        memcpy(model.outputs[i].name,oldModel.outputs[i].name,sizeof(model.outputs[i].name));
        model.outputs[i].name[sizeof(model.outputs[i].name)-1]='\0';
        model.outputs[i].reversed=oldModel.outputs[i].reversed;
        model.outputs[i].expo=oldModel.outputs[i].expo;
        model.outputs[i].curveIndex=-1;
        model.outputs[i].min=oldModel.outputs[i].min;
        model.outputs[i].max=oldModel.outputs[i].max;
        model.outputs[i].subtrim=oldModel.outputs[i].subtrim;
        model.outputs[i].pwmCenterUs=oldModel.outputs[i].pwmCenterUs;
        model.outputs[i].slowUp=oldModel.outputs[i].slowUp;
        model.outputs[i].slowDown=oldModel.outputs[i].slowDown;
        model.outputs[i].failsafeMode=oldModel.outputs[i].failsafeMode;
        model.outputs[i].failsafeValue=oldModel.outputs[i].failsafeValue;
        model.outputs[i].balance=oldModel.outputs[i].balance;
    }
    for(uint8_t i=0;i<MAX_MODEL_INPUTS;i++) model.inputs[i]=oldModel.inputs[i];
    for(uint8_t i=0;i<MAX_MODEL_TRIMS;i++) model.trims[i]=oldModel.trims[i];
    for(uint8_t i=0;i<MAX_MIX_LINES;i++) migrateLegacyMix(oldModel.mixes[i],model.mixes[i]);
    migrateLegacyCurves(oldModel.curves,model.curves);
    for(uint8_t i=0;i<MAX_FLIGHT_MODES;i++) model.flightModes[i]=oldModel.flightModes[i];
    for(uint8_t i=0;i<MAX_LOGICAL_SWITCHES;i++) model.logicalSwitches[i]=oldModel.logicalSwitches[i];
    normalizeLegacyMixActiveConditions(model);
    return true;
}
bool ModelStorage::migrateV4(const LegacyModelConfigV4 &oldModel,ModelConfig &model) const{
    model=ModelConfig{};
    model.version=16;
    model.modelId=oldModel.modelId;
    memcpy(model.name,oldModel.name,sizeof(model.name));
    model.name[sizeof(model.name)-1]='\0';
    model.channelCount=64;
    for(uint8_t i=0;i<48;i++){
        memcpy(model.outputs[i].name,oldModel.outputs[i].name,sizeof(model.outputs[i].name));
        model.outputs[i].name[sizeof(model.outputs[i].name)-1]='\0';
        model.outputs[i].reversed=oldModel.outputs[i].reversed;
        model.outputs[i].expo=oldModel.outputs[i].expo;
        model.outputs[i].curveIndex=oldModel.outputs[i].curveIndex;
        model.outputs[i].min=oldModel.outputs[i].min;
        model.outputs[i].max=oldModel.outputs[i].max;
        model.outputs[i].subtrim=oldModel.outputs[i].subtrim;
        model.outputs[i].pwmCenterUs=oldModel.outputs[i].pwmCenterUs;
        model.outputs[i].slowUp=oldModel.outputs[i].slowUp;
        model.outputs[i].slowDown=oldModel.outputs[i].slowDown;
        model.outputs[i].failsafeMode=oldModel.outputs[i].failsafeMode;
        model.outputs[i].failsafeValue=oldModel.outputs[i].failsafeValue;
        model.outputs[i].balance=oldModel.outputs[i].balance;
    }
    for(uint8_t i=0;i<MAX_MODEL_INPUTS;i++) model.inputs[i]=oldModel.inputs[i];
    for(uint8_t i=0;i<MAX_MODEL_TRIMS;i++) model.trims[i]=oldModel.trims[i];
    for(uint8_t i=0;i<MAX_MIX_LINES;i++) migrateLegacyMix(oldModel.mixes[i],model.mixes[i]);
    migrateLegacyCurves(oldModel.curves,model.curves);
    for(uint8_t i=0;i<MAX_FLIGHT_MODES;i++) model.flightModes[i]=oldModel.flightModes[i];
    for(uint8_t i=0;i<MAX_LOGICAL_SWITCHES;i++) model.logicalSwitches[i]=oldModel.logicalSwitches[i];
    normalizeLegacyMixActiveConditions(model);
    return true;
}
bool ModelStorage::migrateV5(const LegacyModelConfigV5 &oldModel,ModelConfig &model) const{
    model=ModelConfig{};
    model.version=16;
    model.modelId=oldModel.modelId;
    memcpy(model.name,oldModel.name,sizeof(model.name));
    model.name[sizeof(model.name)-1]='\0';
    model.channelCount=64;
    for(uint8_t i=0;i<48;i++) model.outputs[i]=oldModel.outputs[i];
    for(uint8_t i=0;i<MAX_MODEL_INPUTS;i++) model.inputs[i]=oldModel.inputs[i];
    for(uint8_t i=0;i<MAX_MODEL_TRIMS;i++) model.trims[i]=oldModel.trims[i];
    for(uint8_t i=0;i<MAX_MIX_LINES;i++) migrateLegacyMix(oldModel.mixes[i],model.mixes[i]);
    for(uint8_t i=0;i<MAX_MODEL_CURVES;i++) model.curves[i]=CurveConfig{};
    for(uint8_t i=0;i<LEGACY_MAX_CURVES;i++) model.curves[i]=oldModel.curves[i];
    for(uint8_t i=0;i<MAX_FLIGHT_MODES;i++) model.flightModes[i]=oldModel.flightModes[i];
    for(uint8_t i=0;i<MAX_LOGICAL_SWITCHES;i++) model.logicalSwitches[i]=oldModel.logicalSwitches[i];
    normalizeLegacyMixActiveConditions(model);
    return true;
}

bool ModelStorage::migrateV6(const LegacyModelConfigV6 &oldModel,ModelConfig &model) const{
    model=ModelConfig{};
    model.version=16;
    model.modelId=oldModel.modelId;
    memcpy(model.name,oldModel.name,sizeof(model.name));
    model.name[sizeof(model.name)-1]='\0';
    model.channelCount=64;

    for(uint8_t i=0;i<48;i++) model.outputs[i]=oldModel.outputs[i];
    for(uint8_t i=0;i<MAX_MODEL_INPUTS;i++) model.inputs[i]=oldModel.inputs[i];
    for(uint8_t i=0;i<MAX_MODEL_TRIMS;i++) model.trims[i]=oldModel.trims[i];
    for(uint8_t i=0;i<MAX_MIX_LINES;i++) migrateLegacyMix(oldModel.mixes[i],model.mixes[i]);
    for(uint8_t i=0;i<MAX_MODEL_CURVES;i++) model.curves[i]=oldModel.curves[i];
    for(uint8_t i=0;i<MAX_FLIGHT_MODES;i++) model.flightModes[i]=oldModel.flightModes[i];
    for(uint8_t i=0;i<MAX_LOGICAL_SWITCHES;i++) model.logicalSwitches[i]=oldModel.logicalSwitches[i];

    normalizeLegacyMixActiveConditions(model);
    return true;
}


bool ModelStorage::migrateV7(const LegacyModelConfigV7 &oldModel,ModelConfig &model) const{
    model=ModelConfig{};
    model.version=16;
    model.modelId=oldModel.modelId;
    memcpy(model.name,oldModel.name,sizeof(model.name));
    model.name[sizeof(model.name)-1]='\0';
    model.channelCount=64;

    for(uint8_t i=0;i<48;i++) model.outputs[i]=oldModel.outputs[i];
    for(uint8_t i=0;i<MAX_MODEL_INPUTS;i++) model.inputs[i]=oldModel.inputs[i];
    for(uint8_t i=0;i<MAX_MODEL_TRIMS;i++) model.trims[i]=oldModel.trims[i];
    for(uint8_t i=0;i<MAX_MIX_LINES;i++) migrateLegacyMixV7(oldModel.mixes[i],model.mixes[i]);
    for(uint8_t i=0;i<MAX_MODEL_CURVES;i++) model.curves[i]=oldModel.curves[i];
    for(uint8_t i=0;i<MAX_FLIGHT_MODES;i++) model.flightModes[i]=oldModel.flightModes[i];
    for(uint8_t i=0;i<MAX_LOGICAL_SWITCHES;i++) model.logicalSwitches[i]=oldModel.logicalSwitches[i];

    normalizeLegacyMixActiveConditions(model);
    return true;
}


void ModelStorage::normalizeLegacyMixActiveConditions(ModelConfig &model) const{
    for(uint8_t i=0;i<MAX_MIX_LINES;i++){
        MixLine &mix=model.mixes[i];

        // In v2..v9 top-level NONE meant "no condition", i.e. Always on.
        // v10 stores that state explicitly so it can be distinguished from "---".
        if(mix.condition.type==ConditionType::NONE)
            mix.condition.type=ConditionType::ALWAYS;
    }
}

bool ModelStorage::migrateV9(ModelConfig &model) const{
    if(model.version!=9) return false;

    normalizeLegacyMixActiveConditions(model);
    model.version=16;
    return true;
}

bool ModelStorage::migrateV10(ModelConfig &model) const{
    if(model.version!=10) return false;

    // Binary layout is unchanged. v11 only adds a new persisted
    // ConditionType value for Flight Mode.
    model.version=16;
    return true;
}

bool ModelStorage::migrateV8(const LegacyModelConfigV8 &oldModel,ModelConfig &model) const{
    model=ModelConfig{};
    model.version=16;
    model.modelId=oldModel.modelId;
    memcpy(model.name,oldModel.name,sizeof(model.name));
    model.name[sizeof(model.name)-1]='\0';
    model.channelCount=64;

    for(uint8_t i=0;i<48;i++) model.outputs[i]=oldModel.outputs[i];
    for(uint8_t i=0;i<MAX_MODEL_INPUTS;i++) model.inputs[i]=oldModel.inputs[i];
    for(uint8_t i=0;i<MAX_MODEL_TRIMS;i++) model.trims[i]=oldModel.trims[i];
    for(uint8_t i=0;i<MAX_MIX_LINES;i++) migrateLegacyMixV8(oldModel.mixes[i],model.mixes[i]);
    for(uint8_t i=0;i<MAX_MODEL_CURVES;i++) model.curves[i]=oldModel.curves[i];
    for(uint8_t i=0;i<MAX_FLIGHT_MODES;i++) model.flightModes[i]=oldModel.flightModes[i];
    for(uint8_t i=0;i<MAX_LOGICAL_SWITCHES;i++) model.logicalSwitches[i]=oldModel.logicalSwitches[i];

    normalizeLegacyMixActiveConditions(model);
    return true;
}

bool ModelStorage::migrateV11(ModelConfig &model) const{
    if(model.version!=11) return false;
    model.version=16;
    return true;
}

bool ModelStorage::migrateV12(ModelConfig &model) const{
    if(model.version!=12) return false;

    // Binary layout is unchanged. v13 only adds TRIM_POSITION
    // as a new ConditionType enum value.
    model.version=16;
    return true;
}

bool ModelStorage::migrateV13(ModelConfig &model) const{
    if(model.version!=13) return false;

    // Binary layout is unchanged. v14 only adds SYSTEM_EVENT
    // as a new ConditionType enum value.
    model.version=16;
    return true;
}

bool ModelStorage::migrateV9ToV14(
    const LegacyModelConfigV14 &oldModel,
    ModelConfig &model,
    uint32_t sourceVersion
) const{
    if(sourceVersion<9||sourceVersion>14) return false;
    if(oldModel.version!=sourceVersion) return false;

    model=ModelConfig{};
    model.version=16;
    model.modelId=oldModel.modelId;
    memcpy(model.name,oldModel.name,sizeof(model.name));
    model.name[sizeof(model.name)-1]='\0';
    model.channelCount=64;

    for(uint8_t i=0;i<48;i++) model.outputs[i]=oldModel.outputs[i];
    for(uint8_t i=0;i<MAX_MODEL_INPUTS;i++) model.inputs[i]=oldModel.inputs[i];
    for(uint8_t i=0;i<MAX_MODEL_TRIMS;i++) model.trims[i]=oldModel.trims[i];
    for(uint8_t i=0;i<MAX_MIX_LINES;i++) model.mixes[i]=oldModel.mixes[i];
    for(uint8_t i=0;i<MAX_MODEL_CURVES;i++) model.curves[i]=oldModel.curves[i];
    for(uint8_t i=0;i<MAX_FLIGHT_MODES;i++) model.flightModes[i]=oldModel.flightModes[i];
    for(uint8_t i=0;i<MAX_LOGICAL_SWITCHES;i++) model.logicalSwitches[i]=oldModel.logicalSwitches[i];

    // v9 still used NONE as the top-level "Always on" mix condition.
    if(sourceVersion==9)
        normalizeLegacyMixActiveConditions(model);

    // Vars are new in v15 and intentionally remain default/disabled.
    return true;
}

bool ModelStorage::loadModel(uint16_t modelId,ModelConfig &model){
    if(!isReady()||modelId==0) return false;
    char path[48];
    makeModelPath(modelId,path,sizeof(path));
    if(!validateFile(path,modelId)) return false;
    File file=SD_MMC.open(path,FILE_READ);
    if(!file) return false;
    ModelFileHeader header{};
    if(!sd_card_read(file,&header,sizeof(header))){file.close();return false;}
    if(header.modelVersion==16&&header.payloadSize==sizeof(ModelConfig)){
        bool ok=sd_card_read(file,&model,sizeof(ModelConfig));
        file.close();
        return ok&&model.modelId==modelId&&model.version==16;
    }

    if(header.modelVersion==15&&header.payloadSize==sizeof(LegacyModelConfigV15)){
        LegacyModelConfigV15 *oldModel=psramNew<LegacyModelConfigV15>();
        if(!oldModel){file.close();Serial.println("ModelStorage: no PSRAM for legacy v15");return false;}
        bool ok=sd_card_read(file,oldModel,sizeof(LegacyModelConfigV15));
        file.close();
        if(!ok||oldModel->modelId!=modelId||oldModel->version!=15){psramDelete(oldModel);return false;}
        model=ModelConfig{};
        model.version=16;
        model.modelId=oldModel->modelId;
        memcpy(model.name,oldModel->name,sizeof(model.name));
        for(uint8_t i=0;i<48;i++) model.outputs[i]=oldModel->outputs[i];
        for(uint8_t i=0;i<MAX_MODEL_INPUTS;i++) model.inputs[i]=oldModel->inputs[i];
        for(uint8_t i=0;i<MAX_MODEL_TRIMS;i++) model.trims[i]=oldModel->trims[i];
        for(uint8_t i=0;i<MAX_MIX_LINES;i++) model.mixes[i]=oldModel->mixes[i];
        for(uint8_t i=0;i<MAX_MODEL_CURVES;i++) model.curves[i]=oldModel->curves[i];
        for(uint8_t i=0;i<MAX_FLIGHT_MODES;i++) model.flightModes[i]=oldModel->flightModes[i];
        for(uint8_t i=0;i<MAX_LOGICAL_SWITCHES;i++) model.logicalSwitches[i]=oldModel->logicalSwitches[i];
        for(uint8_t i=0;i<MAX_MODEL_VARS;i++) model.vars[i]=oldModel->vars[i];
        model.channelCount=64;
        psramDelete(oldModel);
        if(saveModel(model)) Serial.printf("Model %u migrated v15 -> v16\n",modelId);
        else Serial.printf("WARNING: model %u migrated in RAM but SD rewrite failed\n",modelId);
        return true;
    }

    if(header.modelVersion>=9&&
       header.modelVersion<=14&&
       header.payloadSize==sizeof(LegacyModelConfigV14)){
        LegacyModelConfigV14 *oldModel=psramNew<LegacyModelConfigV14>();
        if(!oldModel){
            file.close();
            Serial.println("ModelStorage: no PSRAM for legacy v9-v14");
            return false;
        }

        bool ok=sd_card_read(file,oldModel,sizeof(LegacyModelConfigV14));
        file.close();

        uint32_t sourceVersion=header.modelVersion;

        if(!ok||
           oldModel->modelId!=modelId||
           oldModel->version!=sourceVersion||
           !migrateV9ToV14(*oldModel,model,sourceVersion)){
            psramDelete(oldModel);
            return false;
        }

        psramDelete(oldModel);

        if(saveModel(model))
            Serial.printf("Model %u migrated v%u -> v16\n",modelId,(unsigned)sourceVersion);
        else
            Serial.printf("WARNING: model %u migrated in RAM but SD rewrite failed\n",modelId);

        return true;
    }

    if(header.modelVersion==8&&header.payloadSize==sizeof(LegacyModelConfigV8)){
        LegacyModelConfigV8 *oldModel=psramNew<LegacyModelConfigV8>();
        if(!oldModel){file.close();Serial.println("ModelStorage: no PSRAM for legacy v8");return false;}
        bool ok=sd_card_read(file,oldModel,sizeof(LegacyModelConfigV8));
        file.close();
        if(!ok||oldModel->modelId!=modelId||oldModel->version!=8||!migrateV8(*oldModel,model)){psramDelete(oldModel);return false;}
        psramDelete(oldModel);
        if(saveModel(model)) Serial.printf("Model %u migrated v8 -> v16\n",modelId);
        else Serial.printf("WARNING: model %u migrated in RAM but SD rewrite failed\n",modelId);
        return true;
    }

    if(header.modelVersion==7&&header.payloadSize==sizeof(LegacyModelConfigV7)){
        LegacyModelConfigV7 *oldModel=psramNew<LegacyModelConfigV7>();
        if(!oldModel){file.close();Serial.println("ModelStorage: no PSRAM for legacy v7");return false;}
        bool ok=sd_card_read(file,oldModel,sizeof(LegacyModelConfigV7));
        file.close();
        if(!ok||oldModel->modelId!=modelId||oldModel->version!=7||!migrateV7(*oldModel,model)){psramDelete(oldModel);return false;}
        psramDelete(oldModel);
        if(saveModel(model)) Serial.printf("Model %u migrated v7 -> v16\n",modelId);
        else Serial.printf("WARNING: model %u migrated in RAM but SD rewrite failed\n",modelId);
        return true;
    }

    if(header.modelVersion==6&&header.payloadSize==sizeof(LegacyModelConfigV6)){
        LegacyModelConfigV6 *oldModel=psramNew<LegacyModelConfigV6>();
        if(!oldModel){file.close();Serial.println("ModelStorage: no PSRAM for legacy v6");return false;}
        bool ok=sd_card_read(file,oldModel,sizeof(LegacyModelConfigV6));
        file.close();
        if(!ok||oldModel->modelId!=modelId||oldModel->version!=6||!migrateV6(*oldModel,model)){psramDelete(oldModel);return false;}
        psramDelete(oldModel);
        if(saveModel(model)) Serial.printf("Model %u migrated v6 -> v16\n",modelId);
        else Serial.printf("WARNING: model %u migrated in RAM but SD rewrite failed\n",modelId);
        return true;
    }
    if(header.modelVersion==5&&header.payloadSize==sizeof(LegacyModelConfigV5)){
        LegacyModelConfigV5 *oldModel=psramNew<LegacyModelConfigV5>();
        if(!oldModel){file.close();Serial.println("ModelStorage: no PSRAM for legacy v5");return false;}
        bool ok=sd_card_read(file,oldModel,sizeof(LegacyModelConfigV5));
        file.close();
        if(!ok||oldModel->modelId!=modelId||oldModel->version!=5||!migrateV5(*oldModel,model)){psramDelete(oldModel);return false;}
        psramDelete(oldModel);
        if(saveModel(model)) Serial.printf("Model %u migrated v5 -> v16\n",modelId);
        else Serial.printf("WARNING: model %u migrated in RAM but SD rewrite failed\n",modelId);
        return true;
    }
    if(header.modelVersion==4&&header.payloadSize==sizeof(LegacyModelConfigV4)){
        LegacyModelConfigV4 *oldModel=psramNew<LegacyModelConfigV4>();
        if(!oldModel){file.close();Serial.println("ModelStorage: no PSRAM for legacy v4");return false;}
        bool ok=sd_card_read(file,oldModel,sizeof(LegacyModelConfigV4));
        file.close();
        if(!ok||oldModel->modelId!=modelId||oldModel->version!=4||!migrateV4(*oldModel,model)){psramDelete(oldModel);return false;}
        psramDelete(oldModel);
        if(saveModel(model)) Serial.printf("Model %u migrated v4 -> v16\n",modelId);
        else Serial.printf("WARNING: model %u migrated in RAM but SD rewrite failed\n",modelId);
        return true;
    }
    if(header.modelVersion==3&&header.payloadSize==sizeof(LegacyModelConfigV3)){
        LegacyModelConfigV3 *oldModel=psramNew<LegacyModelConfigV3>();
        if(!oldModel){file.close();Serial.println("ModelStorage: no PSRAM for legacy v3");return false;}
        bool ok=sd_card_read(file,oldModel,sizeof(LegacyModelConfigV3));
        file.close();
        if(!ok||oldModel->modelId!=modelId||oldModel->version!=3||!migrateV3(*oldModel,model)){psramDelete(oldModel);return false;}
        psramDelete(oldModel);
        if(saveModel(model)) Serial.printf("Model %u migrated v3 -> v16\n",modelId);
        else Serial.printf("WARNING: model %u migrated in RAM but SD rewrite failed\n",modelId);
        return true;
    }
    if(header.modelVersion==2&&header.payloadSize==sizeof(LegacyModelConfigV2)){
        LegacyModelConfigV2 *oldModel=psramNew<LegacyModelConfigV2>();
        if(!oldModel){file.close();Serial.println("ModelStorage: no PSRAM for legacy v2");return false;}
        bool ok=sd_card_read(file,oldModel,sizeof(LegacyModelConfigV2));
        file.close();
        if(!ok||oldModel->modelId!=modelId||oldModel->version!=2||!migrateV2(*oldModel,model)){psramDelete(oldModel);return false;}
        psramDelete(oldModel);
        if(saveModel(model)) Serial.printf("Model %u migrated v2 -> v16\n",modelId);
        else Serial.printf("WARNING: model %u migrated in RAM but SD rewrite failed\n",modelId);
        return true;
    }
    file.close();
    return false;
}
bool ModelStorage::deleteModel(uint16_t modelId){
    if(!isReady()||modelId==0) return false;
    char path[48],tempPath[48],backupPath[48];
    makeModelPath(modelId,path,sizeof(path));
    makeTempPath(modelId,tempPath,sizeof(tempPath));
    makeBackupPath(modelId,backupPath,sizeof(backupPath));
    bool removed=false;
    if(SD_MMC.exists(path)) removed=SD_MMC.remove(path);
    if(SD_MMC.exists(tempPath)) SD_MMC.remove(tempPath);
    if(SD_MMC.exists(backupPath)) SD_MMC.remove(backupPath);
    return removed;
}
bool ModelStorage::modelExists(uint16_t modelId) const{
    if(!isReady()||modelId==0) return false;
    char path[48];
    makeModelPath(modelId,path,sizeof(path));
    return SD_MMC.exists(path)&&validateFile(path,modelId);
}
bool ModelStorage::parseModelFileName(const char *name,uint16_t &modelId) const{
    if(!name) return false;
    const char *base=strrchr(name,'/');
    if(base) name=base+1;
    unsigned int id=0;
    char extra=0;
    if(sscanf(name,"model_%u.bin%c",&id,&extra)!=1||id==0||id>65535) return false;
    modelId=(uint16_t)id;
    return true;
}
uint8_t ModelStorage::getModelIds(uint16_t *ids,uint8_t maxCount) const{
    if(!isReady()||!ids||maxCount==0){
        Serial.println("getModelIds: storage not ready");
        return 0;
    }
    File dir=SD_MMC.open(MODELS_DIR);
    if(!dir||!dir.isDirectory()){
        Serial.println("getModelIds: cannot open /models");
        if(dir) dir.close();
        return 0;
    }
    Serial.println("getModelIds: scanning /models");
    uint8_t count=0;
    while(count<maxCount){
        File entry=dir.openNextFile();
        if(!entry) break;
        bool isDir=entry.isDirectory();
        char entryName[64];
        snprintf(entryName,sizeof(entryName),"%s",entry.name());
        entry.close();
        Serial.printf("getModelIds: entry='%s' dir=%d\n",entryName,isDir?1:0);
        if(isDir) continue;
        uint16_t modelId=0;
        if(!parseModelFileName(entryName,modelId)){
            Serial.println("getModelIds: filename rejected");
            continue;
        }
        char path[48];
        makeModelPath(modelId,path,sizeof(path));
        bool valid=validateFile(path,modelId);
        Serial.printf("getModelIds: id=%u valid=%d\n",modelId,valid?1:0);
        if(valid) ids[count++]=modelId;
    }
    dir.close();
    Serial.printf("getModelIds: found=%u\n",count);
    return count;
}
