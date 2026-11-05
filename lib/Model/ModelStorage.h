#pragma once
#include <Arduino.h>
#include <FS.h>
#include "ModelConfig.h"
class ModelStorage{
public:
    bool begin();
    bool isReady() const;
    bool saveModel(const ModelConfig &model);
    bool loadModel(uint16_t modelId,ModelConfig &model);
    bool deleteModel(uint16_t modelId);
    bool modelExists(uint16_t modelId) const;
    uint8_t getModelIds(uint16_t *ids,uint8_t maxCount) const;
private:
    static constexpr uint8_t LEGACY_MAX_CURVES=16;
    static constexpr uint8_t LEGACY_MAX_CURVE_POINTS=17;
    struct __attribute__((packed)) ModelFileHeader{
        uint32_t magic;
        uint16_t formatVersion;
        uint16_t headerSize;
        uint32_t modelVersion;
        uint32_t payloadSize;
        uint32_t crc32;
        uint16_t modelId;
        uint16_t reserved;
    };
    struct LegacyCurveConfigV4{
        bool enabled;
        char name[16];
        uint8_t type;
        uint8_t pointCount;
        int16_t x[LEGACY_MAX_CURVE_POINTS];
        int16_t y[LEGACY_MAX_CURVE_POINTS];
    };

    struct LegacyMixLineV6{
        bool enabled;
        uint8_t destination;
        SourceId source;
        int16_t weight;
        int16_t offset;
        uint8_t operation;
        int8_t curveIndex;
        ConditionConfig condition;
        uint16_t slowUp;
        uint16_t slowDown;
        uint16_t delayUp;
        uint16_t delayDown;
    };

    struct LegacyMixLineV7{
        bool enabled;
        char name[16];
        uint8_t destination;
        SourceId source;
        MixOperation operation;
        ConditionConfig condition;
        uint16_t flightModeMask;
        uint16_t slowUp;
        uint16_t slowDown;
        int8_t curveIndex;
        int16_t weight;
        int16_t differential;
        int16_t offset;
        int8_t trimIndex;
        bool reversed;
        uint16_t delayUp;
        uint16_t delayDown;
    };

    struct LegacyMixLineV8{
        bool enabled;
        char name[16];
        uint8_t destination;
        SourceId source;
        MixOperation operation;
        ConditionConfig condition;
        uint16_t flightModeMask;
        uint8_t actionCount;
        MixAction actions[MAX_MIX_ACTIONS];
        uint16_t slowUp;
        uint16_t slowDown;
        int8_t curveIndex;
        int16_t weight;
        int16_t differential;
        int16_t offset;
        int8_t trimIndex;
        bool reversed;
        uint16_t delayUp;
        uint16_t delayDown;
    };
    struct LegacyOutputConfigV2{
        char name[16];
        bool reversed;
        int8_t expo;
        int16_t min;
        int16_t max;
        int16_t subtrim;
        int16_t pwmCenterUs;
        FailsafeMode failsafeMode;
        int16_t failsafeValue;
        BalanceConfig balance;
    };
    struct LegacyOutputConfigV3{
        char name[16];
        bool reversed;
        int8_t expo;
        int16_t min;
        int16_t max;
        int16_t subtrim;
        int16_t pwmCenterUs;
        uint16_t slowUp;
        uint16_t slowDown;
        FailsafeMode failsafeMode;
        int16_t failsafeValue;
        BalanceConfig balance;
    };
    struct LegacyOutputConfigV4{
        char name[16];
        bool reversed;
        int8_t expo;
        int8_t curveIndex;
        int16_t min;
        int16_t max;
        int16_t subtrim;
        int16_t pwmCenterUs;
        uint16_t slowUp;
        uint16_t slowDown;
        FailsafeMode failsafeMode;
        int16_t failsafeValue;
        BalanceConfig balance;
    };
    template<typename TOutput>
    struct LegacyModelConfigBase{
        uint32_t version;
        uint16_t modelId;
        char name[32];
        uint8_t channelCount;
        TOutput outputs[48];
        ModelInputConfig inputs[MAX_MODEL_INPUTS];
        TrimConfig trims[MAX_MODEL_TRIMS];
        LegacyMixLineV6 mixes[MAX_MIX_LINES];
        LegacyCurveConfigV4 curves[LEGACY_MAX_CURVES];
        FlightModeConfig flightModes[MAX_FLIGHT_MODES];
        LogicalSwitchConfig logicalSwitches[MAX_LOGICAL_SWITCHES];
    };
    using LegacyModelConfigV2=LegacyModelConfigBase<LegacyOutputConfigV2>;
    using LegacyModelConfigV3=LegacyModelConfigBase<LegacyOutputConfigV3>;
    using LegacyModelConfigV4=LegacyModelConfigBase<LegacyOutputConfigV4>;
    struct LegacyModelConfigV5{
        uint32_t version;
        uint16_t modelId;
        char name[32];
        uint8_t channelCount;
        OutputConfig outputs[48];
        ModelInputConfig inputs[MAX_MODEL_INPUTS];
        TrimConfig trims[MAX_MODEL_TRIMS];
        LegacyMixLineV6 mixes[MAX_MIX_LINES];
        CurveConfig curves[LEGACY_MAX_CURVES];
        FlightModeConfig flightModes[MAX_FLIGHT_MODES];
        LogicalSwitchConfig logicalSwitches[MAX_LOGICAL_SWITCHES];
    };

    struct LegacyModelConfigV6{
        uint32_t version;
        uint16_t modelId;
        char name[32];
        uint8_t channelCount;
        OutputConfig outputs[48];
        ModelInputConfig inputs[MAX_MODEL_INPUTS];
        TrimConfig trims[MAX_MODEL_TRIMS];
        LegacyMixLineV6 mixes[MAX_MIX_LINES];
        CurveConfig curves[MAX_MODEL_CURVES];
        FlightModeConfig flightModes[MAX_FLIGHT_MODES];
        LogicalSwitchConfig logicalSwitches[MAX_LOGICAL_SWITCHES];
    };

    struct LegacyModelConfigV7{
        uint32_t version;
        uint16_t modelId;
        char name[32];
        uint8_t channelCount;
        OutputConfig outputs[48];
        ModelInputConfig inputs[MAX_MODEL_INPUTS];
        TrimConfig trims[MAX_MODEL_TRIMS];
        LegacyMixLineV7 mixes[MAX_MIX_LINES];
        CurveConfig curves[MAX_MODEL_CURVES];
        FlightModeConfig flightModes[MAX_FLIGHT_MODES];
        LogicalSwitchConfig logicalSwitches[MAX_LOGICAL_SWITCHES];
    };

    struct LegacyModelConfigV14{
        uint32_t version;
        uint16_t modelId;
        char name[32];
        uint8_t channelCount;
        OutputConfig outputs[48];
        ModelInputConfig inputs[MAX_MODEL_INPUTS];
        TrimConfig trims[MAX_MODEL_TRIMS];
        MixLine mixes[MAX_MIX_LINES];
        CurveConfig curves[MAX_MODEL_CURVES];
        FlightModeConfig flightModes[MAX_FLIGHT_MODES];
        LogicalSwitchConfig logicalSwitches[MAX_LOGICAL_SWITCHES];
    };

    struct LegacyModelConfigV8{
        uint32_t version;
        uint16_t modelId;
        char name[32];
        uint8_t channelCount;
        OutputConfig outputs[48];
        ModelInputConfig inputs[MAX_MODEL_INPUTS];
        TrimConfig trims[MAX_MODEL_TRIMS];
        LegacyMixLineV8 mixes[MAX_MIX_LINES];
        CurveConfig curves[MAX_MODEL_CURVES];
        FlightModeConfig flightModes[MAX_FLIGHT_MODES];
        LogicalSwitchConfig logicalSwitches[MAX_LOGICAL_SWITCHES];
    };

    // v15 binary payload used 48 outputs. Preserve its exact layout for migration.
    struct LegacyModelConfigV15{
        uint32_t version;
        uint16_t modelId;
        char name[32];
        uint8_t channelCount;
        OutputConfig outputs[48];
        ModelInputConfig inputs[MAX_MODEL_INPUTS];
        TrimConfig trims[MAX_MODEL_TRIMS];
        MixLine mixes[MAX_MIX_LINES];
        CurveConfig curves[MAX_MODEL_CURVES];
        FlightModeConfig flightModes[MAX_FLIGHT_MODES];
        LogicalSwitchConfig logicalSwitches[MAX_LOGICAL_SWITCHES];
        VarConfig vars[MAX_MODEL_VARS];
    };

    static constexpr const char *MODELS_DIR="/models";
    static constexpr uint32_t FILE_MAGIC=0x52434D44UL;
    static constexpr uint16_t FORMAT_VERSION=1;
    static constexpr uint32_t CURRENT_MODEL_VERSION=16;
    bool ready=false;
    void makeModelPath(uint16_t modelId,char *buffer,size_t size) const;
    void makeTempPath(uint16_t modelId,char *buffer,size_t size) const;
    void makeBackupPath(uint16_t modelId,char *buffer,size_t size) const;
    bool parseModelFileName(const char *name,uint16_t &modelId) const;
    uint32_t calculateCRC32(const uint8_t *data,size_t size) const;
    bool validateFile(const char *path,uint16_t expectedModelId) const;
    void migrateLegacyCurves(const LegacyCurveConfigV4 *oldCurves,CurveConfig *newCurves) const;
    bool migrateV2(const LegacyModelConfigV2 &oldModel,ModelConfig &model) const;
    bool migrateV3(const LegacyModelConfigV3 &oldModel,ModelConfig &model) const;
    bool migrateV4(const LegacyModelConfigV4 &oldModel,ModelConfig &model) const;
    bool migrateV5(const LegacyModelConfigV5 &oldModel,ModelConfig &model) const;
    bool migrateV6(const LegacyModelConfigV6 &oldModel,ModelConfig &model) const;
    bool migrateV7(const LegacyModelConfigV7 &oldModel,ModelConfig &model) const;
    bool migrateV8(const LegacyModelConfigV8 &oldModel,ModelConfig &model) const;
    bool migrateV9(ModelConfig &model) const;
    bool migrateV10(ModelConfig &model) const;
    bool migrateV11(ModelConfig &model) const;
    bool migrateV12(ModelConfig &model) const;
    bool migrateV13(ModelConfig &model) const;
    bool migrateV9ToV14(const LegacyModelConfigV14 &oldModel,ModelConfig &model,uint32_t sourceVersion) const;
    void normalizeLegacyMixActiveConditions(ModelConfig &model) const;
    void migrateLegacyMix(const LegacyMixLineV6 &oldMix,MixLine &mix) const;
    void migrateLegacyMixV7(const LegacyMixLineV7 &oldMix,MixLine &mix) const;
    void migrateLegacyMixV8(const LegacyMixLineV8 &oldMix,MixLine &mix) const;
};
extern ModelStorage modelStorage;
