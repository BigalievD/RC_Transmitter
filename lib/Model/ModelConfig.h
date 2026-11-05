#pragma once
#include <Arduino.h>
#include "../InputManager/Source.h"
static constexpr uint8_t MAX_MODEL_CHANNELS=64;
static constexpr uint8_t MAX_MODEL_INPUTS=32;
static constexpr uint8_t MAX_MODEL_TRIMS=8;
static constexpr uint8_t MAX_MIX_LINES=64;
static constexpr uint8_t MAX_MODEL_CURVES=50;
static constexpr uint8_t MAX_CURVE_POINTS=21;
static constexpr uint8_t MAX_FLIGHT_MODES=9;
static constexpr uint8_t MAX_BALANCE_POINTS=21;
static constexpr uint8_t MAX_LOGICAL_SWITCHES=32;
static constexpr uint8_t MAX_MIX_ACTIONS=16;
static constexpr uint8_t MAX_MIX_ACTION_VARIANTS=10;
static constexpr uint8_t MAX_MIX_OUTPUTS=16;
static constexpr uint8_t MAX_MODEL_VARS=64;
static constexpr uint8_t MAX_VAR_VALUES=10;
static constexpr uint8_t MAX_VAR_ACTIONS=10;
static constexpr uint8_t MIX_OUTPUT_NONE=0xFF;
enum class CurveType:uint8_t{EXPO=0,FUNCTION,CUSTOM};
enum class CurveFunction:uint8_t{
    X_POSITIVE=0,
    X_NEGATIVE=1,
    ABS_X=2,
    F_POSITIVE=3,
    F_NEGATIVE=4,
    ABS_F=5,
    NONE=255
};
struct CurveConfig{
    bool enabled=false;
    char name[16]="";
    CurveType type=CurveType::CUSTOM;
    int16_t weight=1000;
    int16_t offset=0;
    int8_t expo=40;
    CurveFunction function=CurveFunction::X_POSITIVE;
    bool smooth=false;
    bool easyMode=true;
    uint8_t pointCount=5;
    int16_t x[MAX_CURVE_POINTS]={};
    int16_t y[MAX_CURVE_POINTS]={};
};
struct TrimConfig{bool enabled=false;char name[16]="";uint8_t step=10;int16_t min=-250;int16_t max=250;};
struct ModelInputConfig{bool enabled=false;char name[16]="";SourceId source;int16_t weight=1000;int16_t offset=0;int8_t expo=0;bool reversed=false;int8_t curveIndex=-1;int8_t trimIndex=-1;};
enum class ConditionType:uint8_t{
    NONE=0,
    SOURCE_POSITIVE=1,
    SOURCE_NEGATIVE=2,
    SOURCE_ABOVE=3,
    SOURCE_BELOW=4,
    SOURCE_EQUAL=5,
    SOURCE_RANGE=6,
    SOURCE_ABOVE_SOURCE=7,
    SOURCE_BELOW_SOURCE=8,

    // Explicit states used by ETHOS-like Active Condition.
    // Values are appended so old model files keep the same numeric mapping.
    ALWAYS=9,
    NEVER=10,

    // Active only while a specific flight mode is active.
    // condition.value1 stores the flight mode index.
    FLIGHT_MODE=11,

    // FS1..FS6. condition.value1 stores 0..5.
    FUNCTION_SWITCH=12,

    // Physical trim direction button.
    // condition.value1 stores TrimDirection.
    TRIM_POSITION=13,

    // SystemEventId is stored in condition.value1.
    SYSTEM_EVENT=14
};
struct ConditionConfig{
    ConditionType type=ConditionType::NONE;SourceId sourceA;SourceId sourceB;int16_t value1=0;int16_t value2=0;int16_t tolerance=10;bool inverted=false;
    bool isValid() const{return type!=ConditionType::NONE;}
};
enum class LogicalSwitchOperation:uint8_t{CONDITION=0,AND,OR,XOR,NOT};
struct LogicalSwitchConfig{bool enabled=false;char name[16]="";LogicalSwitchOperation operation=LogicalSwitchOperation::CONDITION;ConditionConfig conditionA;ConditionConfig conditionB;};

enum class VarActionFunction:uint8_t{
    ASSIGN=0,
    ADD,
    SUBTRACT,
    MULTIPLY,
    DIVIDE,
    PERCENT,
    MINIMUM,
    MAXIMUM,
    REPURPOSE
};

struct VarValueConfig{
    bool enabled=false;
    ConditionConfig condition;

    // Internal scale: 1000 == 100.0%.
    // ETHOS Var values have one decimal precision.
    int16_t value=0;
};

struct VarActionConfig{
    bool enabled=false;
    ConditionConfig condition;
    VarActionFunction function=VarActionFunction::ASSIGN;

    // Arithmetic parameter with one decimal precision.
    // Internal scale: 10 == 1.0.
    int16_t parameter=0;

    // Reserved for REPURPOSE. -1 means not assigned yet.
    int8_t repurposeTrimIndex=-1;

    // Edge: fire only on inactive -> active transition. Default OFF.
    // Stored in the existing tail-padding byte (verify layout below).
    bool edge=false;
};
static_assert(sizeof(VarActionConfig)==22,"VarActionConfig layout changed: model storage migration required");

struct VarConfig{
    bool enabled=false;
    char name[16]="";
    char comment[64]="";

    // Internal scale: 1000 == 100.0%.
    // ETHOS allows one decimal precision and a range inside +/-500.0%.
    int16_t rangeMin=-1000;
    int16_t rangeMax=1000;

    uint8_t valueCount=0;
    VarValueConfig values[MAX_VAR_VALUES];

    uint8_t actionCount=0;
    VarActionConfig actions[MAX_VAR_ACTIONS];
};
enum class MixOperation:uint8_t{ADD=0,MULTIPLY,REPLACE,LOCK};

static constexpr int8_t OUTPUT_CURVE_LEGACY=-1;
static constexpr int8_t OUTPUT_CURVE_NONE=-2;
static constexpr int8_t OUTPUT_CURVE_EXPO=-3;

enum class MixActionType:uint8_t{
    CURVE=0,
    WEIGHT,
    DIFFERENTIAL,
    OFFSET,
    SLOW,
    TRIM
};

struct MixActionVariant{
    bool enabled=false;
    ConditionConfig condition;

    // Weight: internal 1000 == 100%.
    int16_t value=0;

    // Curve: OUTPUT_CURVE_NONE / OUTPUT_CURVE_EXPO / 0..MAX_MODEL_CURVES-1.
    int8_t curveIndex=OUTPUT_CURVE_NONE;
    int8_t expo=0;
};

struct MixAction{
    bool enabled=false;
    MixActionType type=MixActionType::WEIGHT;

    // Overall action condition. For OFFSET it selects active/inactive value.
    ConditionConfig condition;

    // WEIGHT/CURVE use variants. Slot 0 is Otherwise.
    uint8_t variantCount=0;
    MixActionVariant variants[MAX_MIX_ACTION_VARIANTS];

    // DIFFERENTIAL: value1 = -1000..1000.
    // OFFSET: value1 = active, value2 = inactive.
    // SLOW: value1 = up ms, value2 = down ms.
    int16_t value1=0;
    int16_t value2=0;

    // TRIM.
    int8_t trimIndex=-1;
};


struct MixOutput{
    uint8_t channel=MIX_OUTPUT_NONE;
    bool reversed=false;
};

struct MixLine{
    bool enabled=false;
    char name[16]="";
    // Legacy single-output fields are kept for v2..v8 migration only.
    uint8_t destination=0;
    SourceId source;

    MixOperation operation=MixOperation::ADD;
    ConditionConfig condition;

    // ETHOS-like outputs. outputCount controls visible/active slots.
    // channel == MIX_OUTPUT_NONE means the slot is not assigned yet.
    uint8_t outputCount=1;
    MixOutput mixOutputs[MAX_MIX_OUTPUTS];

    // 0 = active in all flight modes.
    // Otherwise bit N enables the mix in flight mode N.
    uint16_t flightModeMask=0;

    // Ordered ETHOS-like action list. Repeated action types are allowed.
    // If actionCount == 0, legacy fixed fields below are used.
    uint8_t actionCount=0;
    MixAction actions[MAX_MIX_ACTIONS];

    // Legacy fixed action chain kept for v2..v7 migration and transitional compatibility:
    // Slow -> Curve -> Weight -> Differential -> Offset -> Trim
    uint16_t slowUp=0;
    uint16_t slowDown=0;
    int8_t curveIndex=-1;
    int16_t weight=1000;
    int16_t differential=0;
    int16_t offset=0;
    int8_t trimIndex=-1;

    // Legacy single-output reverse kept for v2..v8 migration.
    bool reversed=false;

    // Kept for compatibility with the existing condition transition logic.
    uint16_t delayUp=0;
    uint16_t delayDown=0;
};
struct BalanceConfig{bool enabled=false;bool smooth=false;uint8_t pointCount=5;int16_t correction[MAX_BALANCE_POINTS]={};};
enum class FailsafeMode:uint8_t{HOLD=0,CUSTOM,NOPULSE};
struct OutputConfig{
    char name[16]="";
    bool reversed=false;
    int8_t expo=0;
    int8_t curveIndex=OUTPUT_CURVE_NONE;
    int16_t min=-1000;
    int16_t max=1000;
    int16_t subtrim=0;
    int16_t pwmCenterUs=1500;
    uint16_t slowUp=0;
    uint16_t slowDown=0;
    FailsafeMode failsafeMode=FailsafeMode::HOLD;
    int16_t failsafeValue=0;
    BalanceConfig balance;
};
struct FlightModeConfig{bool enabled=false;
    char name[16]="";
    ConditionConfig condition;
    int16_t trims[MAX_MODEL_TRIMS]={};
};
struct ModelConfig{
    uint32_t version=16;
    uint16_t modelId=0;
    char name[32]="New Model";
    uint8_t channelCount=64;
    OutputConfig outputs[MAX_MODEL_CHANNELS];
    ModelInputConfig inputs[MAX_MODEL_INPUTS];
    TrimConfig trims[MAX_MODEL_TRIMS];
    MixLine mixes[MAX_MIX_LINES];
    CurveConfig curves[MAX_MODEL_CURVES];
    FlightModeConfig flightModes[MAX_FLIGHT_MODES];
    LogicalSwitchConfig logicalSwitches[MAX_LOGICAL_SWITCHES];
    VarConfig vars[MAX_MODEL_VARS];
};
