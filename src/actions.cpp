#include "HalfRangeTestInput.h"
#include "FunctionSwitchManager.h"
#include "TrimSwitchManager.h"
#include "SystemEventManager.h"
#include "LogicalSwitchManager.h"
#include <cstdio>
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_heap_caps.h>

#include "ui/actions.h"
#include "ui/ui.h"
#include "ui/vars.h"
#include "ui/styles.h"
#include "ui/fonts.h"
#include "actions.h"
#include <display.h>
#include <RAM.h>
#include "Variables.h"
#include "uart_transmit.h"
#include <Wire.h>
#include "calibration.h"
#include "sd_file_manager.h"
#include "FrSkyEncoder.h"
#include "ColorPicker.h"
#include "Mixer.h"
#include "ModelManager.h"
#include "OutputProcessor.h"
#include "InputManager.h"
#include "ModelInputProcessor.h"
#include "CurveProcessor.h"
#include "BalanceProcessor.h"
#include "ConditionEvaluator.h"
#include "VarProcessor.h"

// Keep text in Montserrat and resolve missing arrow/Edge glyphs from the EEZ icon font.
// Font descriptors are cached for the lifetime of LVGL objects; no per-label allocation.
static void setConditionFontFallback(lv_obj_t *label){
    if(!label) return;
    const lv_font_t *base=lv_obj_get_style_text_font(label,LV_PART_MAIN);
    if(!base||base==&ui_font_arrow) return;
    struct Entry{const lv_font_t *base;lv_font_t combined;};
    static Entry fonts[16]={};
    static uint8_t used=0;
    for(uint8_t i=0;i<used;i++){
        if(base==&fonts[i].combined) return;
        if(base==fonts[i].base){lv_obj_set_style_text_font(label,&fonts[i].combined,LV_PART_MAIN);return;}
    }
    if(used>=16) return;
    Entry &entry=fonts[used++];
    entry.base=base;
    entry.combined=*base;
    entry.combined.fallback=&ui_font_arrow;
    lv_obj_set_style_text_font(label,&entry.combined,LV_PART_MAIN);
}

static void setConditionText(lv_obj_t *label,const char *text){
    if(!label) return;
    setConditionFontFallback(label);
    lv_label_set_text(label,text);
}

int32_t get_sticks(int ch);

static void createLeftStickLine();
static void createLeftStickCircle();
static void updateLeftStickCircle(int xValue, int yValue);
static void createRightStickLine();
static void createRightStickCircle();
static void updateRightStickCircle(int xValue, int yValue);
static void calibration_timer_cb(lv_timer_t *timer);
static void start_stick_calibration();
static void stop_stick_calibration();
static void calibration_update_callback(const StickCalibrationManager::CalibrationData& data);
static void showSavedCalibration();
static void set_disabled_recursive(lv_obj_t *obj, bool disabled);
static void setExclusiveChecked(lv_obj_t *parent,lv_obj_t *selected);
//---для работы с окном Выбор модели
static void refreshModelList();
static void modelCardClicked(lv_event_t *e);
static void showCreateModelDialog();
static void createModelDialogCreate(lv_event_t *e);
static void createModelDialogCancel(lv_event_t *e);
static void addModelCardClicked(lv_event_t *e);
static void rebuildOutputCurveList();
static void outputCurveRowClicked(lv_event_t *e);
static void addOutputCurveClicked(lv_event_t *e);
static void updateOutputCurveSelectionUi(uint8_t ch);
static void updateMixCurveSelectionUi(uint8_t slot);
static int8_t getActiveCurvePickerSelection();
static void refreshCurveList();
static void drawCurveGraph(uint8_t curveIndex);
static void loadCurveToEditor(uint8_t curveIndex);
static void updateCurveEasyMode();
static void updateCurvePointVisibility();
static void distributeCurvePoints(uint8_t pointCount);
static void updateCurvePointXLabel(uint8_t index);
static void setCurvePointX(uint8_t index,int16_t value);
static void saveCurveEditor();
static int8_t getOutputCurveSelection(const OutputConfig &output);
static uint16_t getCurveUsageCount(uint8_t curveIndex);
static void formatCurveUsageOwners(uint8_t curveIndex,char *buffer,size_t size);
static void removeCurveReferences(uint8_t curveIndex);
static void deleteSelectedCurve();
static void curveDeleteConfirmCb(lv_event_t *e);
static void curveDeleteCancelCb(lv_event_t *e);
static void updateChannelBalanceUi(uint8_t ch);
static void openChannelBalanceEditor(uint8_t ch);
static void clearChannelBalance(uint8_t ch);
static void drawChannelBalanceGraph(uint8_t ch);
static void drawChannelBalanceMiniGraph(uint8_t ch);
static void balancePointClicked(lv_event_t *e);
static void updateBalancePointChangerUi();
static void balancePointChangeStep(lv_event_t *e);
static void balancePointDoStep(lv_event_t *e);
static int16_t getBalanceEncoderStep(int8_t direction);
static void balanceStickPositionClicked(lv_event_t *e);
static void balanceMagnetClicked(lv_event_t *e);
static void balanceLockClicked(lv_event_t *e);
static void updateBalanceStickPosition();
static void loadBalanceSettingsUi();
static void updateBalanceSettingsSmoothColors();
static void balanceSettingsSmoothEvent(lv_event_t *e);
static void balanceSettingsApply(lv_event_t *e);
static void balanceSettingsCancel(lv_event_t *e);
static void resampleBalancePoints(BalanceConfig &balance,uint8_t newPointCount);
static lv_obj_t *getBalanceEncoderNavObject(uint8_t index);
static void applyBalanceEncoderSelection();
static void moveBalanceEncoderSelection(int8_t direction);
static int16_t getBalanceCurrentXValue();
static void selectNearestBalancePointToCurrentX();

//---для работы с окном Mixes
static void refreshMixList();
static void mixRowClicked(lv_event_t *e);
static void hideMixInfo();
static void showMixInfo(uint8_t mixIndex);
static void previewMixInfo(uint8_t mixIndex);
static void formatMixSourceName(const SourceId &source,char *buffer,size_t size);
static void formatMixConditionName(const ConditionConfig &condition,char *buffer,size_t size);
static void drawMixGraph(uint8_t mixIndex);
static void clearMixGraph();
static void updateInputRuntime(lv_timer_t *timer);
static int16_t getMixSourceLiveValue(const MixLine &mix);
static void updateMixGraphLive(lv_timer_t *timer);
static void hideMixActionChanger();
static void clearFreeMixActionList();
static void rebuildFreeMixActionList();
static void formatMixActionListDetails(const MixAction &action,const ModelConfig &model,char *buffer,size_t size);
static lv_obj_t *createFreeMixActionRow(lv_obj_t *parent,uint8_t actionIndex,bool alternate);
static void mixActionRowClicked(lv_event_t *e);
static bool saveMixActionEditor();
static bool buildMixActionFromEditor(MixAction &action);
static int32_t applyOneMixActionForGraph(int32_t value,const MixAction &action,const ModelConfig &model,int32_t sourceValue);
static int32_t applyMixGraphPreview(int32_t value,const MixLine &mix,const ModelConfig &model,const MixAction &previewAction);
static void setMixGraphActionPreview(bool enabled);
static void loadMixActionEditor(uint8_t mixIndex,uint8_t actionIndex);
static int8_t findFreeMixIndex();
static void setupMixOutputUiCallbacks();
static void updateMixOutputsUi();
static void updateMixOutputMonitors();
static void applyMixOutputCountFromEditor();
static void mixOutputCountPanelClicked(lv_event_t *e);
static void mixOutputNamePanelClicked(lv_event_t *e);
static void mixOutputReverseChanged(lv_event_t *e);
static void mixChannelListRowClicked(lv_event_t *e);
static void mixChannelListOkClicked(lv_event_t *e);
static void showMixChannelList(uint8_t outputSlot);

//--- Vars UI
static void setupVarsUiCallbacks();
static void refreshVarsList();
static bool isVarMoveMode();
static void exitVarMoveMode();
static void openVarEditor(uint8_t varIndex);
static void hideVarPopup();
static void varDeleteMenuClicked(lv_event_t *e);
static void saveVarEditorText();
static void updateVarEditorUi();
static void hideVarChanger();
static void varsNumericChangeStep(lv_event_t *e);
static void varsNumericDoStep(lv_event_t *e);

static lv_obj_t *createModelDialog=nullptr;
static lv_obj_t *createModelName=nullptr;
static ColorPicker *colorPicker = nullptr;
static lv_obj_t *dynamicCurveRows[MAX_MODEL_CURVES]={};
static uint8_t dynamicCurveRowCount=0;
static lv_obj_t *dynamicAddCurveRow=nullptr;
bool isChannelsScreenActive = false;
static lv_obj_t *curveGraphLine=nullptr;
static lv_point_precise_t curveGraphPoints[300];
static lv_obj_t *mixGraphLine=nullptr;
static lv_obj_t *mixGraphCursorLine=nullptr;
static lv_obj_t *mixGraphCurrentPoint=nullptr;
static lv_timer_t *mixGraphTimer=nullptr;
static lv_timer_t *inputRuntimeTimer=nullptr;
static constexpr uint16_t MIX_GRAPH_DRAW_POINTS=201;
// Alternate point storage so LVGL always receives a new array when the
// Action chain changes (in particular when the last Action is deleted).
static lv_point_precise_t mixGraphPoints[2][MIX_GRAPH_DRAW_POINTS];
static uint8_t mixGraphPointBuffer=0;
static lv_point_precise_t mixGraphCursorPoints[2];
static lv_obj_t *curveGraphPointMarkers[MAX_CURVE_POINTS]={};
static lv_obj_t *balanceGraphLine=nullptr;
static lv_obj_t *balanceMiniGraphLine=nullptr;
static constexpr uint16_t BALANCE_GRAPH_DRAW_POINTS=157;
static constexpr uint16_t BALANCE_MINI_DRAW_POINTS=71;
static lv_point_precise_t balanceGraphPoints[BALANCE_GRAPH_DRAW_POINTS];
static lv_point_precise_t balanceMiniGraphPoints[BALANCE_MINI_DRAW_POINTS];
static lv_obj_t *balanceGraphPointMarkers[MAX_BALANCE_POINTS]={};
static lv_obj_t *balanceGraphPointHitAreas[MAX_BALANCE_POINTS]={};
static lv_obj_t *balanceMiniGraphPointMarkers[MAX_BALANCE_POINTS]={};
static lv_obj_t *balanceZeroLineErase=nullptr;
static lv_obj_t *balanceZeroLine=nullptr;
static bool balancePointEditActive=false;
static uint8_t activeBalancePoint=0;
static float balancePointEditValue=0.0f;
static uint32_t balanceEncoderLastStepMs=0;
static int8_t balanceEncoderLastDirection=0;

// Full balance graph redraw is relatively heavy. During fast encoder rotation
// many detents can arrive before LVGL has time to render every intermediate frame.
// Collapse them into one redraw per display frame instead of drawing on every detent.
static bool balanceGraphRedrawPending=false;
static uint32_t balanceGraphLastRedrawMs=0;
static constexpr uint32_t BALANCE_GRAPH_REDRAW_INTERVAL_MS=16;
static bool balanceAutoAnalogEnabled=false;
static bool balanceAutoAnalogCaptured=false;
static AnalogInput balanceAutoAnalogInput=AnalogInput::STICK_LX;
static int16_t balanceAutoAnalogPrevious[static_cast<uint8_t>(AnalogInput::COUNT)]={};
static constexpr int16_t BALANCE_AUTO_ANALOG_THRESHOLD=35;
static bool balanceMagnetEnabled=false;
static bool balanceMagnetUiBound=false;
static bool balanceStickPositionUiBound=false;
static bool balanceLockUiBound=false;
static int16_t balanceLockedXValue=0;
static uint8_t balanceSettingsPointCount=5;
static bool balanceSettingsSmooth=false;
static bool balanceSettingsUiBound=false;
static uint8_t balanceEncoderNavIndex=0;
static bool balanceCanvasEditMode=false;
static constexpr uint8_t BALANCE_ENCODER_NAV_COUNT=6;
static lv_obj_t *balanceStickCursorLine=nullptr;
static lv_obj_t *balanceStickMarker=nullptr;
static lv_obj_t *balanceStickInfoLabel=nullptr;
static int8_t selectedCurveIndex=-1;
static int8_t editingCurveIndex=-1;
static bool curveEditorIsNew=false;
static bool curveEditorReturnToChannel=false;
static bool curveEditorReturnToMixAction=false;
static bool curveEditorReturnToAileron=false;
static bool curvePickerForMixAction=false;
static uint8_t activeMixCurveSlot=0;
static int8_t mixActionCurveSelection[6]={
    OUTPUT_CURVE_NONE,OUTPUT_CURVE_NONE,OUTPUT_CURVE_NONE,
    OUTPUT_CURVE_NONE,OUTPUT_CURVE_NONE,OUTPUT_CURVE_NONE
};
enum class SharedEditDeletePopupTarget:uint8_t{
    CURVE=0,
    MIX,
    ACTION,
    VAR
};
static SharedEditDeletePopupTarget sharedEditDeletePopupTarget=SharedEditDeletePopupTarget::CURVE;

static void prepareSharedEditDeletePopupForCurve();
static void prepareSharedEditDeletePopupForMix(uint8_t mixIndex);
static void prepareSharedEditDeletePopupForAction(uint8_t actionIndex);
static const char *getMixActionTypeName(MixActionType type);
static void openExistingMixEditor(uint8_t mixIndex);
static void deleteSelectedMix();
static void deleteSelectedMixAction();
static void mixDeleteConfirmCb(lv_event_t *e);
static void mixDeleteCancelCb(lv_event_t *e);
static void mixActionDeleteConfirmCb(lv_event_t *e);
static void mixActionDeleteCancelCb(lv_event_t *e);

static int8_t selectedMixIndex=-1;
static int8_t selectedMixActionIndex=-1;
static int8_t focusedMixActionIndex=-1;
static int8_t selectedVarIndex=-1;

static bool mixActionMoveMode=false;
static bool mixActionMovePopupBound=false;
static bool mixActionClonePopupBound=false;
static bool mixActionAddPopupBound=false;
static bool mixActionDeletePopupBound=false;

static void mixActionCloneMenuClicked(lv_event_t *e);
static void mixActionAddMenuClicked(lv_event_t *e);
static void mixActionMoveMenuClicked(lv_event_t *e);
static void mixActionMoveArrowClicked(lv_event_t *e);
static void mixActionMoveOutsideClicked(lv_event_t *e);
static void moveSelectedMixAction(int8_t direction);
static void exitMixActionMoveMode();

// Encoder navigation for the Mixes screen.
// This is a focus cursor; selectedMixIndex remains the actually selected Mix.
static constexpr int8_t MIX_ENCODER_FOCUS_NONE=-1;
static constexpr int8_t MIX_ENCODER_FOCUS_ADD=-2;

static int8_t mixEncoderFocusIndex=MIX_ENCODER_FOCUS_NONE;
static uint8_t mixPopupEncoderIndex=0; // 0 = Edit, 1 = Delete

enum class MixEditorEncoderItemType:uint8_t{
    NAME=0,
    ACTIVE_CONDITION,
    SOURCE,
    OPERATION,
    ACTION,
    ADD_ACTION,
    OUTPUT_COUNT,
    OUTPUT_CHANNEL,
    OUTPUT_REVERSE
};

struct MixEditorEncoderItem{
    lv_obj_t *obj=nullptr;
    MixEditorEncoderItemType type=MixEditorEncoderItemType::NAME;
    uint8_t data=0;
};

static int16_t mixEditorEncoderIndex=0;
static constexpr uint8_t MAX_MIX_EDITOR_ENCODER_ITEMS=
    4 + MAX_MIX_ACTIONS + 1 + 1 + (MAX_MIX_OUTPUTS * 2);

static uint8_t buildMixEditorEncoderItems(
    MixEditorEncoderItem *items,
    uint8_t maxItems
);
static bool isMixEditorMainVisible();
static void applyMixEditorEncoderFocus();
static void mixEditorEncoderMove(int8_t direction);
static void mixEditorEncoderPressed();
static void resetMixEditorEncoderNavigation();
static void syncMixEditorEncoderIndexFromCurrentFocus(MixEditorEncoderItem *items,uint8_t count);
static void mixEditorNavigationTouchEvent(lv_event_t *e);
static void bindMixEditorNavigationTouchTracking(MixEditorEncoderItem *items,uint8_t count);
static void updateMixOperationEditorUi();
static void updateMixOperationPopupFocus();
static void hideMixOperationPopup();
static void loadMixNameToEditor();
static void saveMixNameFromEditor();

static int8_t findFirstEnabledMixIndex();
static int8_t findLastEnabledMixIndex();
static int8_t findNextEnabledMixIndex(int8_t current,int8_t direction);
static lv_obj_t *getMixListRowForMixIndex(uint8_t mixIndex);
static void applyMixEncoderFocus();
static void applyMixPopupEncoderFocus();
static bool isMixPopupVisible();
static bool isMixListVisible();
static void mixEncoderLeft();
static void mixEncoderRight();
static void mixEncoderPressed();

enum class MixEditorKind:uint8_t{FREE_MIX=0,AILERON=1};
static MixEditorKind activeMixEditorKind=MixEditorKind::FREE_MIX;
static bool isAileronMixEditor(){return activeMixEditorKind==MixEditorKind::AILERON;}

struct AileronEditorState{
    uint32_t signature=0x41494C31;
    int16_t weights[10]={100,100,100,100,100,100,100,100,100,100};
    int16_t curveExpo[6]={};
    int8_t curveSelection[6]={OUTPUT_CURVE_NONE,OUTPUT_CURVE_NONE,OUTPUT_CURVE_NONE,OUTPUT_CURVE_NONE,OUTPUT_CURVE_NONE,OUTPUT_CURVE_NONE};
    ConditionConfig weightConditions[10]={};
    ConditionConfig curveConditions[6]={};
    uint16_t weightEnabled=0;
    uint8_t curveEnabled=0;
    int16_t differential=0;
    bool trim=true;
};
static AileronEditorState aileronEditorState{};
static bool aileronEditorLoading=false;
static bool aileronCallbacksBound=false;
static void updateAileronEditorUi();
static void saveAileronEditorState();
static void loadAileronEditorState(uint8_t mixIndex);
static void bindAileronEditorCallbacks();
static void aileronTrimChanged(lv_event_t *e);

static int8_t editingMixIndex=-1;
static void aileronPreferencesKey(char *buffer,size_t size,uint8_t mixIndex){
    const ModelConfig &model=modelManager.getActiveModel();
    snprintf(buffer,size,"m%04X_%02X",(unsigned)model.modelId,(unsigned)mixIndex);
}

// Bit 15 is outside the 9 supported flight modes. It identifies an Aileron
// while retaining the existing ModelConfig binary layout and storage version.
static constexpr uint16_t MIX_AILERON_TYPE_FLAG=0x8000u;

static void syncAileronEngineState(){
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];
    if(!mix.enabled||(mix.flightModeMask&MIX_AILERON_TYPE_FLAG)==0) return;

    // The ordinary ordered action engine already implements curve and rate
    // selection, so the Aileron UI can serialize those settings as two actions.
    mix.source=Sources::stick(StickFunction::AILERON);
    mix.actionCount=3;
    for(uint8_t i=0;i<3;i++) mix.actions[i]=MixAction{};

    MixAction &curve=mix.actions[0];
    curve.enabled=true;
    curve.type=MixActionType::CURVE;
    curve.condition.type=ConditionType::ALWAYS;
    curve.variantCount=6;
    for(uint8_t i=0;i<6;i++){
        MixActionVariant &v=curve.variants[i];
        v.enabled=i==0||(aileronEditorState.curveEnabled&(1u<<i))!=0;
        v.condition=aileronEditorState.curveConditions[i];
        v.curveIndex=aileronEditorState.curveSelection[i];
        v.expo=(int8_t)constrain((int32_t)aileronEditorState.curveExpo[i],-100L,100L);
    }

    MixAction &weight=mix.actions[1];
    weight.enabled=true;
    weight.type=MixActionType::WEIGHT;
    weight.condition.type=ConditionType::ALWAYS;
    weight.variantCount=10;
    for(uint8_t i=0;i<10;i++){
        MixActionVariant &v=weight.variants[i];
        v.enabled=i==0||(aileronEditorState.weightEnabled&(1u<<i))!=0;
        v.condition=aileronEditorState.weightConditions[i];
        v.value=(int16_t)constrain((int32_t)aileronEditorState.weights[i]*10L,-5000L,5000L);
    }

    MixAction &trim=mix.actions[2];
    trim.enabled=aileronEditorState.trim;
    trim.type=MixActionType::TRIM;
    trim.condition.type=ConditionType::ALWAYS;
    trim.trimIndex=2; // Aileron trim in the default model configuration.
    mix.differential=(int16_t)constrain((int32_t)aileronEditorState.differential*10L,-1000L,1000L);
    modelManager.requestSaveActiveModel();
}

static void saveAileronEditorState(){
    if(aileronEditorLoading||editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
    syncAileronEngineState();
    char key[16];
    aileronPreferencesKey(key,sizeof(key),(uint8_t)editingMixIndex);
    Preferences prefs;
    if(!prefs.begin("aileronMix",false)) return;
    prefs.putBytes(key,&aileronEditorState,sizeof(aileronEditorState));
    prefs.end();
}

static void loadAileronEditorState(uint8_t mixIndex){
    aileronEditorLoading=true;
    aileronEditorState=AileronEditorState{};
    for(uint8_t i=0;i<10;i++) aileronEditorState.weightConditions[i].type=ConditionType::ALWAYS;
    for(uint8_t i=0;i<6;i++) aileronEditorState.curveConditions[i].type=ConditionType::ALWAYS;
    char key[16];
    aileronPreferencesKey(key,sizeof(key),mixIndex);
    Preferences prefs;
    if(prefs.begin("aileronMix",true)){
        AileronEditorState saved{};
        if(prefs.getBytesLength(key)==sizeof(saved)&&prefs.getBytes(key,&saved,sizeof(saved))==sizeof(saved)&&saved.signature==0x41494C31)
            aileronEditorState=saved;
        prefs.end();
    }

    // Model actions are authoritative once an Aileron has been synchronized.
    // This also preserves settings when the user reorders or clones mix lines.
    const MixLine &mix=modelManager.getActiveModel().mixes[mixIndex];
    if((mix.flightModeMask&MIX_AILERON_TYPE_FLAG)!=0&&mix.actionCount>=3&&
       mix.actions[0].type==MixActionType::CURVE&&mix.actions[1].type==MixActionType::WEIGHT){
        aileronEditorState.weightEnabled=0;
        aileronEditorState.curveEnabled=0;
        const MixAction &curve=mix.actions[0];
        const MixAction &weight=mix.actions[1];
        for(uint8_t i=0;i<6;i++){
            aileronEditorState.curveSelection[i]=curve.variants[i].curveIndex;
            aileronEditorState.curveExpo[i]=curve.variants[i].expo;
            aileronEditorState.curveConditions[i]=curve.variants[i].condition;
            if(i>0&&curve.variants[i].enabled) aileronEditorState.curveEnabled|=(uint8_t)(1u<<i);
        }
        for(uint8_t i=0;i<10;i++){
            aileronEditorState.weights[i]=(int16_t)(weight.variants[i].value/10);
            aileronEditorState.weightConditions[i]=weight.variants[i].condition;
            if(i>0&&weight.variants[i].enabled) aileronEditorState.weightEnabled|=(uint16_t)(1u<<i);
        }
        aileronEditorState.trim=mix.actions[2].enabled;
        aileronEditorState.differential=(int16_t)(mix.differential/10);
    }
    aileronEditorLoading=false;
}

static int8_t editingMixActionIndex=-1;
static bool mixActionEditorIsNew=false;
static bool mixGraphActionPreview=false;
static MixActionType activeMixActionType=MixActionType::WEIGHT;
static int8_t activeMixTrimIndex=0;
static constexpr CurveType CURVE_TYPE_NONE=static_cast<CurveType>(255);
static uint8_t currentCurvePointCount=5;
static int16_t curveEditX[MAX_CURVE_POINTS]={};
static lv_obj_t *getCurvePointContainer(uint8_t index){
    lv_obj_t *points[MAX_CURVE_POINTS]={
        objects.curv_point_editor__point1,
        objects.curv_point_editor__point2,
        objects.curv_point_editor__point3,
        objects.curv_point_editor__point4,
        objects.curv_point_editor__point5,
        objects.curv_point_editor__point6,
        objects.curv_point_editor__point7,
        objects.curv_point_editor__point8,
        objects.curv_point_editor__point9,
        objects.curv_point_editor__point10,
        objects.curv_point_editor__point11,
        objects.curv_point_editor__point12,
        objects.curv_point_editor__point13,
        objects.curv_point_editor__point14,
        objects.curv_point_editor__point15,
        objects.curv_point_editor__point16,
        objects.curv_point_editor__point17,
        objects.curv_point_editor__point18,
        objects.curv_point_editor__point19,
        objects.curv_point_editor__point20,
        objects.curv_point_editor__point21
    };
    return index<MAX_CURVE_POINTS?points[index]:nullptr;
}
static lv_obj_t *getCurvePointX(uint8_t index){
    lv_obj_t *xLabels[MAX_CURVE_POINTS]={
        objects.curv_point_editor__curv_x_pos1,
        objects.curv_point_editor__curv_x_pos2,
        objects.curv_point_editor__curv_x_pos3,
        objects.curv_point_editor__curv_x_pos4,
        objects.curv_point_editor__curv_x_pos5,
        objects.curv_point_editor__curv_x_pos6,
        objects.curv_point_editor__curv_x_pos7,
        objects.curv_point_editor__curv_x_pos8,
        objects.curv_point_editor__curv_x_pos9,
        objects.curv_point_editor__curv_x_pos10,
        objects.curv_point_editor__curv_x_pos11,
        objects.curv_point_editor__curv_x_pos12,
        objects.curv_point_editor__curv_x_pos13,
        objects.curv_point_editor__curv_x_pos14,
        objects.curv_point_editor__curv_x_pos15,
        objects.curv_point_editor__curv_x_pos16,
        objects.curv_point_editor__curv_x_pos17,
        objects.curv_point_editor__curv_x_pos18,
        objects.curv_point_editor__curv_x_pos19,
        objects.curv_point_editor__curv_x_pos20,
        objects.curv_point_editor__curv_x_pos21
    };
    return index<MAX_CURVE_POINTS?xLabels[index]:nullptr;
}
static lv_obj_t *getCurvePointY(uint8_t index){
    lv_obj_t *yLabels[MAX_CURVE_POINTS]={
        objects.curv_point_editor__curv_x_pos1_percent,
        objects.curv_point_editor__curv_x_pos2_percent,
        objects.curv_point_editor__curv_x_pos3_percent,
        objects.curv_point_editor__curv_x_pos4_percent,
        objects.curv_point_editor__curv_x_pos5_percent,
        objects.curv_point_editor__curv_x_pos6_percent,
        objects.curv_point_editor__curv_x_pos7_percent,
        objects.curv_point_editor__curv_x_pos8_percent,
        objects.curv_point_editor__curv_x_pos9_percent,
        objects.curv_point_editor__curv_x_pos10_percent,
        objects.curv_point_editor__curv_x_pos11_percent,
        objects.curv_point_editor__curv_x_pos12_percent,
        objects.curv_point_editor__curv_x_pos13_percent,
        objects.curv_point_editor__curv_x_pos14_percent,
        objects.curv_point_editor__curv_x_pos15_percent,
        objects.curv_point_editor__curv_x_pos16_percent,
        objects.curv_point_editor__curv_x_pos17_percent,
        objects.curv_point_editor__curv_x_pos18_percent,
        objects.curv_point_editor__curv_x_pos19_percent,
        objects.curv_point_editor__curv_x_pos20_percent,
        objects.curv_point_editor__curv_x_pos21_percent
    };
    return index<MAX_CURVE_POINTS?yLabels[index]:nullptr;
}

static void updateCurvePointXLabel(uint8_t index){
    if(index>=MAX_CURVE_POINTS) return;
    lv_obj_t *label=getCurvePointX(index);
    if(!label) return;
    lv_label_set_text_fmt(label,"%d%%",curveEditX[index]/10);
}
static void distributeCurvePoints(uint8_t pointCount){
    if(pointCount<2||pointCount>MAX_CURVE_POINTS) return;
    currentCurvePointCount=pointCount;
    for(uint8_t i=0;i<pointCount;i++){
        int32_t percent=lroundf(-100.0f+(200.0f*i)/(pointCount-1));
        curveEditX[i]=(int16_t)(percent*10);
        updateCurvePointXLabel(i);
    }
    curveEditX[0]=-1000;
    curveEditX[pointCount-1]=1000;
    updateCurvePointXLabel(0);
    updateCurvePointXLabel(pointCount-1);
}
static void setCurvePointX(uint8_t index,int16_t value){
    if(index>=currentCurvePointCount) return;
    if(index==0){
        curveEditX[index]=-1000;
        updateCurvePointXLabel(index);
        return;
    }
    if(index==currentCurvePointCount-1){
        curveEditX[index]=1000;
        updateCurvePointXLabel(index);
        return;
    }
    value=(int16_t)(lroundf(value/10.0f)*10);
    int16_t minValue=curveEditX[index-1]+10;
    int16_t maxValue=curveEditX[index+1]-10;
    if(minValue>maxValue) return;
    curveEditX[index]=(int16_t)constrain((int32_t)value,(int32_t)minValue,(int32_t)maxValue);
    updateCurvePointXLabel(index);
}

// 1. Объявление массивов на 8 каналов
lv_obj_t* ch_num_labels[8];
lv_obj_t* ch_name_labels[8];
lv_obj_t* ch_rev_icons[8];
lv_obj_t* ch_expo_icons[8];
lv_obj_t* ch_balance_icons[8];
lv_obj_t* ch_slow_icons[8];
lv_obj_t* ch_out_bars[8];
lv_obj_t* ch_mix_bars[8];
lv_obj_t* ch_out_val_labels[8];
lv_obj_t* ch_out_pct_labels[8];
lv_obj_t* ch_mix_pct_labels[8];
lv_obj_t* ch_l_pans[8];
lv_obj_t* ch_r_pans[8];
lv_obj_t* ch_out_lines[8];
lv_obj_t* ch_mix_lines[8];

// 2. Наборы данных для каждого типа поля
static const StepConfig STEPS_EXPO={"1%\n10%",1,{1.0f,10.0f,0.0f}};
static const StepConfig STEPS_LIMIT={"0.1%\n1.0%\n10.0%",2,{0.1f,1.0f,10.0f}};
static const StepConfig STEPS_PWM={"1us\n10us\n100us",2,{1.0f,10.0f,100.0f}};
static const StepConfig STEPS_SLOW={"0.1c\n1.0c",1,{0.1f,1.0f,0.0f}};
static bool active_field_is_pwm=false;
static float ch_pwm_center=1500.0f;
// 3. Прямое определение конфига по указателю на активное поле
const StepConfig* get_current_step_config() {
    if(active_field_is_pwm) return &STEPS_PWM;
    if(active_field==FIELD_EXPO_CURVE) return &STEPS_EXPO;
    if(active_field==FIELD_SLOW_UP||active_field==FIELD_SLOW_DOWN) return &STEPS_SLOW;
    return &STEPS_LIMIT;
}
static uint32_t last_selected_indices[4]={0,0,0,0};
StepGroup get_current_step_group() {
    if(active_field_is_pwm) return STEP_GROUP_PWM;
    if(active_field==FIELD_EXPO_CURVE) return STEP_GROUP_EXPO;
    if(active_field==FIELD_SLOW_UP||active_field==FIELD_SLOW_DOWN) return STEP_GROUP_SLOW;
    return STEP_GROUP_LIMIT;
}
static float getChannelMixPercent(uint8_t ch){
    ModelConfig &model = modelManager.getActiveModel();
    if (ch >= model.channelCount) return 0.0f;
    return mixer.getChannel(ch) / 10.0f;
}
void update_dropdown_step_options() {
    if (objects.ch_edit_changer_panel__dropdown_step == NULL) return;
    const StepConfig* cfg = get_current_step_config();
    if (cfg == NULL) return;
    StepGroup group = get_current_step_group();
    const char* current_options = lv_dropdown_get_options(objects.ch_edit_changer_panel__dropdown_step);
    // Если строки вариантов различаются (перешли на другой тип поля)
    if (current_options == NULL || strcmp(current_options, cfg->options) != 0) {
        // 1. Устанавливаем новые опции в LVGL
        lv_dropdown_set_options(objects.ch_edit_changer_panel__dropdown_step, cfg->options); 
        // 2. Восстанавливаем ранее сохраненный индекс для ЭТОЙ группы
        uint32_t saved_idx = last_selected_indices[group];
        if (saved_idx > cfg->max_idx) saved_idx = cfg->max_idx;  
        lv_dropdown_set_selected(objects.ch_edit_changer_panel__dropdown_step, saved_idx);
    } else {
        // Если группа не менялась, просто обновляем сохраненное значение от текущего выбора
        last_selected_indices[group] = lv_dropdown_get_selected(objects.ch_edit_changer_panel__dropdown_step);
    }
    // Текущий актуальный индекс
    uint32_t current_selected = lv_dropdown_get_selected(objects.ch_edit_changer_panel__dropdown_step);
    // 3. Блокировка / разблокировка кнопок reduce_step и increase_step
    if (objects.ch_edit_changer_panel__reduce_step != NULL) {
        if (current_selected == 0) lv_obj_add_state(objects.ch_edit_changer_panel__reduce_step, LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.ch_edit_changer_panel__reduce_step, LV_STATE_DISABLED);
    }

    if (objects.ch_edit_changer_panel__increase_step != NULL) {
        if (current_selected >= cfg->max_idx) lv_obj_add_state(objects.ch_edit_changer_panel__increase_step, LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.ch_edit_changer_panel__increase_step, LV_STATE_DISABLED);
    }
}
static CurveEditField activeCurveEditField=CURVE_EDIT_NONE;
static uint8_t activeCurveEditPoint=0;
static uint32_t curveStepSelectedInteger=0;
static uint32_t curveStepSelectedFine=0;
static bool isChildOf(lv_obj_t *obj,lv_obj_t *parent){
    if(!obj||!parent) return false;
    lv_obj_t *p=obj;
    while(p){
        if(p==parent) return true;
        p=lv_obj_get_parent(p);
    }
    return false;
}
static bool eventFromChanger(lv_event_t *e,lv_obj_t *changer){
    if(!e||!changer) return false;
    lv_obj_t *target=(lv_obj_t *)lv_event_get_current_target(e);
    if(isChildOf(target,changer)) return true;
    target=(lv_obj_t *)lv_event_get_target(e);
    return isChildOf(target,changer);
}

static lv_obj_t *mixChangerOpenTarget=nullptr;
static lv_obj_t *varChangerOpenTarget=nullptr;

static void dismissMixChangerOnPress(lv_event_t *e){
    if(!e||!objects.mix_changer_panel) return;
    if(lv_obj_has_flag(objects.mix_changer_panel,LV_OBJ_FLAG_HIDDEN)) return;
    lv_obj_t *target=(lv_obj_t *)lv_event_get_target(e);
    if(target&&isChildOf(target,objects.mix_changer_panel)) return;
    if(target&&mixChangerOpenTarget&&isChildOf(target,mixChangerOpenTarget)) return;
    hideMixActionChanger();
}

static void dismissVarChangerOnPress(lv_event_t *e){
    if(!e||!objects.vars_changer_panel) return;
    if(lv_obj_has_flag(objects.vars_changer_panel,LV_OBJ_FLAG_HIDDEN)) return;
    lv_obj_t *target=(lv_obj_t *)lv_event_get_target(e);
    if(target&&isChildOf(target,objects.vars_changer_panel)) return;
    if(target&&varChangerOpenTarget&&isChildOf(target,varChangerOpenTarget)) return;
    hideVarChanger();
}

static void bindChangerDismissRecursive(lv_obj_t *root,lv_obj_t *changer,lv_event_cb_t cb){
    if(!root||!cb) return;
    if(root==changer||isChildOf(root,changer)) return;

    lv_obj_remove_event_cb(root,cb);
    lv_obj_add_event_cb(root,cb,LV_EVENT_CLICKED,nullptr);

    uint32_t childCount=lv_obj_get_child_count(root);
    for(uint32_t i=0;i<childCount;i++){
        lv_obj_t *child=lv_obj_get_child(root,(int32_t)i);
        bindChangerDismissRecursive(child,changer,cb);
    }
}
static const StepConfig *getCurveStepConfig(){
    switch(activeCurveEditField){
        case CURVE_EDIT_EXPO_WEIGHT:
        case CURVE_EDIT_EXPO_EXPO:
        case CURVE_EDIT_POINT_X:return &STEPS_EXPO;
        case CURVE_EDIT_EXPO_OFFSET:
        case CURVE_EDIT_FUNCTION_OFFSET:
        case CURVE_EDIT_CUSTOM_OFFSET:
        case CURVE_EDIT_POINT_Y:return &STEPS_LIMIT;
        default:return nullptr;
    }
}
static bool curveFieldUsesDecimal(){
    switch(activeCurveEditField){
        case CURVE_EDIT_EXPO_OFFSET:
        case CURVE_EDIT_FUNCTION_OFFSET:
        case CURVE_EDIT_CUSTOM_OFFSET:
        case CURVE_EDIT_POINT_Y:return true;
        default:return false;
    }
}
static CurveConfig *getEditingCurve(){
    if(selectedCurveIndex<0||selectedCurveIndex>=MAX_MODEL_CURVES) return nullptr;
    ModelConfig &model=modelManager.getActiveModel();
    CurveConfig &curve=model.curves[selectedCurveIndex];
    if(!curve.enabled) return nullptr;
    return &curve;
}
static lv_obj_t *getActiveCurveFieldLabel(){
    switch(activeCurveEditField){
        case CURVE_EDIT_EXPO_WEIGHT:return objects.curve_expo_weight;
        case CURVE_EDIT_EXPO_OFFSET:return objects.curve_expo_offset;
        case CURVE_EDIT_EXPO_EXPO:return objects.curve_expo_expo;
        case CURVE_EDIT_FUNCTION_OFFSET:return objects.curve_function_offset;
        case CURVE_EDIT_CUSTOM_OFFSET:return objects.custom_curv_offset_lbl;
        case CURVE_EDIT_POINT_X:return getCurvePointX(activeCurveEditPoint);
        case CURVE_EDIT_POINT_Y:return getCurvePointY(activeCurveEditPoint);
        default:return nullptr;
    }
}
static float getActiveCurveFieldValue(){
    CurveConfig *curve=getEditingCurve();
    if(!curve) return 0.0f;
    switch(activeCurveEditField){
        case CURVE_EDIT_EXPO_WEIGHT:return curve->weight/10.0f;
        case CURVE_EDIT_EXPO_OFFSET:return curve->offset/10.0f;
        case CURVE_EDIT_EXPO_EXPO:return (float)curve->expo;
        case CURVE_EDIT_FUNCTION_OFFSET:return curve->offset/10.0f;
        case CURVE_EDIT_CUSTOM_OFFSET:return curve->offset/10.0f;
        case CURVE_EDIT_POINT_X:
            if(activeCurveEditPoint<currentCurvePointCount) return curveEditX[activeCurveEditPoint]/10.0f;
            break;
        case CURVE_EDIT_POINT_Y:
            if(activeCurveEditPoint<currentCurvePointCount) return curve->y[activeCurveEditPoint]/10.0f;
            break;
        default:break;
    }
    return 0.0f;
}
static void getActiveCurveFieldBounds(float &minValue,float &maxValue){
    minValue=-100.0f;
    maxValue=100.0f;
    if(activeCurveEditField==CURVE_EDIT_POINT_X){
        if(activeCurveEditPoint==0||activeCurveEditPoint>=currentCurvePointCount-1){
            float value=getActiveCurveFieldValue();
            minValue=value;
            maxValue=value;
            return;
        }
        minValue=curveEditX[activeCurveEditPoint-1]/10.0f+1.0f;
        maxValue=curveEditX[activeCurveEditPoint+1]/10.0f-1.0f;
    }
}
static void formatActiveCurveField(float value){
    lv_obj_t *label=getActiveCurveFieldLabel();
    if(!label) return;
    char buf[16];
    if(curveFieldUsesDecimal()) snprintf(buf,sizeof(buf),"%.1f%%",value);
    else snprintf(buf,sizeof(buf),"%d%%",(int)lroundf(value));
    lv_label_set_text(label,buf);
}
static void setActiveCurveFieldValue(float value){
    CurveConfig *curve=getEditingCurve();
    if(!curve) return;
    float minValue,maxValue;
    getActiveCurveFieldBounds(minValue,maxValue);
    if(value<minValue) value=minValue;
    if(value>maxValue) value=maxValue;
    if(!curveFieldUsesDecimal()) value=roundf(value);
    switch(activeCurveEditField){
        case CURVE_EDIT_EXPO_WEIGHT:curve->weight=(int16_t)lroundf(value*10.0f);break;
        case CURVE_EDIT_EXPO_OFFSET:
        case CURVE_EDIT_FUNCTION_OFFSET:
        case CURVE_EDIT_CUSTOM_OFFSET:curve->offset=(int16_t)lroundf(value*10.0f);break;
        case CURVE_EDIT_EXPO_EXPO:curve->expo=(int8_t)constrain((int32_t)lroundf(value),-100L,100L);break;
        case CURVE_EDIT_POINT_X:
            if(activeCurveEditPoint>=currentCurvePointCount) return;
            setCurvePointX(activeCurveEditPoint,(int16_t)lroundf(value*10.0f));
            curve->x[activeCurveEditPoint]=curveEditX[activeCurveEditPoint];
            value=curveEditX[activeCurveEditPoint]/10.0f;
            break;
        case CURVE_EDIT_POINT_Y:
            if(activeCurveEditPoint>=currentCurvePointCount) return;
            curve->y[activeCurveEditPoint]=(int16_t)constrain((int32_t)lroundf(value*10.0f),-1000L,1000L);
            value=curve->y[activeCurveEditPoint]/10.0f;
            break;
        default:return;
    }
    formatActiveCurveField(value);
    drawCurveGraph((uint8_t)selectedCurveIndex);
}
static void updateCurveChangerStepButtons(){
    const StepConfig *cfg=getCurveStepConfig();
    if(!cfg||!objects.curve_changer_panel__dropdown_step) return;
    uint32_t selected=lv_dropdown_get_selected(objects.curve_changer_panel__dropdown_step);
    if(selected>cfg->max_idx) selected=cfg->max_idx;
    if(objects.curve_changer_panel__reduce_step){
        if(selected==0) lv_obj_add_state(objects.curve_changer_panel__reduce_step,LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.curve_changer_panel__reduce_step,LV_STATE_DISABLED);
    }
    if(objects.curve_changer_panel__increase_step){
        if(selected>=cfg->max_idx) lv_obj_add_state(objects.curve_changer_panel__increase_step,LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.curve_changer_panel__increase_step,LV_STATE_DISABLED);
    }
}
static void updateCurveChangerStepOptions(){
    const StepConfig *cfg=getCurveStepConfig();
    if(!cfg||!objects.curve_changer_panel__dropdown_step) return;
    bool integerGroup=(cfg==&STEPS_EXPO);
    uint32_t saved=integerGroup?curveStepSelectedInteger:curveStepSelectedFine;
    const char *current=lv_dropdown_get_options(objects.curve_changer_panel__dropdown_step);
    if(!current||strcmp(current,cfg->options)!=0) lv_dropdown_set_options(objects.curve_changer_panel__dropdown_step,cfg->options);
    if(saved>cfg->max_idx) saved=cfg->max_idx;
    lv_dropdown_set_selected(objects.curve_changer_panel__dropdown_step,saved);
    updateCurveChangerStepButtons();
}
static void updateCurveChangerUi(){
    CurveConfig *curve=getEditingCurve();
    if(!curve||activeCurveEditField==CURVE_EDIT_NONE||!objects.curve_changer_panel__mcmsusd_slider) return;
    updateCurveChangerStepOptions();
    float minValue,maxValue;
    getActiveCurveFieldBounds(minValue,maxValue);
    float value=getActiveCurveFieldValue();
    if(value<minValue) value=minValue;
    if(value>maxValue) value=maxValue;
    if(curveFieldUsesDecimal()){
        lv_slider_set_range(objects.curve_changer_panel__mcmsusd_slider,(int32_t)lroundf(minValue*10.0f),(int32_t)lroundf(maxValue*10.0f));
        lv_slider_set_value(objects.curve_changer_panel__mcmsusd_slider,(int32_t)lroundf(value*10.0f),LV_ANIM_OFF);
    }else{
        lv_slider_set_range(objects.curve_changer_panel__mcmsusd_slider,(int32_t)lroundf(minValue),(int32_t)lroundf(maxValue));
        lv_slider_set_value(objects.curve_changer_panel__mcmsusd_slider,(int32_t)lroundf(value),LV_ANIM_OFF);
    }
    formatActiveCurveField(value);
}
static void curveChangeStep(lv_event_t *e){
    const StepConfig *cfg=getCurveStepConfig();
    if(!e||!cfg||!objects.curve_changer_panel__dropdown_step) return;
    uintptr_t direction=(uintptr_t)lv_event_get_user_data(e);
    uint32_t selected=lv_dropdown_get_selected(objects.curve_changer_panel__dropdown_step);
    if(direction==0){
        if(selected>0) selected--;
    }else if(direction==1){
        if(selected<cfg->max_idx) selected++;
    }
    lv_dropdown_set_selected(objects.curve_changer_panel__dropdown_step,selected);
    if(cfg==&STEPS_EXPO) curveStepSelectedInteger=selected;
    else curveStepSelectedFine=selected;
    updateCurveChangerStepButtons();
}
static void curveDoStep(lv_event_t *e){
    const StepConfig *cfg=getCurveStepConfig();
    if(!e||!cfg) return;
    uintptr_t command=(uintptr_t)lv_event_get_user_data(e);
    float value=getActiveCurveFieldValue();
    if(command==2){
        if(!objects.curve_changer_panel__mcmsusd_slider) return;
        int32_t sliderValue=lv_slider_get_value(objects.curve_changer_panel__mcmsusd_slider);
        value=curveFieldUsesDecimal()?sliderValue/10.0f:(float)sliderValue;
    }else{
        if(!objects.curve_changer_panel__dropdown_step) return;
        uint32_t selected=lv_dropdown_get_selected(objects.curve_changer_panel__dropdown_step);
        if(selected>cfg->max_idx) selected=cfg->max_idx;
        float step=cfg->steps[selected];
        if(command==0) value-=step;
        else if(command==1) value+=step;
        else return;
    }
    setActiveCurveFieldValue(value);
    if(command!=2) updateCurveChangerUi();
}
static void toggleChangerPanels(lv_obj_t *buttonPanel,lv_obj_t *sliderPanel){
    if(!buttonPanel||!sliderPanel) return;
    bool buttonHidden=lv_obj_has_flag(buttonPanel,LV_OBJ_FLAG_HIDDEN);
    bool sliderHidden=lv_obj_has_flag(sliderPanel,LV_OBJ_FLAG_HIDDEN);
    if(buttonHidden&&sliderHidden){
        lv_obj_remove_flag(buttonPanel,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(sliderPanel,LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if(buttonHidden){
        lv_obj_remove_flag(buttonPanel,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(sliderPanel,LV_OBJ_FLAG_HIDDEN);
    }else{
        lv_obj_add_flag(buttonPanel,LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(sliderPanel,LV_OBJ_FLAG_HIDDEN);
    }
}

static ScreensEnum current_active_screen = SCREEN_ID_SCR_MAIN;
static ScreensEnum mixes_return_screen = SCREEN_ID_SCR_MODEL;

// When CH settings are opened from a Free Mix output widget,
// GoBack must return to that same Free Mix editor, not Channels overview.
static bool channelEditorReturnToMix=false;
static int8_t channelEditorReturnMixIndex=-1;
static lv_coord_t channelEditorReturnMixScrollY=0;
ScreensEnum getParentScreen(ScreensEnum screen_id) {
    switch (screen_id) {
        // Уровень 4: Настройки конкретного канала -> Список каналов
        case SCREEN_ID_SCR_CH_SETTINGS:
            return SCREEN_ID_SCR_CHANNELS;
        // Уровень 3: Список каналов -> Модель
        case SCREEN_ID_SCR_CHANNELS:
            return SCREEN_ID_SCR_MODEL;
        case SCREEN_ID_SCR_MODEL_SELECT:
            return SCREEN_ID_SCR_MODEL;
        case SCREEN_ID_SCR_CURVE:
        case SCREEN_ID_SCR_VARS:
            return SCREEN_ID_SCR_MODEL;
        // Подэкраны Настроек -> Настройки
        case SCREEN_ID_SCR_SD:
        case SCREEN_ID_SCR_GENERAL_SETTINGS:
            return SCREEN_ID_SCR_SETTINGS;
        case SCREEN_ID_SCR_CALIBRATION_STICKS:
            return SCREEN_ID_SCR_SETTINGS;
        // Уровень 2: Вкладки первого уровня -> Главный экран
        case SCREEN_ID_SCR_MODEL:
        case SCREEN_ID_SCR_TELEMETRY:
        case SCREEN_ID_SCR_SETTINGS:
            return SCREEN_ID_SCR_MAIN;
        // Уровень 1: Главный экран (выше идти некуда)
        case SCREEN_ID_SCR_MAIN:
        default:
            return SCREEN_ID_SCR_MAIN;
    }
}
// Переносит текущие значения из ch_data в оперативную память канаcfg.outMin    = 1500 + (int)(ch_data.min_val * 5.12f);ла (channelConfigs)
static void sync_ch_data_to_config(uint8_t ch){
    if(ch>=MAX_MODEL_CHANNELS) return;
    ModelConfig &model=modelManager.getActiveModel();
    OutputConfig &output=model.outputs[ch];
    output.min=(int16_t)constrain((int32_t)lroundf(ch_data.min_val*10.0f),-1500L,0L);
    output.subtrim=(int16_t)constrain((int32_t)lroundf(ch_data.center_val*10.0f),-1000L,1000L);
    output.max=(int16_t)constrain((int32_t)lroundf(ch_data.max_val*10.0f),0L,1500L);
    output.expo=(int8_t)constrain((int32_t)lroundf(ch_data.expo_curve),-100L,100L);
    output.slowUp=(uint16_t)constrain((int32_t)lroundf(ch_data.slow_up*1000.0f),0L,25000L);
    output.slowDown=(uint16_t)constrain((int32_t)lroundf(ch_data.slow_down*1000.0f),0L,25000L);
    output.pwmCenterUs=(int16_t)constrain((int32_t)lroundf(ch_pwm_center),1000L,2000L);
}
FieldIndex active_field = FIELD_CENTER;
ChannelEditData ch_data = { -100.0f, 0.0f, 100.0f, 0.0f, 0.0f, 0.0f };
// Функция ВСЕГДА должна быть объявлена ВЫШЕ её первых вызовов
float* get_active_field_ptr() {
    if(active_field_is_pwm) return &ch_pwm_center;
    switch(active_field){
        case FIELD_MIN:return &ch_data.min_val;
        case FIELD_CENTER:return &ch_data.center_val;
        case FIELD_MAX:return &ch_data.max_val;
        case FIELD_EXPO_CURVE:return &ch_data.expo_curve;
        case FIELD_SLOW_UP:return &ch_data.slow_up;
        case FIELD_SLOW_DOWN:return &ch_data.slow_down;
        default:return nullptr;
    }
}
// Вспомогательная функция для получения диапазона допустимых значений для активного поля
void get_current_field_bounds(float &min_out, float &max_out) {
    if(active_field_is_pwm){
        min_out=1000.0f;
        max_out=2000.0f;
        return;
    }
    switch(active_field){
        case FIELD_MIN:
            min_out = -150.0f;
            max_out = 0.0f;
            break;
        case FIELD_CENTER: {
            // Границы центра строятся от текущих значений ch_min и ch_max
            float ch_min_val = ch_data.min_val;
            float ch_max_val = ch_data.max_val;
            // Защита, если min окажется больше max
            min_out = std::min(ch_min_val, ch_max_val);
            max_out = std::max(ch_min_val, ch_max_val);
            break;
        }
        case FIELD_MAX:
            min_out = 0.0f;
            max_out = 150.0f;
            break;
        case FIELD_EXPO_CURVE:
            min_out = -100.0f;
            max_out = 100.0f;
            break;
        case FIELD_SLOW_UP:
        case FIELD_SLOW_DOWN:
            min_out = 0.0f;
            max_out = 25.0f;
            break;
        default:
            min_out = -100.0f;
            max_out = 100.0f;
            break;
    }
}
// Вспомогательная функция для синхронной обрезки ch_center в пределах ch_min и ch_max
static void clamp_center_to_bounds() {
    float min_c, max_c;
    // Определяем текущие рамки для центра
    min_c = std::min(ch_data.min_val, ch_data.max_val);
    max_c = std::max(ch_data.min_val, ch_data.max_val);
    // Если центр вышел за новые рамки — усекаем его
    if (ch_data.center_val < min_c) ch_data.center_val = min_c;
    if (ch_data.center_val > max_c) ch_data.center_val = max_c;
    // Обновляем текст в UI для ch_center
    if (objects.ch_center) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f%%", ch_data.center_val);
        lv_label_set_text(objects.ch_center, buf);
    }
}
// 2. Функция связывания указателей (вызывается ПОСЛЕ ui_init)
void init_channel_widget_pointers() {
    ch_num_labels[0] = objects.widget_ch1__ch_num;
    ch_num_labels[1] = objects.widget_ch2__ch_num;
    ch_num_labels[2] = objects.widget_ch3__ch_num;
    ch_num_labels[3] = objects.widget_ch4__ch_num;
    ch_num_labels[4] = objects.widget_ch5__ch_num;
    ch_num_labels[5] = objects.widget_ch6__ch_num;
    ch_num_labels[6] = objects.widget_ch7__ch_num;
    ch_num_labels[7] = objects.widget_ch8__ch_num;

    ch_name_labels[0] = objects.widget_ch1__ch_name;
    ch_name_labels[1] = objects.widget_ch2__ch_name;
    ch_name_labels[2] = objects.widget_ch3__ch_name;
    ch_name_labels[3] = objects.widget_ch4__ch_name;
    ch_name_labels[4] = objects.widget_ch5__ch_name;
    ch_name_labels[5] = objects.widget_ch6__ch_name;
    ch_name_labels[6] = objects.widget_ch7__ch_name;
    ch_name_labels[7] = objects.widget_ch8__ch_name;

    ch_l_pans[0] = objects.widget_ch1__l_pan;
    ch_l_pans[1] = objects.widget_ch2__l_pan;
    ch_l_pans[2] = objects.widget_ch3__l_pan;
    ch_l_pans[3] = objects.widget_ch4__l_pan;
    ch_l_pans[4] = objects.widget_ch5__l_pan;
    ch_l_pans[5] = objects.widget_ch6__l_pan;
    ch_l_pans[6] = objects.widget_ch7__l_pan;
    ch_l_pans[7] = objects.widget_ch8__l_pan;

    ch_r_pans[0] = objects.widget_ch1__r_pan;
    ch_r_pans[1] = objects.widget_ch2__r_pan;
    ch_r_pans[2] = objects.widget_ch3__r_pan;
    ch_r_pans[3] = objects.widget_ch4__r_pan;
    ch_r_pans[4] = objects.widget_ch5__r_pan;
    ch_r_pans[5] = objects.widget_ch6__r_pan;
    ch_r_pans[6] = objects.widget_ch7__r_pan;
    ch_r_pans[7] = objects.widget_ch8__r_pan;

    ch_rev_icons[0] = objects.widget_ch1__ch_revers;
    ch_rev_icons[1] = objects.widget_ch2__ch_revers;
    ch_rev_icons[2] = objects.widget_ch3__ch_revers;
    ch_rev_icons[3] = objects.widget_ch4__ch_revers;
    ch_rev_icons[4] = objects.widget_ch5__ch_revers;
    ch_rev_icons[5] = objects.widget_ch6__ch_revers;
    ch_rev_icons[6] = objects.widget_ch7__ch_revers;
    ch_rev_icons[7] = objects.widget_ch8__ch_revers;

    ch_balance_icons[0]=objects.widget_ch1__ch_balance;
    ch_balance_icons[1]=objects.widget_ch2__ch_balance;
    ch_balance_icons[2]=objects.widget_ch3__ch_balance;
    ch_balance_icons[3]=objects.widget_ch4__ch_balance;
    ch_balance_icons[4]=objects.widget_ch5__ch_balance;
    ch_balance_icons[5]=objects.widget_ch6__ch_balance;
    ch_balance_icons[6]=objects.widget_ch7__ch_balance;
    ch_balance_icons[7]=objects.widget_ch8__ch_balance;

    ch_expo_icons[0] = objects.widget_ch1__ch_expo;
    ch_expo_icons[1] = objects.widget_ch2__ch_expo;
    ch_expo_icons[2] = objects.widget_ch3__ch_expo;
    ch_expo_icons[3] = objects.widget_ch4__ch_expo;
    ch_expo_icons[4] = objects.widget_ch5__ch_expo;
    ch_expo_icons[5] = objects.widget_ch6__ch_expo;
    ch_expo_icons[6] = objects.widget_ch7__ch_expo;
    ch_expo_icons[7] = objects.widget_ch8__ch_expo;

    ch_slow_icons[0] = objects.widget_ch1__ch_slow;
    ch_slow_icons[1] = objects.widget_ch2__ch_slow;
    ch_slow_icons[2] = objects.widget_ch3__ch_slow;
    ch_slow_icons[3] = objects.widget_ch4__ch_slow;
    ch_slow_icons[4] = objects.widget_ch5__ch_slow;
    ch_slow_icons[5] = objects.widget_ch6__ch_slow;
    ch_slow_icons[6] = objects.widget_ch7__ch_slow;
    ch_slow_icons[7] = objects.widget_ch8__ch_slow;

    ch_out_bars[0] = objects.widget_ch1__ch_bar;
    ch_out_bars[1] = objects.widget_ch2__ch_bar;
    ch_out_bars[2] = objects.widget_ch3__ch_bar;
    ch_out_bars[3] = objects.widget_ch4__ch_bar;
    ch_out_bars[4] = objects.widget_ch5__ch_bar;
    ch_out_bars[5] = objects.widget_ch6__ch_bar;
    ch_out_bars[6] = objects.widget_ch7__ch_bar;
    ch_out_bars[7] = objects.widget_ch8__ch_bar;

    ch_mix_bars[0] = objects.widget_ch1__mx_bar;
    ch_mix_bars[1] = objects.widget_ch2__mx_bar;
    ch_mix_bars[2] = objects.widget_ch3__mx_bar;
    ch_mix_bars[3] = objects.widget_ch4__mx_bar;
    ch_mix_bars[4] = objects.widget_ch5__mx_bar;
    ch_mix_bars[5] = objects.widget_ch6__mx_bar;
    ch_mix_bars[6] = objects.widget_ch7__mx_bar;
    ch_mix_bars[7] = objects.widget_ch8__mx_bar;

    ch_out_val_labels[0] = objects.widget_ch1__ch_val;
    ch_out_val_labels[1] = objects.widget_ch2__ch_val;
    ch_out_val_labels[2] = objects.widget_ch3__ch_val;
    ch_out_val_labels[3] = objects.widget_ch4__ch_val;
    ch_out_val_labels[4] = objects.widget_ch5__ch_val;
    ch_out_val_labels[5] = objects.widget_ch6__ch_val;
    ch_out_val_labels[6] = objects.widget_ch7__ch_val;
    ch_out_val_labels[7] = objects.widget_ch8__ch_val;

    ch_out_pct_labels[0] = objects.widget_ch1__ch_percent;
    ch_out_pct_labels[1] = objects.widget_ch2__ch_percent;
    ch_out_pct_labels[2] = objects.widget_ch3__ch_percent;
    ch_out_pct_labels[3] = objects.widget_ch4__ch_percent;
    ch_out_pct_labels[4] = objects.widget_ch5__ch_percent;
    ch_out_pct_labels[5] = objects.widget_ch6__ch_percent;
    ch_out_pct_labels[6] = objects.widget_ch7__ch_percent;
    ch_out_pct_labels[7] = objects.widget_ch8__ch_percent;

    ch_mix_pct_labels[0] = objects.widget_ch1__mx_percent;
    ch_mix_pct_labels[1] = objects.widget_ch2__mx_percent;
    ch_mix_pct_labels[2] = objects.widget_ch3__mx_percent;
    ch_mix_pct_labels[3] = objects.widget_ch4__mx_percent;
    ch_mix_pct_labels[4] = objects.widget_ch5__mx_percent;
    ch_mix_pct_labels[5] = objects.widget_ch6__mx_percent;
    ch_mix_pct_labels[6] = objects.widget_ch7__mx_percent;
    ch_mix_pct_labels[7] = objects.widget_ch8__mx_percent;

    ch_out_lines[0] = objects.widget_ch1__channel_line;
    ch_out_lines[1] = objects.widget_ch2__channel_line;
    ch_out_lines[2] = objects.widget_ch3__channel_line;
    ch_out_lines[3] = objects.widget_ch4__channel_line;
    ch_out_lines[4] = objects.widget_ch5__channel_line;
    ch_out_lines[5] = objects.widget_ch6__channel_line;
    ch_out_lines[6] = objects.widget_ch7__channel_line;
    ch_out_lines[7] = objects.widget_ch8__channel_line;

    ch_mix_lines[0] = objects.widget_ch1__mixer_line;
    ch_mix_lines[1] = objects.widget_ch2__mixer_line;
    ch_mix_lines[2] = objects.widget_ch3__mixer_line;
    ch_mix_lines[3] = objects.widget_ch4__mixer_line;
    ch_mix_lines[4] = objects.widget_ch5__mixer_line;
    ch_mix_lines[5] = objects.widget_ch6__mixer_line;
    ch_mix_lines[6] = objects.widget_ch7__mixer_line;
    ch_mix_lines[7] = objects.widget_ch8__mixer_line;
}
void update_channels_overview_live(){
    if(!isChannelsScreenActive) return;
    ModelConfig &model=modelManager.getActiveModel();
    for(uint8_t ch=0;ch<8;ch++){
        const OutputConfig &output=model.outputs[ch];
        float mixPercent=getChannelMixPercent(ch);
        int16_t outputNormalized=outputProcessor.getChannelNormalized(ch);
        float outputPercent=outputNormalized/10.0f;
        int outputUs=outputProcessor.getChannelUs(ch);
        int monitorUs=1500+((int32_t)outputNormalized*512L)/1000L;
        int monitorSubtrimUs=1500+((int32_t)output.subtrim*512L)/1000L;
        if(ch_out_val_labels[ch]!=NULL)
            lv_label_set_text_fmt(ch_out_val_labels[ch],"%4dus",outputUs);
        if(ch_out_pct_labels[ch]!=NULL){
            char buf[16];
            snprintf(buf,sizeof(buf),"%.1f%%",outputPercent);
            lv_label_set_text(ch_out_pct_labels[ch],buf);
        }
        if(ch_out_bars[ch]!=NULL){
            if(monitorUs>=monitorSubtrimUs){
                lv_bar_set_start_value(ch_out_bars[ch],monitorSubtrimUs,LV_ANIM_OFF);
                lv_bar_set_value(ch_out_bars[ch],monitorUs,LV_ANIM_OFF);
            }else{
                lv_bar_set_start_value(ch_out_bars[ch],monitorUs,LV_ANIM_OFF);
                lv_bar_set_value(ch_out_bars[ch],monitorSubtrimUs,LV_ANIM_OFF);
            }
        }
        if(ch_mix_pct_labels[ch]!=NULL){
            char buf[16];
            snprintf(buf,sizeof(buf),"%.1f%%",mixPercent);
            lv_label_set_text(ch_mix_pct_labels[ch],buf);
        }
        if(ch_mix_bars[ch]!=NULL){
            const int MIX_CENTER_US=1500;
            int mixUs=MIX_CENTER_US+(int)roundf(mixPercent*5.12f);
            mixUs=constrain(mixUs,732,2268);
            if(mixUs>=MIX_CENTER_US){
                lv_bar_set_start_value(ch_mix_bars[ch],MIX_CENTER_US,LV_ANIM_OFF);
                lv_bar_set_value(ch_mix_bars[ch],mixUs,LV_ANIM_OFF);
            }else{
                lv_bar_set_start_value(ch_mix_bars[ch],mixUs,LV_ANIM_OFF);
                lv_bar_set_value(ch_mix_bars[ch],MIX_CENTER_US,LV_ANIM_OFF);
            }
        }
    }
}
void load_channels_overview_ui(){
    ModelConfig &model = modelManager.getActiveModel();
    for (uint8_t ch = 0; ch < 8; ch++){
        const OutputConfig &output = model.outputs[ch];
        // Номер и имя канала
        if (ch_num_labels[ch] != NULL)
            lv_label_set_text_fmt(ch_num_labels[ch], "CH%d", ch + 1);
        if (ch_name_labels[ch] != NULL)
            lv_label_set_text(ch_name_labels[ch], output.name);
        // Reverse
        if (ch_rev_icons[ch] != NULL){
            if (output.reversed)
                lv_obj_remove_flag(ch_rev_icons[ch], LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(ch_rev_icons[ch], LV_OBJ_FLAG_HIDDEN);
        }
        if(ch_balance_icons[ch]!=NULL){
            if(output.balance.enabled)
                lv_obj_remove_flag(ch_balance_icons[ch],LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(ch_balance_icons[ch],LV_OBJ_FLAG_HIDDEN);
        }
        // Expo
        if (ch_expo_icons[ch] != NULL){
            int8_t curveSelection=output.curveIndex;
            bool curveEnabled=false;
            if(curveSelection==OUTPUT_CURVE_EXPO) curveEnabled=true;
            else if(curveSelection==OUTPUT_CURVE_LEGACY&&output.expo!=0) curveEnabled=true;
            else if(curveSelection>=0&&curveSelection<MAX_MODEL_CURVES&&model.curves[curveSelection].enabled) curveEnabled=true;
            if (curveEnabled)
                lv_obj_remove_flag(ch_expo_icons[ch], LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(ch_expo_icons[ch], LV_OBJ_FLAG_HIDDEN);
        }
        // Slow
        if (ch_slow_icons[ch] != NULL){
            bool slowEnabled = output.slowUp > 0 || output.slowDown > 0;
            if (slowEnabled)
                lv_obj_remove_flag(ch_slow_icons[ch], LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(ch_slow_icons[ch], LV_OBJ_FLAG_HIDDEN);
        }
        // Output monitor geometry is logical and must not move with PWM Center
        int monitorSubtrimUs=1500+((int32_t)output.subtrim*512L)/1000L;
        if (ch_out_bars[ch] != NULL)
            lv_bar_set_range(ch_out_bars[ch], 732, 2268);
        if (ch_mix_bars[ch] != NULL)
            lv_bar_set_range(ch_mix_bars[ch], 732, 2268);
        // Limit panels
        float minLimitPct=fabsf(output.min/10.0f);
        float maxLimitPct=output.max/10.0f;
        int l_width = map((int)roundf(minLimitPct), 0, 150, 189, 0);
        l_width = constrain(l_width, 0, 189);
        if (ch_l_pans[ch] != NULL)
            lv_obj_set_width(ch_l_pans[ch], l_width);
        int r_width = map((int)roundf(maxLimitPct), 0, 150, 189, 0);
        r_width = constrain(r_width, 0, 189);
        if (ch_r_pans[ch] != NULL)
            lv_obj_set_width(ch_r_pans[ch], r_width);
        // Center line
        int line_x=map(monitorSubtrimUs,732,2268,0,378);
        line_x = constrain(line_x, 0, 378);
        if (ch_out_lines[ch] != NULL)
            lv_obj_set_x(ch_out_lines[ch], line_x);
        if(ch_mix_lines[ch]!=NULL) lv_obj_set_x(ch_mix_lines[ch],189);
    }
}
void update_channel_settings_live(uint8_t ch){
    ModelConfig &model=modelManager.getActiveModel();
    if(ch>=model.channelCount) return;
    OutputConfig &output=model.outputs[ch];
    if(objects.ch_revers!=NULL)
        output.reversed=lv_obj_has_state(objects.ch_revers,LV_STATE_CHECKED);
    float mixPercent=getChannelMixPercent(ch);
    int16_t outputNormalized=outputProcessor.getChannelNormalized(ch);
    float outputPercent=outputNormalized/10.0f;
    int outputUs=outputProcessor.getChannelUs(ch);
    int monitorUs=1500+((int32_t)outputNormalized*512L)/1000L;
    int monitorSubtrimUs=1500+((int32_t)output.subtrim*512L)/1000L;
    if(objects.channel_editing_widget__ch_edit_val!=NULL)
        lv_label_set_text_fmt(objects.channel_editing_widget__ch_edit_val,"%4dus",outputUs);
    if(objects.channel_editing_widget__ch_edit_percent!=NULL){
        char percentBuf[16];
        snprintf(percentBuf,sizeof(percentBuf),"%.1f%%",outputPercent);
        lv_label_set_text(objects.channel_editing_widget__ch_edit_percent,percentBuf);
    }
    if(objects.channel_editing_widget__ch_edit_bar!=NULL){
        if(monitorUs>=monitorSubtrimUs){
            lv_bar_set_start_value(objects.channel_editing_widget__ch_edit_bar,monitorSubtrimUs,LV_ANIM_OFF);
            lv_bar_set_value(objects.channel_editing_widget__ch_edit_bar,monitorUs,LV_ANIM_OFF);
        }else{
            lv_bar_set_start_value(objects.channel_editing_widget__ch_edit_bar,monitorUs,LV_ANIM_OFF);
            lv_bar_set_value(objects.channel_editing_widget__ch_edit_bar,monitorSubtrimUs,LV_ANIM_OFF);
        }
    }
    if(objects.channel_editing_widget__mx_edit_percent!=NULL){
        char mxBuf[16];
        snprintf(mxBuf,sizeof(mxBuf),"%.1f%%",mixPercent);
        lv_label_set_text(objects.channel_editing_widget__mx_edit_percent,mxBuf);
    }
    if(objects.channel_editing_widget__mx_edit_bar!=NULL){
        const int MIX_CENTER_US=1500;
        int mixUs=MIX_CENTER_US+(int)roundf(mixPercent*5.12f);
        mixUs=constrain(mixUs,732,2268);
        if(mixUs>=MIX_CENTER_US){
            lv_bar_set_start_value(objects.channel_editing_widget__mx_edit_bar,MIX_CENTER_US,LV_ANIM_OFF);
            lv_bar_set_value(objects.channel_editing_widget__mx_edit_bar,mixUs,LV_ANIM_OFF);
        }else{
            lv_bar_set_start_value(objects.channel_editing_widget__mx_edit_bar,mixUs,LV_ANIM_OFF);
            lv_bar_set_value(objects.channel_editing_widget__mx_edit_bar,MIX_CENTER_US,LV_ANIM_OFF);
        }
    }
    float minLimitPct=fabsf(output.min/10.0f);
    float maxLimitPct=output.max/10.0f;
    int l_width=map((int)roundf(minLimitPct),0,150,399,0);
    l_width=constrain(l_width,0,399);
    if(objects.channel_editing_widget__l_pan_edit!=NULL)
        lv_obj_set_width(objects.channel_editing_widget__l_pan_edit,l_width);
    int r_width=map((int)roundf(maxLimitPct),0,150,399,0);
    r_width=constrain(r_width,0,399);
    if(objects.channel_editing_widget__r_pan_edit!=NULL)
        lv_obj_set_width(objects.channel_editing_widget__r_pan_edit,r_width);
    if(objects.channel_editing_widget__ch_edit_revers!=NULL){
        if(output.reversed) lv_obj_remove_flag(objects.channel_editing_widget__ch_edit_revers,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(objects.channel_editing_widget__ch_edit_revers,LV_OBJ_FLAG_HIDDEN);
    }
    if(objects.channel_editing_widget__ch_edit_expo!=NULL){
        int8_t curveSelection=output.curveIndex;
        bool curveEnabled=false;
        if(curveSelection==OUTPUT_CURVE_EXPO) curveEnabled=true;
        else if(curveSelection==OUTPUT_CURVE_LEGACY&&output.expo!=0) curveEnabled=true;
        else if(curveSelection>=0&&curveSelection<MAX_MODEL_CURVES&&model.curves[curveSelection].enabled) curveEnabled=true;
        if(curveEnabled) lv_obj_remove_flag(objects.channel_editing_widget__ch_edit_expo,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(objects.channel_editing_widget__ch_edit_expo,LV_OBJ_FLAG_HIDDEN);
    }
    if(objects.channel_editing_widget__ch_edit_slow!=NULL){
        if(output.slowUp>0||output.slowDown>0) lv_obj_remove_flag(objects.channel_editing_widget__ch_edit_slow,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(objects.channel_editing_widget__ch_edit_slow,LV_OBJ_FLAG_HIDDEN);
    }
    int ch_line_x=map(monitorSubtrimUs,732,2268,0,798);
    ch_line_x=constrain(ch_line_x,0,798);
    if(objects.channel_editing_widget__ch_edit_ch_line!=NULL)
        lv_obj_set_x(objects.channel_editing_widget__ch_edit_ch_line,ch_line_x);
    if(objects.channel_editing_widget__ch_edit_mx_line!=NULL)
        lv_obj_set_x(objects.channel_editing_widget__ch_edit_mx_line,399);
}
void load_channel_settings_to_ui(uint8_t ch){
    ModelConfig &model = modelManager.getActiveModel();
    if (ch >= model.channelCount) return;    
    const OutputConfig &output = model.outputs[ch];

    int8_t expo = output.expo;
    uint16_t slowUpMs = output.slowUp;
    uint16_t slowDownMs = output.slowDown;

    ch_data.min_val=output.min/10.0f;
    ch_data.center_val=output.subtrim/10.0f;
    ch_data.max_val=output.max/10.0f;
    ch_data.expo_curve = (float)expo;
    ch_data.slow_up=slowUpMs/1000.0f;
    ch_data.slow_down=slowDownMs/1000.0f;
    ch_pwm_center=(float)constrain((int32_t)output.pwmCenterUs,1000L,2000L);

    char buf[32];
    Serial.printf("\n--- Загрузка UI для канала CH%d (%s) ---\n", ch + 1, output.name);

    if (objects.channel_editing_widget__ch_edit_num != NULL)
        lv_label_set_text_fmt(objects.channel_editing_widget__ch_edit_num, "CH%d", ch + 1);

    if (objects.channel_editing_widget__ch_edit_name != NULL)
        lv_label_set_text(objects.channel_editing_widget__ch_edit_name, output.name);

    int centerUs=output.pwmCenterUs;
    int minUs=centerUs+(output.min*512L)/1000L;
    int maxUs=centerUs+(output.max*512L)/1000L;
    int subtrimUs=centerUs+(output.subtrim*512L)/1000L;

    if (objects.channel_editing_widget__ch_edit_bar != NULL)
        lv_bar_set_range(objects.channel_editing_widget__ch_edit_bar, 732, 2268);

    if (objects.channel_editing_widget__mx_edit_bar != NULL)
        lv_bar_set_range(objects.channel_editing_widget__mx_edit_bar, 732, 2268);

    float minLimitPct=ch_data.min_val;
    float maxLimitPct=ch_data.max_val;
    float centerPct=ch_data.center_val;

    int ch_line_x=map(subtrimUs,centerUs-768,centerUs+768,0,798);
    ch_line_x = constrain(ch_line_x, 0, 798);

    if (objects.channel_editing_widget__ch_edit_ch_line != NULL)
        lv_obj_set_x(objects.channel_editing_widget__ch_edit_ch_line, ch_line_x);

    if (objects.channel_editing_widget__ch_edit_mx_line != NULL)
        lv_obj_set_x(objects.channel_editing_widget__ch_edit_mx_line,399);

    int l_width = map((int)roundf(abs(minLimitPct)), 0, 150, 399, 0);
    l_width = constrain(l_width, 0, 399);

    if (objects.channel_editing_widget__l_pan_edit != NULL)
        lv_obj_set_width(objects.channel_editing_widget__l_pan_edit, l_width);

    int r_width = map((int)roundf(abs(maxLimitPct)), 0, 150, 399, 0);
    r_width = constrain(r_width, 0, 399);

    if (objects.channel_editing_widget__r_pan_edit != NULL)
        lv_obj_set_width(objects.channel_editing_widget__r_pan_edit, r_width);

    if (objects.channel_editing_widget__ch_edit_expo != NULL){
        if (expo != 0)
            lv_obj_remove_flag(objects.channel_editing_widget__ch_edit_expo, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(objects.channel_editing_widget__ch_edit_expo, LV_OBJ_FLAG_HIDDEN);
    }

    if (objects.channel_editing_widget__ch_edit_slow != NULL){
        if (slowUpMs != 0 || slowDownMs != 0)
            lv_obj_remove_flag(objects.channel_editing_widget__ch_edit_slow, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(objects.channel_editing_widget__ch_edit_slow, LV_OBJ_FLAG_HIDDEN);
    }

    if (objects.channel_editing_widget__ch_edit_revers != NULL){
        if (output.reversed)
            lv_obj_remove_flag(objects.channel_editing_widget__ch_edit_revers, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(objects.channel_editing_widget__ch_edit_revers, LV_OBJ_FLAG_HIDDEN);
    }

    if (objects.ch_name_editor != NULL)
        lv_textarea_set_text(objects.ch_name_editor, output.name);

    if (objects.ch_revers != NULL){
        if (output.reversed)
            lv_obj_add_state(objects.ch_revers, LV_STATE_CHECKED);
        else
            lv_obj_clear_state(objects.ch_revers, LV_STATE_CHECKED);
    }

    if (objects.ch_min != NULL){
        snprintf(buf, sizeof(buf), "%.1f%%", minLimitPct);
        lv_label_set_text(objects.ch_min, buf);
    }

    if (objects.ch_center != NULL){
        snprintf(buf, sizeof(buf), "%.1f%%", centerPct);
        lv_label_set_text(objects.ch_center, buf);
    }

    if (objects.ch_max != NULL){
        snprintf(buf, sizeof(buf), "%.1f%%", maxLimitPct);
        lv_label_set_text(objects.ch_max, buf);
    }

    if(objects.ch_pwm!=NULL){
        snprintf(buf,sizeof(buf),"%dus",(int)lroundf(ch_pwm_center));
        lv_label_set_text(objects.ch_pwm,buf);
    }

    if (objects.ch_expo_percent != NULL){
        snprintf(buf, sizeof(buf), "%d%%", expo);
        lv_label_set_text(objects.ch_expo_percent, buf);
    }

    if (objects.ch_slow_up != NULL){
        snprintf(buf, sizeof(buf), "%.1f с.", slowUpMs / 1000.0f);
        lv_label_set_text(objects.ch_slow_up, buf);
    }

    if (objects.ch_slow_down != NULL){
        snprintf(buf, sizeof(buf), "%.1f с.", slowDownMs / 1000.0f);
        lv_label_set_text(objects.ch_slow_down, buf);
    }
    updateOutputCurveSelectionUi(ch);
    updateChannelBalanceUi(ch);
}
void update_single_channel_widget(lv_obj_t* bar,lv_obj_t* val_lbl,lv_obj_t* pct_lbl,int physicalUs,float percent,int monitorCenterUs){
    if(val_lbl!=NULL) lv_label_set_text_fmt(val_lbl,"%dus",physicalUs);
    int monitorUs=1500+(int)roundf(percent*5.12f);
    monitorUs=constrain(monitorUs,732,2268);
    monitorCenterUs=constrain(monitorCenterUs,732,2268);
    if(bar!=NULL){
        if(monitorUs>=monitorCenterUs){
            lv_bar_set_start_value(bar,monitorCenterUs,LV_ANIM_OFF);
            lv_bar_set_value(bar,monitorUs,LV_ANIM_OFF);
        }else{
            lv_bar_set_start_value(bar,monitorUs,LV_ANIM_OFF);
            lv_bar_set_value(bar,monitorCenterUs,LV_ANIM_OFF);
        }
    }
    if(pct_lbl!=NULL){
        char percentBuf[16];
        snprintf(percentBuf,sizeof(percentBuf),"%.1f%%",percent);
        lv_label_set_text(pct_lbl,percentBuf);
    }
}
void update_channels(){
    if (calibrationManager.isCalibrating())
        return;
    updateBalanceStickPosition();
    ModelConfig &model = modelManager.getActiveModel();
    int outputUs[8];
    float outputPercent[8];
    for (uint8_t i = 0; i < 8; i++){
        outputUs[i] = outputProcessor.getChannelUs(i);
        outputPercent[i] = outputProcessor.getChannelNormalized(i) / 10.0f;
    }
    // Экран редактирования канала
    if (isChSettingsScreenActive){
        update_channel_settings_live(editedChannel);
        return;
    }
    // Экран списка каналов не открыт
    if (!isChannelsScreenActive)
        return;
    // Overview CH1...CH8
    update_single_channel_widget(objects.widget_ch1__ch_bar,objects.widget_ch1__ch_val,objects.widget_ch1__ch_percent,outputUs[0],outputPercent[0],1500+((int32_t)model.outputs[0].subtrim*512L)/1000L);
    update_single_channel_widget(objects.widget_ch2__ch_bar,objects.widget_ch2__ch_val,objects.widget_ch2__ch_percent,outputUs[1],outputPercent[1],1500+((int32_t)model.outputs[1].subtrim*512L)/1000L);
    update_single_channel_widget(objects.widget_ch3__ch_bar,objects.widget_ch3__ch_val,objects.widget_ch3__ch_percent,outputUs[2],outputPercent[2],1500+((int32_t)model.outputs[2].subtrim*512L)/1000L);
    update_single_channel_widget(objects.widget_ch4__ch_bar,objects.widget_ch4__ch_val,objects.widget_ch4__ch_percent,outputUs[3],outputPercent[3],1500+((int32_t)model.outputs[3].subtrim*512L)/1000L);
    update_single_channel_widget(objects.widget_ch5__ch_bar,objects.widget_ch5__ch_val,objects.widget_ch5__ch_percent,outputUs[4],outputPercent[4],1500+((int32_t)model.outputs[4].subtrim*512L)/1000L);
    update_single_channel_widget(objects.widget_ch6__ch_bar,objects.widget_ch6__ch_val,objects.widget_ch6__ch_percent,outputUs[5],outputPercent[5],1500+((int32_t)model.outputs[5].subtrim*512L)/1000L);
    update_single_channel_widget(objects.widget_ch7__ch_bar,objects.widget_ch7__ch_val,objects.widget_ch7__ch_percent,outputUs[6],outputPercent[6],1500+((int32_t)model.outputs[6].subtrim*512L)/1000L);
    update_single_channel_widget(objects.widget_ch8__ch_bar,objects.widget_ch8__ch_val,objects.widget_ch8__ch_percent,outputUs[7],outputPercent[7],1500+((int32_t)model.outputs[7].subtrim*512L)/1000L);
}
void load_all_calibration_limits(){
    for (uint8_t i = 0; i < 4; i++){
        if (sticks[i].isCalibrated()){
            globalStickLimits[i].min = sticks[i].getMin();
            globalStickLimits[i].center = sticks[i].getCenter();
            globalStickLimits[i].max = sticks[i].getMax();
            globalStickLimits[i].isCalibrated = true;
        }else{
            globalStickLimits[i].min = 0;
            globalStickLimits[i].center = 13216;
            globalStickLimits[i].max = 26432;
            globalStickLimits[i].isCalibrated = false;
        }
    }

    for (uint8_t i = 4; i < 8; i++){
        globalStickLimits[i].min = 0;
        globalStickLimits[i].center = 13216;
        globalStickLimits[i].max = 26432;
        globalStickLimits[i].isCalibrated = false;
    }
}
// 1. Новая функция: Переводит АЦП стика строго в диапазон [-1000 .. +1000]
int16_t getNormalizedStickValue(uint8_t ch) {
    if (ch >= 8) return 0;
    int32_t rawStick = get_sticks(ch); // Считываем ADS1115[cite: 4]
    // Берем готовые границы напрямую из RAM-массива
    int rMin = globalStickLimits[ch].min;
    int rCenter = globalStickLimits[ch].center;
    int rMax = globalStickLimits[ch].max;
    int result = 0;
    if (rawStick <= rCenter) result = map(rawStick, rMin, rCenter, -1000, 0);
    else result = map(rawStick, rCenter, rMax, 0, 1000);
    result = constrain(result, -1000, 1000);
    // Мертвая зона
    if (abs(result) <= 30) result = 0;
    return (int16_t)result;
}
// Чтение лимитов канала из NVS (988 и 2012 по умолчанию)
void getChannelLimits(uint8_t ch, int &outMin, int &outMax) {
    Preferences pref;
    char ns[16];
    snprintf(ns, sizeof(ns), "ch_cfg_%d", ch);
    if (pref.begin(ns, true)) {
        outMin = pref.getInt("min", 988);
        outMax = pref.getInt("max", 2012);
        pref.end();
    } else {
        outMin = 988;
        outMax = 2012;
    }
}
// КАЛИБРОВКА СТИКОВ
StickCalibration sticks[4] = {
    StickCalibration(0),   // Left X
    StickCalibration(1),   // Left Y
    StickCalibration(2),   // Right X
    StickCalibration(3)    // Right Y
};
StickCalibrationManager calibrationManager(sticks);
static lv_timer_t *calibrationTimer = nullptr;
static lv_obj_t *leftStickCircle = nullptr;
static lv_obj_t *leftStickLine = nullptr;
static lv_point_precise_t linePointsLeft[2];
static lv_obj_t *rightStickCircle = nullptr;
static lv_obj_t *rightStickLine = nullptr;
static lv_point_precise_t linePointsRight[2];
static int normalizeCalibrationValue(int value, int minValue, int maxValue){
    if (maxValue <= minValue) return 0;
    return constrain(map(value, minValue, maxValue, -1000, 1000), -1000, 1000);
}
static const int STICK_DEADZONE = 10;
static int applyDeadzone(int value, int deadzone){
    if (value > -deadzone && value < deadzone) return 0;
    if (value >= deadzone)
        return map(value, deadzone, 1000, 0, 1000);
    return map(value, -1000, -deadzone, -1000, 0);
}
// Фильтрация значений стика
static int filterStickValue(int value, int index){
    static int lastValue[4] = {0, 0, 0, 0};
    if (abs(value - lastValue[index]) <= 5)
        return lastValue[index];
    lastValue[index] = value;
    return value;
}
// Таймер калибровки
static void calibration_timer_cb(lv_timer_t *timer){
    if (!calibrationManager.isCalibrating()){
        Serial.println("CALIBRATION NOT ACTIVE");
        return;
    }
    StickCalibrationManager::RawData raw{};
    raw.leftX = get_sticks(0);
    raw.leftY = get_sticks(1);
    raw.rightX = get_sticks(2);
    raw.rightY = get_sticks(3);
    calibrationManager.update(raw);
}
// Запуск калибровки
static void start_stick_calibration(){
    if (calibrationTimer != nullptr){
        lv_timer_del(calibrationTimer);
        calibrationTimer = nullptr;
    }
    
    // Очистка левого стика
    if (leftStickCircle != nullptr) { lv_obj_del(leftStickCircle); leftStickCircle = nullptr; }
    if (leftStickLine != nullptr) { lv_obj_del(leftStickLine); leftStickLine = nullptr; }
    
    // Очистка правого стика
    if (rightStickCircle != nullptr) { lv_obj_del(rightStickCircle); rightStickCircle = nullptr; }
    if (rightStickLine != nullptr) { lv_obj_del(rightStickLine); rightStickLine = nullptr; }

    calibrationManager.start();

    // Создание графических элементов для обоих стиков
    createLeftStickLine();
    createLeftStickCircle();
    createRightStickLine();
    createRightStickCircle();

    calibrationTimer = lv_timer_create(calibration_timer_cb, 20, nullptr);
}
// Остановка калибровки
static void stop_stick_calibration(){
    if (calibrationTimer != nullptr){
        lv_timer_del(calibrationTimer);
        calibrationTimer = nullptr;
    }
    if (calibrationManager.isCalibrating()) calibrationManager.cancel();

    // Очистка LVGL объектов
    if (leftStickCircle != nullptr) { lv_obj_del(leftStickCircle); leftStickCircle = nullptr; }
    if (leftStickLine != nullptr) { lv_obj_del(leftStickLine); leftStickLine = nullptr; }
    if (rightStickCircle != nullptr) { lv_obj_del(rightStickCircle); rightStickCircle = nullptr; }
    if (rightStickLine != nullptr) { lv_obj_del(rightStickLine); rightStickLine = nullptr; }
}
// --- ЛЕВЫЙ СТИК ---
static void createLeftStickLine(){
    if (leftStickLine != nullptr) return;
    leftStickLine = lv_line_create(objects.left_canva);
    static lv_point_precise_t points[2] = {{73, 73}, {73, 73}};
    lv_line_set_points(leftStickLine, points, 2);
    lv_obj_set_style_line_width(leftStickLine, 3, LV_PART_MAIN);
    lv_obj_set_style_line_color(leftStickLine, lv_color_hex(0x000070), LV_PART_MAIN);
    lv_obj_set_style_line_opa(leftStickLine, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(leftStickLine, true, LV_PART_MAIN);
}
static void createLeftStickCircle(){
    if (leftStickCircle != nullptr) return;
    leftStickCircle = lv_obj_create(objects.left_canva);
    lv_obj_set_size(leftStickCircle, 8, 8);
    lv_obj_set_style_radius(leftStickCircle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(leftStickCircle, lv_color_hex(0x1683FF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(leftStickCircle, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(leftStickCircle, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(leftStickCircle, 0, LV_PART_MAIN);
    lv_obj_set_pos(leftStickCircle, 70, 70);
    lv_obj_move_foreground(leftStickCircle);
}
static void updateLeftStickCircle(int xValue, int yValue){
    if (leftStickCircle == nullptr) return;
    const int canvasSize = 150;
    const int circleSize = 8;
    const int maxPos = canvasSize - circleSize;
    int x = constrain(map(yValue, -1000, 1000, 0, maxPos), 0, maxPos);
    int y = constrain(map(xValue, -1000, 1000, maxPos, 0), 0, maxPos);
    lv_obj_set_pos(leftStickCircle, x, y);

    if (leftStickLine != nullptr){
        linePointsLeft[0].x = 73;
        linePointsLeft[0].y = 73;
        linePointsLeft[1].x = x + 4;
        linePointsLeft[1].y = y + 4;
        lv_line_set_points(leftStickLine, linePointsLeft, 2);
    }
}
// --- ПРАВЫЙ СТИК ---
static void createRightStickLine(){
    if (rightStickLine != nullptr) return;
    rightStickLine = lv_line_create(objects.right_canva);
    static lv_point_precise_t points[2] = {{73, 73}, {73, 73}};
    lv_line_set_points(rightStickLine, points, 2);
    lv_obj_set_style_line_width(rightStickLine, 3, LV_PART_MAIN);
    lv_obj_set_style_line_color(rightStickLine, lv_color_hex(0x000070), LV_PART_MAIN);
    lv_obj_set_style_line_opa(rightStickLine, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(rightStickLine, true, LV_PART_MAIN);
}
static void createRightStickCircle(){
    if (rightStickCircle != nullptr) return;
    rightStickCircle = lv_obj_create(objects.right_canva);
    lv_obj_set_size(rightStickCircle, 8, 8);
    lv_obj_set_style_radius(rightStickCircle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(rightStickCircle, lv_color_hex(0x1683FF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(rightStickCircle, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(rightStickCircle, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(rightStickCircle, 0, LV_PART_MAIN);
    lv_obj_set_pos(rightStickCircle, 70, 70);
    lv_obj_move_foreground(rightStickCircle);
}
static void updateRightStickCircle(int xValue, int yValue){
    if (rightStickCircle == nullptr) return;
    const int canvasSize = 150;
    const int circleSize = 8;
    const int maxPos = canvasSize - circleSize;
    int x = constrain(map(yValue, -1000, 1000, 0, maxPos), 0, maxPos);
    int y = constrain(map(xValue, -1000, 1000, maxPos, 0), 0, maxPos);
    lv_obj_set_pos(rightStickCircle, x, y);

    if (rightStickLine != nullptr){
        linePointsRight[0].x = 73;
        linePointsRight[0].y = 73;
        linePointsRight[1].x = x + 4;
        linePointsRight[1].y = y + 4;
        lv_line_set_points(rightStickLine, linePointsRight, 2);
    }
}
// Callback обновления интерфейса
static void calibration_update_callback(const StickCalibrationManager::CalibrationData& data){
    // Метки Левого стика
    lv_label_set_text_fmt(objects.lxmin, "%d", data.leftX.min);
    lv_label_set_text_fmt(objects.lxcenter, "%d", data.leftX.center);
    lv_label_set_text_fmt(objects.lxmax, "%d", data.leftX.max);
    lv_label_set_text_fmt(objects.lymin, "%d", data.leftY.min);
    lv_label_set_text_fmt(objects.lycenter, "%d", data.leftY.center);
    lv_label_set_text_fmt(objects.lymax, "%d", data.leftY.max);

    // Метки Правого стика
    lv_label_set_text_fmt(objects.rxmin, "%d", data.rightX.min);
    lv_label_set_text_fmt(objects.rxcenter, "%d", data.rightX.center);
    lv_label_set_text_fmt(objects.rxmax, "%d", data.rightX.max);
    lv_label_set_text_fmt(objects.rymin, "%d", data.rightY.min);
    lv_label_set_text_fmt(objects.rycenter, "%d", data.rightY.center);
    lv_label_set_text_fmt(objects.rymax, "%d", data.rightY.max);

    // Расчет позиций для левого стика
    int lx = 0, ly = 0;
    if ((data.leftX.max - data.leftX.min) > 1000) lx = normalizeCalibrationValue(data.leftX.current, data.leftX.min, data.leftX.max);
    if ((data.leftY.max - data.leftY.min) > 1000) ly = normalizeCalibrationValue(data.leftY.current, data.leftY.min, data.leftY.max);
    updateLeftStickCircle(lx, ly);

    // Расчет позиций для правого стика
    int rx = 0, ry = 0;
    if ((data.rightX.max - data.rightX.min) > 1000) rx = normalizeCalibrationValue(data.rightX.current, data.rightX.min, data.rightX.max);
    if ((data.rightY.max - data.rightY.min) > 1000) ry = normalizeCalibrationValue(data.rightY.current, data.rightY.min, data.rightY.max);
    updateRightStickCircle(rx, ry);
}
// Инициализация калибровки
void calibration_init(){
    calibrationManager.begin();
    calibrationManager.setUpdateCallback(calibration_update_callback);
    if (!calibrationManager.loadAll()){
        Serial.println("Stick calibration not found");
    }
}
static void showSavedCalibration(){
    if (!sticks[0].isCalibrated() || !sticks[1].isCalibrated() || !sticks[2].isCalibrated() || !sticks[3].isCalibrated()) return; 
    // LEFT X & Y
    lv_label_set_text_fmt(objects.lxmin, "%d", sticks[0].getMin());
    lv_label_set_text_fmt(objects.lxcenter, "%d", sticks[0].getCenter());
    lv_label_set_text_fmt(objects.lxmax, "%d", sticks[0].getMax());
    lv_label_set_text_fmt(objects.lymin, "%d", sticks[1].getMin());
    lv_label_set_text_fmt(objects.lycenter, "%d", sticks[1].getCenter());
    lv_label_set_text_fmt(objects.lymax, "%d", sticks[1].getMax());
    // RIGHT X & Y
    lv_label_set_text_fmt(objects.rxmin, "%d", sticks[2].getMin());
    lv_label_set_text_fmt(objects.rxcenter, "%d", sticks[2].getCenter());
    lv_label_set_text_fmt(objects.rxmax, "%d", sticks[2].getMax());
    lv_label_set_text_fmt(objects.rymin, "%d", sticks[3].getMin());
    lv_label_set_text_fmt(objects.rycenter, "%d", sticks[3].getCenter());
    lv_label_set_text_fmt(objects.rymax, "%d", sticks[3].getMax());
}
uint8_t editedChannel = 0;
static char title_buffer[32];
void ads_init(){
    Wire.begin(ADS_SDA, ADS_SCL);
    Wire.setTimeOut(20);
    Wire.setClock(400000);

    Wire.beginTransmission(ADS1115_ADDR);
    if (Wire.endTransmission() == 0) 
        Serial.println("ADS1115 #1 (0x48) подключен");
    else Serial.println("ADS1115 #1 ОШИБКА!");

    Wire.beginTransmission(ADS1115_ADDR2);
    if (Wire.endTransmission() == 0)
        Serial.printf("ADS1115 #2 (0x%02X) подключен\n", ADS1115_ADDR2);
    else Serial.printf("ADS1115 #2 (0x%02X) ОШИБКА!\n", ADS1115_ADDR2);
}
int16_t readADS1115(uint8_t addr, uint8_t channel) {
    if (channel > 3) return 0;    
    uint16_t config = 0xC3E3 | ((uint16_t)channel << 12);
    Wire.beginTransmission(addr);
    Wire.write(0x01);
    Wire.write(config >> 8);
    Wire.write(config & 0xFF);
    uint8_t result = Wire.endTransmission();
    if (result != 0) {
        Serial.printf("ADS CONFIG ERROR addr=0x%02X ch=%d code=%d\n", addr, channel, result);
        return 0;
    }
    delay(2);
    Wire.beginTransmission(addr);
    Wire.write(0x00);
    result = Wire.endTransmission(false);
    if (result != 0) {
        Serial.printf("ADS POINTER ERROR addr=0x%02X ch=%d code=%d\n", addr, channel, result);
        return 0;
    }
    uint8_t count = Wire.requestFrom(addr, (uint8_t)2);
    if (count != 2) {
        Serial.printf("ADS READ ERROR addr=0x%02X ch=%d count=%d\n", addr, channel, count);
        return 0;
    }
    return (int16_t)((Wire.read() << 8) | Wire.read());
}
int32_t get_sticks(int ch) {
    if (ch < 0 || ch > 15) return 0;
    static constexpr uint8_t addresses[4] = {ADS1115_ADDR, ADS1115_ADDR2, 0x49, 0x4B};
    uint8_t addr = addresses[ch / 4];
    uint8_t adc_channel = ch % 4;
    return readADS1115(addr, adc_channel);
}
void update_icons(){
    int rssi = WiFi.RSSI();
    lv_label_set_text_fmt(objects.main_page1__rssi_widget__rssi_widget_level_lbl, "%d", rssi);
    //Serial.printf("Wifi = %d\n", rssi);
}
void update_topbar_icon(int percent, int rssi){
    update_icons();
}
static lv_obj_t *sharedKeyboardTextarea=nullptr;

static void hideSharedKeyboard(){
    if(objects.keyboard1)
        lv_keyboard_set_textarea(objects.keyboard1,nullptr);

    if(objects.keybaerd_panel)
        lv_obj_add_flag(objects.keybaerd_panel,LV_OBJ_FLAG_HIDDEN);

    if(sharedKeyboardTextarea)
        lv_obj_remove_state(sharedKeyboardTextarea,LV_STATE_FOCUSED);

    sharedKeyboardTextarea=nullptr;
}

static void sharedKeyboardTextareaEvent(lv_event_t *e){
    if(!e||!objects.keybaerd_panel||!objects.keyboard1) return;

    lv_event_code_t code=lv_event_get_code(e);
    lv_obj_t *ta=(lv_obj_t *)lv_event_get_current_target(e);
    if(!ta) ta=(lv_obj_t *)lv_event_get_target(e);
    if(!ta) return;

    if(code==LV_EVENT_FOCUSED){
        sharedKeyboardTextarea=ta;
        lv_keyboard_set_textarea(objects.keyboard1,ta);
        lv_obj_remove_flag(objects.keybaerd_panel,LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(objects.keybaerd_panel);
        return;
    }

    if(code==LV_EVENT_DEFOCUSED&&sharedKeyboardTextarea==ta)
        hideSharedKeyboard();
}

static void sharedKeyboardEvent(lv_event_t *e){
    if(!e) return;

    lv_event_code_t code=lv_event_get_code(e);
    if(code==LV_EVENT_READY||code==LV_EVENT_CANCEL)
        hideSharedKeyboard();
}

static void bind_textarea_to_keyboard(lv_obj_t *ta){
    if(!ta||!objects.keybaerd_panel||!objects.keyboard1) return;

    // setup_all_inputs() может вызываться повторно.
    // Удаляем предыдущую такую же привязку перед добавлением.
    lv_obj_remove_event_cb(ta,sharedKeyboardTextareaEvent);
    lv_obj_add_event_cb(ta,sharedKeyboardTextareaEvent,LV_EVENT_ALL,nullptr);
}

static void setupSharedKeyboard(){
    if(!objects.keybaerd_panel||!objects.keyboard1) return;

    // На Hardware клавиатура является последним дочерним элементом hw_main_panel.
    // На других экранах общая клавиатура остаётся на верхнем LVGL-слое.
    lv_obj_t *keyboardParent=current_active_screen==SCREEN_ID_SCR_HW&&objects.hw_main_panel?objects.hw_main_panel:lv_layer_top();
    if(lv_obj_get_parent(objects.keybaerd_panel)!=keyboardParent) lv_obj_set_parent(objects.keybaerd_panel,keyboardParent);
    if(keyboardParent==objects.hw_main_panel){
        // Hardware: last child participates in hw_main_panel FLEX COLUMN.
        lv_obj_remove_flag(objects.keybaerd_panel,LV_OBJ_FLAG_FLOATING);
        lv_obj_set_size(objects.keybaerd_panel,LV_PCT(100),175);
        lv_obj_set_style_flex_grow(objects.keybaerd_panel,0,LV_PART_MAIN);
    }else{
        // Other screens keep their existing floating keyboard behavior.
        lv_obj_add_flag(objects.keybaerd_panel,LV_OBJ_FLAG_FLOATING);
        lv_obj_set_size(objects.keybaerd_panel,800,175);
        lv_obj_align(objects.keybaerd_panel,LV_ALIGN_BOTTOM_MID,0,0);
    }
    lv_obj_move_foreground(objects.keybaerd_panel);
    lv_obj_add_flag(objects.keybaerd_panel,LV_OBJ_FLAG_HIDDEN);

    lv_obj_remove_event_cb(objects.keyboard1,sharedKeyboardEvent);
    lv_obj_add_event_cb(objects.keyboard1,sharedKeyboardEvent,LV_EVENT_ALL,nullptr);

    bind_textarea_to_keyboard(objects.ch_name_editor);
    bind_textarea_to_keyboard(objects.curve_name);
    bind_textarea_to_keyboard(objects.mix_name);
    bind_textarea_to_keyboard(objects.aileron_mix_name);
    bind_textarea_to_keyboard(objects.vars_name);
    bind_textarea_to_keyboard(objects.vars_edit_comment);

    if(objects.mix_name){
        lv_textarea_set_one_line(objects.mix_name,true);
        lv_textarea_set_max_length(objects.mix_name,15);
    }
    if(objects.aileron_mix_name){
        lv_textarea_set_one_line(objects.aileron_mix_name,true);
        lv_textarea_set_max_length(objects.aileron_mix_name,15);
    }
}


// Hardware editor bindings. Generated EEZ objects are never modified here.
static constexpr uint8_t HW_SWITCH_ROWS=14;
static int8_t hwEditingRow=-1;
static bool hwLoadingSettings=false;

static lv_obj_t *hwSwitchSlotPanel(uint8_t row){
    lv_obj_t *items[]={objects.sw_panel_1,objects.sw_panel_2,objects.sw_panel_3,objects.sw_panel_4,objects.sw_panel_5,objects.sw_panel_6,objects.sw_panel_7,objects.sw_panel_8,objects.sw_panel_9,objects.sw_panel_10,objects.sw_panel_11,objects.sw_panel_12,objects.sw_panel_13,objects.sw_panel_14};
    return row<HW_SWITCH_ROWS?items[row]:nullptr;
}
static lv_obj_t *hwSwitchSlotLabel(uint8_t row){
    lv_obj_t *items[]={objects.sw_lbl_1,objects.sw_lbl_2,objects.sw_lbl_3,objects.sw_lbl_4,objects.sw_lbl_5,objects.sw_lbl_6,objects.sw_lbl_7,objects.sw_lbl_8,objects.sw_lbl_9,objects.sw_lbl_10,objects.sw_lbl_11,objects.sw_lbl_12,objects.sw_lbl_13,objects.sw_lbl_14};
    return row<HW_SWITCH_ROWS?items[row]:nullptr;
}
static lv_obj_t *hwSwitchModePanel(uint8_t row){
    lv_obj_t *items[]={objects.sw_pos_panel_1,objects.sw_pos_panel_2,objects.sw_pos_panel_3,objects.sw_pos_panel_4,objects.sw_pos_panel_5,objects.sw_pos_panel_6,objects.sw_pos_panel_7,objects.sw_pos_panel_8,objects.sw_pos_panel_9,objects.sw_pos_panel_10,objects.sw_pos_panel_11,objects.sw_pos_panel_12,objects.sw_pos_panel_13,objects.sw_pos_panel_14};
    return row<HW_SWITCH_ROWS?items[row]:nullptr;
}
static lv_obj_t *hwSwitchModeLabel(uint8_t row){
    lv_obj_t *items[]={objects.sw_pos_lbl_1,objects.sw_pos_lbl_2,objects.sw_pos_lbl_3,objects.sw_pos_lbl_4,objects.sw_pos_lbl_5,objects.sw_pos_lbl_6,objects.sw_pos_lbl_7,objects.sw_pos_lbl_8,objects.sw_pos_lbl_9,objects.sw_pos_lbl_10,objects.sw_pos_lbl_11,objects.sw_pos_lbl_12,objects.sw_pos_lbl_13,objects.sw_pos_lbl_14};
    return row<HW_SWITCH_ROWS?items[row]:nullptr;
}
static lv_obj_t *hwSwitchNameField(uint8_t row){
    lv_obj_t *items[]={objects.sw_name_1,objects.sw_name_2,objects.sw_name_3,objects.sw_name_4,objects.sw_name_5,objects.sw_name_6,objects.sw_name_7,objects.sw_name_8,objects.sw_name_9,objects.sw_name_10,objects.sw_name_11,objects.sw_name_12,objects.sw_name_13,objects.sw_name_14};
    return row<HW_SWITCH_ROWS?items[row]:nullptr;
}
static const char *hwSwitchModeText(uint8_t mode){
    switch(mode){
        case 1: return "2-POS";
        case 2: return "3-POS";
        case 3: return "Momentary";
        default: return "None";
    }
}
static void hwHidePickers(){
    if(objects.hw_select_switches_panel) lv_obj_add_flag(objects.hw_select_switches_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.hw_select_mode_panel) lv_obj_add_flag(objects.hw_select_mode_panel,LV_OBJ_FLAG_HIDDEN);
    hwEditingRow=-1;
}
static void hwShowPicker(lv_obj_t *picker){
    if(!picker) return;
    lv_obj_remove_flag(picker,LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(picker);
}
static void hwOpenSlotPicker(lv_event_t *e){
    if(lv_event_get_code(e)!=LV_EVENT_CLICKED) return;
    hwEditingRow=(int8_t)(uintptr_t)lv_event_get_user_data(e);
    if(objects.hw_select_mode_panel) lv_obj_add_flag(objects.hw_select_mode_panel,LV_OBJ_FLAG_HIDDEN);
    hwShowPicker(objects.hw_select_switches_panel);
}
static void hwOpenModePicker(lv_event_t *e){
    if(lv_event_get_code(e)!=LV_EVENT_CLICKED) return;
    hwEditingRow=(int8_t)(uintptr_t)lv_event_get_user_data(e);
    if(objects.hw_select_switches_panel) lv_obj_add_flag(objects.hw_select_switches_panel,LV_OBJ_FLAG_HIDDEN);
    hwShowPicker(objects.hw_select_mode_panel);
}
static void hwSelectSlot(lv_event_t *e){
    if(lv_event_get_code(e)!=LV_EVENT_CLICKED||hwEditingRow<0||hwEditingRow>=HW_SWITCH_ROWS) return;
    uint8_t selectedSlot=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(selectedSlot>=HW_SWITCH_ROWS) return;

    const uint8_t row=(uint8_t)hwEditingRow;
    Preferences prefs;
    if(!prefs.begin("hwsettings",false)) return;

    uint8_t slots[HW_SWITCH_ROWS];
    for(uint8_t i=0;i<HW_SWITCH_ROWS;i++){
        char key[16];
        snprintf(key,sizeof(key),"swslot%u",(unsigned)(i+1));
        slots[i]=prefs.getUChar(key,i);
        if(slots[i]>=HW_SWITCH_ROWS) slots[i]=i;
    }

    uint8_t previousSlot=slots[row];
    if(previousSlot!=selectedSlot){
        int8_t otherRow=-1;
        for(uint8_t i=0;i<HW_SWITCH_ROWS;i++){
            if(i!=row&&slots[i]==selectedSlot){otherRow=(int8_t)i;break;}
        }

        char key[16];
        snprintf(key,sizeof(key),"swslot%u",(unsigned)(row+1));
        prefs.putUChar(key,selectedSlot);
        if(otherRow>=0){
            snprintf(key,sizeof(key),"swslot%u",(unsigned)(otherRow+1));
            prefs.putUChar(key,previousSlot);
            lv_obj_t *otherLabel=hwSwitchSlotLabel((uint8_t)otherRow);
            if(otherLabel) lv_label_set_text_fmt(otherLabel,"Switch %u",(unsigned)(previousSlot+1));
        }
        lv_obj_t *label=hwSwitchSlotLabel(row);
        if(label) lv_label_set_text_fmt(label,"Switch %u",(unsigned)(selectedSlot+1));
    }

    prefs.end();
    if(previousSlot!=selectedSlot) inputManager.reloadHardwareMapping();
    hwHidePickers();
}
static void hwSelectMode(lv_event_t *e){
    if(lv_event_get_code(e)!=LV_EVENT_CLICKED||hwEditingRow<0||hwEditingRow>=HW_SWITCH_ROWS) return;
    uint8_t mode=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(mode>3) return;
    Preferences prefs;
    if(prefs.begin("hwsettings",false)){
        char key[16];snprintf(key,sizeof(key),"swtype%u",(unsigned)(hwEditingRow+1));
        prefs.putUChar(key,mode);
        prefs.end();
    }
    inputManager.reloadHardwareMapping();
    lv_obj_t *label=hwSwitchModeLabel(hwEditingRow);
    if(label) lv_label_set_text(label,hwSwitchModeText(mode));
    hwHidePickers();
}
static void hwSwitchNameChanged(lv_event_t *e){
    if(hwLoadingSettings||lv_event_get_code(e)!=LV_EVENT_DEFOCUSED) return;
    uint8_t row=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    lv_obj_t *field=hwSwitchNameField(row);
    if(!field) return;
    const char *name=lv_textarea_get_text(field);
    Preferences prefs;
    if(prefs.begin("hwsettings",false)){
        char key[16];snprintf(key,sizeof(key),"swname%u",(unsigned)(row+1));
        prefs.putString(key,name?name:"");
        prefs.end();
    }
}
// Analog Hardware labels are stored separately from physical ADS channel mapping.
static constexpr uint8_t HW_ANALOG_LABEL_COUNT=10;
static lv_obj_t *hwAnalogNameField(uint8_t row){
    lv_obj_t *fields[]={objects.obj11,objects.obj14,objects.obj17,objects.obj20,objects.obj26,objects.obj32,objects.obj35,objects.obj38,objects.obj44,objects.obj50};
    return row<HW_ANALOG_LABEL_COUNT?fields[row]:nullptr;
}
static void hwAnalogNameChanged(lv_event_t *e){
    if(hwLoadingSettings||lv_event_get_code(e)!=LV_EVENT_DEFOCUSED) return;
    uint8_t row=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    lv_obj_t *field=hwAnalogNameField(row);
    if(!field) return;
    Preferences prefs;
    if(!prefs.begin("hwsettings",false)) return;
    char key[16];
    snprintf(key,sizeof(key),"aname%u",(unsigned)(row+1));
    prefs.putString(key,lv_textarea_get_text(field));
    prefs.end();
    sourceResolver.reloadHardwareAnalogNames();
}
// EEZ stores the text labels (Enable/Invert) in objects.obj25/obj31/obj43/obj49.
// The actual lv_switch is the preceding sibling inside the same parent panel.
static lv_obj_t *hwAnalogToggle(uint8_t index){
    lv_obj_t *labels[]={objects.obj25,objects.obj31,objects.obj43,objects.obj49};
    if(index>=4 || !labels[index]) return nullptr;
    lv_obj_t *parent=lv_obj_get_parent(labels[index]);
    if(!parent) return nullptr;
    lv_obj_t *toggle=lv_obj_get_child(parent,1);
    if(!toggle || !lv_obj_check_type(toggle,&lv_switch_class)) return nullptr;
    return toggle;
}
static void hwAnalogToggleChanged(lv_event_t *e){
    if(hwLoadingSettings||lv_event_get_code(e)!=LV_EVENT_VALUE_CHANGED) return;
    uint8_t index=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    lv_obj_t *toggle=hwAnalogToggle(index);
    if(!toggle) return;
    const char *keys[]={"ext1en","ext2en","slider1inv","slider2inv"};
    if(index>=4) return;
    bool selected=lv_obj_has_state(toggle,LV_STATE_CHECKED);
    Preferences prefs;
    if(prefs.begin("hwsettings",false)){
        prefs.putBool(keys[index],selected);
        prefs.end();
    }
    inputManager.reloadAnalogControlSettings();
    mixer.resetRuntime();
}
static void setupHardwareAnalogToggles(){
    static const char *keys[]={"ext1en","ext2en","slider1inv","slider2inv"};
    Preferences prefs;
    bool opened=prefs.begin("hwsettings",true);
    hwLoadingSettings=true;
    for(uint8_t i=0;i<4;i++){
        lv_obj_t *toggle=hwAnalogToggle(i);
        if(!toggle) continue;
        bool selected=opened?prefs.getBool(keys[i],i<2):i<2;
        if(selected) lv_obj_add_state(toggle,LV_STATE_CHECKED);
        else lv_obj_remove_state(toggle,LV_STATE_CHECKED);
        lv_obj_remove_event_cb(toggle,hwAnalogToggleChanged);
        lv_obj_add_event_cb(toggle,hwAnalogToggleChanged,LV_EVENT_VALUE_CHANGED,(void *)(uintptr_t)i);
    }
    hwLoadingSettings=false;
    if(opened) prefs.end();
    inputManager.reloadAnalogControlSettings();
}
static void setupHardwareAnalogNames(){
    static const char *defaults[]={"S1","S2","S3","S4","Ext1","Ext2","L1","L2","Slider1","Slider2"};
    Preferences prefs;
    bool opened=prefs.begin("hwsettings",true);
    hwLoadingSettings=true;
    for(uint8_t i=0;i<HW_ANALOG_LABEL_COUNT;i++){
        lv_obj_t *field=hwAnalogNameField(i);
        if(!field) continue;
        char key[16];
        snprintf(key,sizeof(key),"aname%u",(unsigned)(i+1));
        String value=opened?prefs.getString(key,defaults[i]):String(defaults[i]);
        lv_textarea_set_max_length(field,32);
        lv_textarea_set_text(field,value.c_str());
        bind_textarea_to_keyboard(field);
        lv_obj_remove_event_cb(field,hwAnalogNameChanged);
        lv_obj_add_event_cb(field,hwAnalogNameChanged,LV_EVENT_DEFOCUSED,(void *)(uintptr_t)i);
    }
    hwLoadingSettings=false;
    if(opened) prefs.end();
    sourceResolver.reloadHardwareAnalogNames();
}

// Hardware middle position delay: fixed 10 ms increment, 0..200 ms.
static bool hwMiddleDelayEditing=false;
static bool hwMiddleDelayUiUpdating=false;
static uint16_t hwMiddleDelayValue=0;
static uint16_t hwMiddleDelaySaved=0;
static constexpr uint16_t HW_MIDDLE_DELAY_MAX=200;
static constexpr uint16_t HW_MIDDLE_DELAY_STEP=10;

static void hwMiddleDelayUpdateUi(){
    hwMiddleDelayUiUpdating=true;
    if(objects.sw_mid_delay_lbl) lv_label_set_text_fmt(objects.sw_mid_delay_lbl,"%u ms",(unsigned)hwMiddleDelayValue);
    if(objects.hw_changer_panel__dropdown_step){
        lv_dropdown_set_options(objects.hw_changer_panel__dropdown_step,"10 ms");
        lv_dropdown_set_selected(objects.hw_changer_panel__dropdown_step,0);
    }
    if(objects.hw_changer_panel__mcmsusd_slider){
        lv_slider_set_range(objects.hw_changer_panel__mcmsusd_slider,0,HW_MIDDLE_DELAY_MAX);
        lv_slider_set_value(objects.hw_changer_panel__mcmsusd_slider,hwMiddleDelayValue,LV_ANIM_OFF);
    }
    if(objects.hw_changer_panel__reduce_step) lv_obj_add_state(objects.hw_changer_panel__reduce_step,LV_STATE_DISABLED);
    if(objects.hw_changer_panel__increase_step) lv_obj_add_state(objects.hw_changer_panel__increase_step,LV_STATE_DISABLED);
    if(objects.hw_changer_panel__step_down){
        if(hwMiddleDelayValue==0) lv_obj_add_state(objects.hw_changer_panel__step_down,LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.hw_changer_panel__step_down,LV_STATE_DISABLED);
    }
    if(objects.hw_changer_panel__step_up){
        if(hwMiddleDelayValue>=HW_MIDDLE_DELAY_MAX) lv_obj_add_state(objects.hw_changer_panel__step_up,LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.hw_changer_panel__step_up,LV_STATE_DISABLED);
    }
    hwMiddleDelayUiUpdating=false;
}

static void hwMiddleDelaySave(){
    Preferences prefs;
    if(prefs.begin("hwsettings",false)){
        prefs.putUShort("swMidDelay",hwMiddleDelayValue);
        prefs.end();
    }
    inputManager.reloadHardwareMapping();
}

static void hwMiddleDelayDoStep(lv_event_t *e){
    if(!e||hwMiddleDelayUiUpdating) return;
    uintptr_t cmd=(uintptr_t)lv_event_get_user_data(e);
    int32_t next=hwMiddleDelayValue;
    if(cmd==0) next-=HW_MIDDLE_DELAY_STEP;
    else if(cmd==1) next+=HW_MIDDLE_DELAY_STEP;
    else if(cmd==2&&objects.hw_changer_panel__mcmsusd_slider){
        int32_t raw=lv_slider_get_value(objects.hw_changer_panel__mcmsusd_slider);
        next=((raw+HW_MIDDLE_DELAY_STEP/2)/HW_MIDDLE_DELAY_STEP)*HW_MIDDLE_DELAY_STEP;
    }else return;
    next=constrain(next,0,(int32_t)HW_MIDDLE_DELAY_MAX);
    if(next==hwMiddleDelayValue) return;
    hwMiddleDelayValue=(uint16_t)next;
    hwMiddleDelayUpdateUi();
}

// EEZ generated callback registrations can differ across revisions.
// Bind the existing Hardware controls directly and only once per screen setup.
static void hwMiddleDelayBindControls(){
    struct Binding {lv_obj_t *obj;lv_event_code_t code;uintptr_t command;};
    Binding binds[]={
        {objects.hw_changer_panel__step_down,LV_EVENT_CLICKED,0},
        {objects.hw_changer_panel__step_up,LV_EVENT_CLICKED,1},
        {objects.hw_changer_panel__mcmsusd_slider,LV_EVENT_VALUE_CHANGED,2}
    };
    for(const Binding &b:binds){
        if(!b.obj) continue;
        lv_obj_remove_event_cb(b.obj,action_do_step);
        lv_obj_remove_event_cb(b.obj,hwMiddleDelayDoStep);
        lv_obj_add_event_cb(b.obj,hwMiddleDelayDoStep,b.code,(void *)b.command);
    }
}

static void hwMiddleDelayClose(){
    if(hwMiddleDelayEditing&&hwMiddleDelayValue!=hwMiddleDelaySaved){
        hwMiddleDelaySave();
        hwMiddleDelaySaved=hwMiddleDelayValue;
    }
    hwMiddleDelayEditing=false;
    if(objects.hw_changer_panel) lv_obj_add_flag(objects.hw_changer_panel,LV_OBJ_FLAG_HIDDEN);
}

static void hwMiddleDelayOpen(lv_event_t *e){
    if(!e) return;
    lv_event_code_t code=lv_event_get_code(e);
    if(code!=LV_EVENT_FOCUSED&&code!=LV_EVENT_CLICKED) return;
    if(!objects.hw_changer_panel) return;
    hwMiddleDelayEditing=true;
    if(objects.hw_changer_panel__button_panel) lv_obj_remove_flag(objects.hw_changer_panel__button_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.hw_changer_panel__slider_panel) lv_obj_add_flag(objects.hw_changer_panel__slider_panel,LV_OBJ_FLAG_HIDDEN);
    hwMiddleDelayUpdateUi();
    lv_obj_move_foreground(objects.hw_changer_panel);
    lv_obj_remove_flag(objects.hw_changer_panel,LV_OBJ_FLAG_HIDDEN);
}

static void hwMiddleDelaySetup(){
    Preferences prefs;
    bool opened=prefs.begin("hwsettings",true);
    hwMiddleDelayValue=opened?prefs.getUShort("swMidDelay",0):0;
    if(opened) prefs.end();
    if(hwMiddleDelayValue>HW_MIDDLE_DELAY_MAX) hwMiddleDelayValue=HW_MIDDLE_DELAY_MAX;
    hwMiddleDelaySaved=hwMiddleDelayValue;
    hwMiddleDelayBindControls();
    hwMiddleDelayUpdateUi();
    if(objects.hw_changer_panel) lv_obj_add_flag(objects.hw_changer_panel,LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *targets[]={objects.sw_mid_delay_panel,objects.sw_mid_delay_lbl};
    for(lv_obj_t *target:targets){
        if(!target) continue;
        lv_obj_add_flag(target,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_event_cb(target,hwMiddleDelayOpen);
        lv_obj_add_event_cb(target,hwMiddleDelayOpen,LV_EVENT_CLICKED,nullptr);
        lv_obj_add_event_cb(target,hwMiddleDelayOpen,LV_EVENT_FOCUSED,nullptr);
    }
}

// Dismiss Hardware editors when focus moves to another control.
// Both panels remain ordinary children of hw_main_panel in the EEZ Flex layout.
static bool hwIsEditableName(lv_obj_t *target){
    if(!target) return false;
    for(uint8_t i=0;i<HW_SWITCH_ROWS;i++){
        lv_obj_t *name=hwSwitchNameField(i);
        if(name&&isChildOf(target,name)) return true;
    }
    for(uint8_t i=0;i<HW_ANALOG_LABEL_COUNT;i++){
        lv_obj_t *name=hwAnalogNameField(i);
        if(name&&isChildOf(target,name)) return true;
    }
    return false;
}

static void hwDismissEditorsOnOutsidePress(lv_event_t *e){
    if(!e||current_active_screen!=SCREEN_ID_SCR_HW) return;
    lv_obj_t *target=(lv_obj_t *)lv_event_get_target(e);
    if(!target) return;

    // A visible Hardware picker closes when the press is outside its bounds.
    // Keep selection events inside the picker working normally.
    bool slotPickerOpen=objects.hw_select_switches_panel&&!lv_obj_has_flag(objects.hw_select_switches_panel,LV_OBJ_FLAG_HIDDEN);
    bool modePickerOpen=objects.hw_select_mode_panel&&!lv_obj_has_flag(objects.hw_select_mode_panel,LV_OBJ_FLAG_HIDDEN);
    if(slotPickerOpen||modePickerOpen){
        bool insideSlot=slotPickerOpen&&isChildOf(target,objects.hw_select_switches_panel);
        bool insideMode=modePickerOpen&&isChildOf(target,objects.hw_select_mode_panel);
        if(!insideSlot&&!insideMode) hwHidePickers();
    }

    if(hwMiddleDelayEditing){
        if(objects.hw_changer_panel&&isChildOf(target,objects.hw_changer_panel)) return;
        if(objects.sw_mid_delay_panel&&isChildOf(target,objects.sw_mid_delay_panel)) return;
        if(objects.sw_mid_delay_lbl&&isChildOf(target,objects.sw_mid_delay_lbl)) return;
        hwMiddleDelayClose();
    }

    if(objects.keybaerd_panel&&!lv_obj_has_flag(objects.keybaerd_panel,LV_OBJ_FLAG_HIDDEN)){
        if(isChildOf(target,objects.keybaerd_panel)) return;
        if(hwIsEditableName(target)) return; // Switching to another text field is handled by focus.
        hideSharedKeyboard();
    }
}

static void hwBindOutsidePressRecursive(lv_obj_t *root){
    if(!root) return;
    // Interacting with the editor itself must not close it.
    if(objects.hw_changer_panel&&isChildOf(root,objects.hw_changer_panel)) return;
    if(objects.keybaerd_panel&&isChildOf(root,objects.keybaerd_panel)) return;
    lv_obj_remove_event_cb(root,hwDismissEditorsOnOutsidePress);
    lv_obj_add_event_cb(root,hwDismissEditorsOnOutsidePress,LV_EVENT_PRESSED,nullptr);
    uint32_t count=lv_obj_get_child_count(root);
    for(uint32_t i=0;i<count;i++) hwBindOutsidePressRecursive(lv_obj_get_child(root,(int32_t)i));
}

static void setupHardwareSwitchEditor(){
    if(!objects.scr_hw) return;
    hwHidePickers();
    hwLoadingSettings=true;
    Preferences prefs;
    bool opened=prefs.begin("hwsettings",true);
    for(uint8_t i=0;i<HW_SWITCH_ROWS;i++){
        lv_obj_t *slot=hwSwitchSlotPanel(i);
        lv_obj_t *mode=hwSwitchModePanel(i);
        lv_obj_t *name=hwSwitchNameField(i);
        if(slot){
            lv_obj_remove_event_cb(slot,hwOpenSlotPicker);
            lv_obj_add_event_cb(slot,hwOpenSlotPicker,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
        }
        if(mode){
            lv_obj_remove_event_cb(mode,hwOpenModePicker);
            lv_obj_add_event_cb(mode,hwOpenModePicker,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
        }
        char key[16];
        snprintf(key,sizeof(key),"swslot%u",(unsigned)(i+1));
        uint8_t hardwareSlot=opened?prefs.getUChar(key,i):i;
        if(hardwareSlot>=HW_SWITCH_ROWS) hardwareSlot=i;
        if(hwSwitchSlotLabel(i)) lv_label_set_text_fmt(hwSwitchSlotLabel(i),"Switch %u",(unsigned)(hardwareSlot+1));
        snprintf(key,sizeof(key),"swtype%u",(unsigned)(i+1));
        uint8_t type=opened?prefs.getUChar(key,i<6?2:1):(i<6?2:1);
        if(type>3) type=0;
        if(hwSwitchModeLabel(i)) lv_label_set_text(hwSwitchModeLabel(i),hwSwitchModeText(type));
        snprintf(key,sizeof(key),"swname%u",(unsigned)(i+1));
        String label=opened?prefs.getString(key,""):String("");
        if(name){
            lv_textarea_set_max_length(name,32);
            lv_textarea_set_text(name,label.c_str());
            bind_textarea_to_keyboard(name);
            lv_obj_remove_event_cb(name,hwSwitchNameChanged);
            lv_obj_add_event_cb(name,hwSwitchNameChanged,LV_EVENT_DEFOCUSED,(void *)(uintptr_t)i);
        }
    }
    if(opened) prefs.end();
    lv_obj_t *choices[]={objects.obj90,objects.obj91,objects.obj92,objects.obj93,objects.obj94,objects.obj95,objects.obj96,objects.obj97,objects.obj98,objects.obj99,objects.obj100,objects.obj101,objects.obj102,objects.obj103};
    for(uint8_t i=0;i<HW_SWITCH_ROWS;i++){
        if(!choices[i]) continue;
        lv_obj_remove_event_cb(choices[i],hwSelectSlot);
        lv_obj_add_event_cb(choices[i],hwSelectSlot,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
    }
    lv_obj_t *modes[]={objects.hw_mode_none,objects.hw_mode_2pos,objects.hw_mode_3pos,objects.hw_mode_momentary};
    for(uint8_t i=0;i<4;i++){
        if(!modes[i]) continue;
        lv_obj_remove_event_cb(modes[i],hwSelectMode);
        lv_obj_add_event_cb(modes[i],hwSelectMode,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
    }
    hwLoadingSettings=false;
}

static void ensureInputRuntimeStarted(){
    if(inputRuntimeTimer!=nullptr) return;

    // InputManager is initialized once in setup(), before ui_init().
    // Do not reinitialize hardware inputs when opening the Mix editor.
    updateInputRuntime(nullptr);
    inputRuntimeTimer=lv_timer_create(updateInputRuntime,20,nullptr);
}

void setup_all_inputs() {
    setupSharedKeyboard();
    setupMixOutputUiCallbacks();
    bindAileronEditorCallbacks();
    setupVarsUiCallbacks();
    ensureInputRuntimeStarted();
}

static bool isModelEditorScreen(ScreensEnum screen){
    return screen==SCREEN_ID_SCR_MIXES||screen==SCREEN_ID_SCR_CURVE||screen==SCREEN_ID_SCR_VARS;
}

static void saveEditorChangesOnExit(){
    if(!modelManager.flushDeferredEditorSave())
        Serial.println("ERROR: deferred model editor save failed");
}

void openScreen(ScreensEnum screen_id) {
    if(hwMiddleDelayEditing) hwMiddleDelayClose();
    if(isModelEditorScreen(current_active_screen)&&current_active_screen!=screen_id) saveEditorChangesOnExit();
    modelManager.setEditorSaveDeferred(isModelEditorScreen(screen_id));
    current_active_screen = screen_id;
    loadScreen(screen_id);

    // Клавиатура общая для Channel / Curve / Mix.
    // Инициализируем её при любом переходе экрана, а не только после
    // первого открытия редактора каналов.
    setupSharedKeyboard();

    if(screen_id==SCREEN_ID_SCR_HW){setupHardwareSwitchEditor();setupHardwareAnalogNames();setupHardwareAnalogToggles();hwMiddleDelaySetup();hwBindOutsidePressRecursive(objects.scr_hw);}

    if(screen_id==SCREEN_ID_SCR_VARS){
        if(isVarMoveMode()) exitVarMoveMode();
        setupVarsUiCallbacks();
        refreshVarsList();
        if(objects.vars_main_panel) lv_obj_remove_flag(objects.vars_main_panel,LV_OBJ_FLAG_HIDDEN);
        if(objects.vars_addedit_main_panel) lv_obj_add_flag(objects.vars_addedit_main_panel,LV_OBJ_FLAG_HIDDEN);
    }

    update_BB_widget_visibility(screen_id);
}
const char* getScreenTitle(ScreensEnum screen_id) {
    switch (screen_id) {
        case SCREEN_ID_SCR_MAIN:               return "Главная";
        case SCREEN_ID_SCR_MODEL:              return "Модель";
        case SCREEN_ID_SCR_MODEL_SELECT:       return "Выбор модели";
        case SCREEN_ID_SCR_CURVE:              return "Кривые";
        case SCREEN_ID_SCR_VARS:               return "Vars";
        case SCREEN_ID_SCR_MIXES:              return "Mixes";
        case SCREEN_ID_SCR_CHANNELS:           return "Каналы";
        // Выводим "Канал 1" .. "Канал 8"
        case SCREEN_ID_SCR_CH_SETTINGS:
            snprintf(title_buffer, sizeof(title_buffer), "Канал %d", (int)editedChannel + 1);
            return title_buffer;
        case SCREEN_ID_SCR_TELEMETRY:          return "Телеметрия";
        case SCREEN_ID_SCR_SETTINGS:           return "Настройки";
        case SCREEN_ID_SCR_GENERAL_SETTINGS:   return "Общие";
        case SCREEN_ID_SCR_SD:                 return "SD-карта";
        case SCREEN_ID_SCR_CALIBRATION_STICKS: return "Калибровка";

        default:                               return "";
    }
}
static int8_t activeMixOutputSlot=-1;
static uint8_t pendingMixOutputChannel=MIX_OUTPUT_NONE;
static lv_obj_t *mixOutputCallbacksBoundScreen=nullptr;
static lv_obj_t *sourceSelectorCallbacksBoundScreen=nullptr;

enum class SharedSelectorMode:uint8_t{
    SOURCE=0,
    ACTIVE_CONDITION,
    ACTION_ACTIVE_CONDITION,
    VARIANT_ACTIVE_CONDITION
};

static SharedSelectorMode sharedSelectorMode=SharedSelectorMode::SOURCE;

static bool mixSourceStickFocusDetectActive=false;
static bool mixSourceAnalogMembersActive=false;
static int16_t mixSourceStickFocusBaseline[static_cast<uint8_t>(StickFunction::COUNT)]={};
static int16_t mixSourceAnalogFocusBaseline[static_cast<uint8_t>(AnalogInput::COUNT)]={};
static int16_t mixSourceSwitchFocusBaseline[14]={};
static int16_t mixSourceSwitchLastSample[14]={};
static uint8_t mixSourceSwitchStableCount[14]={};
static constexpr uint8_t MIX_SOURCE_SWITCH_DEBOUNCE_SAMPLES=2;
static constexpr int16_t MIX_SOURCE_STICK_FOCUS_THRESHOLD=80;

static ConditionConfig mixActionEditorCondition{};
static ConditionConfig mixActionVariantConditions[MAX_MIX_ACTION_VARIANTS]={};
static MixActionType activeMixVariantConditionType=MixActionType::WEIGHT;
static uint8_t activeMixVariantConditionSlot=1;
static constexpr int16_t ACTION_DIRECTION_MARKER=0x4449;

enum class ActionDirection:uint8_t{
    RIGHT=0,
    LEFT,
    TOP,
    BOTTOM
};

static bool sharedSelectorIsConditionMode(){
    return sharedSelectorMode==SharedSelectorMode::ACTIVE_CONDITION||
           sharedSelectorMode==SharedSelectorMode::ACTION_ACTIVE_CONDITION||
           sharedSelectorMode==SharedSelectorMode::VARIANT_ACTIVE_CONDITION;
}

static lv_obj_t *getMixSourceSelectorRoot();
static void setupMixSourceSelectorCallbacks();
static void configureSharedSelectorForSource();
static void configureSharedSelectorForActiveCondition();
static void configureSharedSelectorForActionActiveCondition();
static void configureSharedSelectorForVariantActiveCondition();
static void orderSharedSelectorRows(lv_obj_t *const *rows,uint8_t count);
static void actionMixActiveConditionShow(lv_event_t *e);
static void actionMixActionActiveConditionShow(lv_event_t *e);
static void actionMixVariantActiveConditionShow(lv_event_t *e);
static void activeConditionDirectionCategoryClicked(lv_event_t *e);
static void activeConditionDirectionClicked(lv_event_t *e);
static void buildActiveConditionDirectionMembers();
static void focusCurrentActionDirectionSelection();
static void syncMixDirectionConditionsToSource(MixLine &mix);
static bool mixSourceUsesHorizontalDirection(const MixLine &mix);
static void updateMixActionConditionEditorUi();
static void updateMixActionVariantConditionEditorUi(MixActionType type,uint8_t slot);
static lv_obj_t *getMixActionVariantConditionPanel(MixActionType type,uint8_t slot);
static bool getMixActionVariantConditionTarget(lv_obj_t *target,MixActionType &type,uint8_t &slot);
static ConditionConfig *getSharedConditionTarget();
static void updateSharedConditionTargetUi();
static void closeSharedConditionSelector();
static void clearMixSourceMembers();
static void buildMixAnalogMembers();
static void mixSourceCategoryNoneClicked(lv_event_t *e);
static void mixSourceCategoryAnalogClicked(lv_event_t *e);
static void mixSourceMemberClicked(lv_event_t *e);
static void startMixSourceStickFocusDetect();
static void updateMixSourceStickFocusDetect();
static void updateVarConditionAutoDetect();
static void stopMixSourceStickFocusDetect();
static void focusMixAnalogMember(StickFunction function);
static bool isMixSourceSelectorVisible();
static bool moveMixSourceMemberFocus(int8_t direction);
static bool pressFocusedMixSourceMember();
static void ensureInputRuntimeStarted();
static void buildActiveConditionSwitchStubMembers();
static void activeConditionSwitchStubClicked(lv_event_t *e);
static void activeConditionSwitchCategoryClicked(lv_event_t *e);
static void buildActiveConditionFunctionSwitchStubMembers();
static void buildActiveConditionLogicSwitchStubMembers();
static void buildActiveConditionTrimStubMembers();
static void buildActiveConditionFlightModeStubMembers();
static void buildActiveConditionSystemEventStubMembers();
static void activeConditionFunctionSwitchCategoryClicked(lv_event_t *e);
static void activeConditionFunctionSwitchClicked(lv_event_t *e);
static void activeConditionLogicSwitchCategoryClicked(lv_event_t *e);
static void activeConditionLogicSwitchClicked(lv_event_t *e);
static void activeConditionTrimCategoryClicked(lv_event_t *e);
static void activeConditionTrimClicked(lv_event_t *e);
static void activeConditionFlightModeCategoryClicked(lv_event_t *e);
static void activeConditionFlightModeClicked(lv_event_t *e);
static void activeConditionSystemEventCategoryClicked(lv_event_t *e);
static void activeConditionGenericStubClicked(lv_event_t *e);
static void updateMixSourceEditorUi();
static int16_t getMixSourceRawLiveValue(const MixLine &mix);
static SourceId getMixBaseSource(const MixLine &mix);
static uint8_t hardwareSwitchType(uint8_t slot);
static bool mixSourceSupportsHalfRange(const MixLine &mix);
static bool mixSourceHalfRangeEnabled(const MixLine &mix);
static void setMixSourceHalfRange(MixLine &mix,bool enabled);
static int32_t applyMixSourceOptionsValue(int32_t value,const MixLine &mix);
static void ensureSourceHalfRangeRow();
static void setSourceHalfRangeRowVisible(bool visible);
static void sourceHalfRangeChanged(lv_event_t *e);
static void buildMixSwitchSourceMembers();
static void buildMixLogicSourceMembers();
static void mixSourceSwitchCategoryClicked(lv_event_t *e);
static void mixSourceSwitchMemberClicked(lv_event_t *e);
static void mixSourceLogicMemberClicked(lv_event_t *e);
static void updateMixActiveConditionEditorUi();
static void setMixActiveConditionAlways();
static void setMixActiveConditionNever();
static void showPopupDismissOverlay(lv_obj_t *popup);
static void hidePopupDismissOverlay();
static void deletePopupDismissOverlayAsync(void *param);
static void popupDismissOverlayClicked(lv_event_t *e);
static void showMixOperationPopupAsync(void *param);
static void openMixActionEditorAsync(void *param);
void action_invert_panel_show(lv_event_t *e);
static lv_obj_t *getActiveConditionInvertCheckbox();
enum class InvertTargetMode:uint8_t{MIX=0,ACTION,VARIANT,SOURCE};
static InvertTargetMode invertTargetMode=InvertTargetMode::MIX;
static bool suppressMixActiveConditionClick=false;
static bool suppressMixSourceClick=false;
static bool suppressActionActiveConditionClick=false;
static bool suppressVariantActiveConditionClick=false;

// Mix Source option storage.
// Eligible Source indices are all below 0x80, so the high bit can persist
// Half range without changing the model binary layout/version.
static constexpr uint8_t MIX_SOURCE_HALF_RANGE_FLAG=0x80;
static lv_obj_t *sourceHalfRangeRow=nullptr;
static lv_obj_t *sourceHalfRangeCheckbox=nullptr;
static lv_obj_t *mixOptionSelectedRow=nullptr;

static lv_obj_t *popupDismissOverlay=nullptr;
static bool mixOperationPopupShowPending=false;
static bool mixActionOpenPending=false;
static uint8_t pendingMixActionMixIndex=0;
static uint8_t pendingMixActionIndex=0;

void goBack() {
    if(hwMiddleDelayEditing){hwMiddleDelayClose();return;}
    if(current_active_screen==SCREEN_ID_SCR_HW){
        if(objects.hw_select_switches_panel&&!lv_obj_has_flag(objects.hw_select_switches_panel,LV_OBJ_FLAG_HIDDEN)){hwHidePickers();return;}
        if(objects.hw_select_mode_panel&&!lv_obj_has_flag(objects.hw_select_mode_panel,LV_OBJ_FLAG_HIDDEN)){hwHidePickers();return;}
    }
    if(popupDismissOverlay){
        popupDismissOverlayClicked(nullptr);
        return;
    }

    if(objects.keybaerd_panel!=NULL&&
       !lv_obj_has_flag(objects.keybaerd_panel,LV_OBJ_FLAG_HIDDEN)){
        hideSharedKeyboard();
        return;
    }

    if(current_active_screen==SCREEN_ID_SCR_MIXES){
        if(objects.operation_panel!=NULL&&
           !lv_obj_has_flag(objects.operation_panel,LV_OBJ_FLAG_HIDDEN)){
            hideMixOperationPopup();
            return;
        }

        if(objects.curve_popup!=NULL&&
           lv_obj_get_parent(objects.curve_popup)==objects.scr_mixes&&
           !lv_obj_has_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN)){
            lv_obj_add_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN);
            return;
        }
        if(objects.ch_edit_curve_list!=NULL&&
           lv_obj_get_parent(objects.ch_edit_curve_list)==objects.scr_mixes&&
           !lv_obj_has_flag(objects.ch_edit_curve_list,LV_OBJ_FLAG_HIDDEN)){
            lv_obj_add_flag(objects.ch_edit_curve_list,LV_OBJ_FLAG_HIDDEN);
            return;
        }

        if(objects.channels_list_in_mix!=NULL&&!lv_obj_has_flag(objects.channels_list_in_mix,LV_OBJ_FLAG_HIDDEN)){
            lv_obj_add_flag(objects.channels_list_in_mix,LV_OBJ_FLAG_HIDDEN);
            activeMixOutputSlot=-1;
            pendingMixOutputChannel=MIX_OUTPUT_NONE;
            return;
        }

        if(objects.mix_changer_panel!=NULL&&!lv_obj_has_flag(objects.mix_changer_panel,LV_OBJ_FLAG_HIDDEN)){
            hideMixActionChanger();
            return;
        }

        if(objects.action_type_selcet_panel!=NULL&&!lv_obj_has_flag(objects.action_type_selcet_panel,LV_OBJ_FLAG_HIDDEN)){
            lv_obj_add_flag(objects.action_type_selcet_panel,LV_OBJ_FLAG_HIDDEN);
            return;
        }

        if(objects.trim_type_select_panel!=NULL&&!lv_obj_has_flag(objects.trim_type_select_panel,LV_OBJ_FLAG_HIDDEN)){
            lv_obj_add_flag(objects.trim_type_select_panel,LV_OBJ_FLAG_HIDDEN);
            return;
        }

        bool actionEditorVisible=
            objects.mix_edit_new_action_panel!=NULL&&
            !lv_obj_has_flag(objects.mix_edit_new_action_panel,LV_OBJ_FLAG_HIDDEN);

        if(actionEditorVisible){
            if(!saveMixActionEditor()) return;

            // mix_editing_diag_panel is shared by Mix Editor and Action Editor.
            // Returning from Action Editor must NOT move the canvas to the Mix list.
            setMixGraphActionPreview(true);

            lv_obj_add_flag(objects.mix_edit_new_action_panel,LV_OBJ_FLAG_HIDDEN);
            editingMixActionIndex=-1;
            mixActionEditorIsNew=false;

            if(objects.free_mixes_edit_panel!=NULL)
                lv_obj_remove_flag(objects.free_mixes_edit_panel,LV_OBJ_FLAG_HIDDEN);

            rebuildFreeMixActionList();
            applyMixEditorEncoderFocus();

            if(selectedMixIndex>=0)
                drawMixGraph((uint8_t)selectedMixIndex);

            if(objects.global_status_bar__caption!=NULL)
                lv_label_set_text(objects.global_status_bar__caption,"Mixes");

            return;
        }

        bool editorVisible=objects.mix_addedit_panel!=NULL&&!lv_obj_has_flag(objects.mix_addedit_panel,LV_OBJ_FLAG_HIDDEN);
        bool typeListVisible=objects.all_mixes_panel!=NULL&&!lv_obj_has_flag(objects.all_mixes_panel,LV_OBJ_FLAG_HIDDEN);

        if(editorVisible||typeListVisible){
            setMixGraphActionPreview(false);

            if(editingMixIndex>=0&&editingMixIndex<MAX_MIX_LINES){
                saveMixNameFromEditor();
                if(isAileronMixEditor()) saveAileronEditorState();

                ModelConfig &model=modelManager.getActiveModel();
                MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

                // A mix without assigned channels is valid and remains in the model.
                if(mix.enabled) modelManager.requestSaveActiveModel();

                editingMixIndex=-1;
                editingMixActionIndex=-1;
                mixActionEditorIsNew=false;
            }

            if(objects.mix_addedit_panel!=NULL)
                lv_obj_add_flag(objects.mix_addedit_panel,LV_OBJ_FLAG_HIDDEN);
            if(objects.all_mixes_panel!=NULL)
                lv_obj_add_flag(objects.all_mixes_panel,LV_OBJ_FLAG_HIDDEN);
            if(objects.mix_main_panel!=NULL)
                lv_obj_remove_flag(objects.mix_main_panel,LV_OBJ_FLAG_HIDDEN);

            saveEditorChangesOnExit();
            refreshMixList();
            if(objects.global_status_bar__caption!=NULL)
                lv_label_set_text(objects.global_status_bar__caption,"Mixes");
            return;
        }

        ScreensEnum target=mixes_return_screen;
        if(target==SCREEN_ID_SCR_MIXES)
            target=SCREEN_ID_SCR_MODEL;

        openScreen(target);
        return;
    }
    if(current_active_screen==SCREEN_ID_SCR_VARS){
        if(isVarMoveMode()){
            exitVarMoveMode();
            return;
        }
        if(objects.vars_changer_panel&&!lv_obj_has_flag(objects.vars_changer_panel,LV_OBJ_FLAG_HIDDEN)){
            hideVarChanger();
            return;
        }
        if(objects.vars_function_panel&&!lv_obj_has_flag(objects.vars_function_panel,LV_OBJ_FLAG_HIDDEN)){
            lv_obj_add_flag(objects.vars_function_panel,LV_OBJ_FLAG_HIDDEN);
            return;
        }
        if(objects.vars_trim_type_select_panel&&!lv_obj_has_flag(objects.vars_trim_type_select_panel,LV_OBJ_FLAG_HIDDEN)){
            lv_obj_add_flag(objects.vars_trim_type_select_panel,LV_OBJ_FLAG_HIDDEN);
            return;
        }
        if(objects.vars_active_condition_panel&&!lv_obj_has_flag(objects.vars_active_condition_panel,LV_OBJ_FLAG_HIDDEN)){
            lv_obj_add_flag(objects.vars_active_condition_panel,LV_OBJ_FLAG_HIDDEN);
            return;
        }
        if(objects.vars_addedit_main_panel&&!lv_obj_has_flag(objects.vars_addedit_main_panel,LV_OBJ_FLAG_HIDDEN)){
            saveVarEditorText();
            if(objects.vars_addedit_main_panel) lv_obj_add_flag(objects.vars_addedit_main_panel,LV_OBJ_FLAG_HIDDEN);
            if(objects.vars_main_panel) lv_obj_remove_flag(objects.vars_main_panel,LV_OBJ_FLAG_HIDDEN);
            saveEditorChangesOnExit();
            refreshVarsList();
            return;
        }
    }

    if(current_active_screen==SCREEN_ID_SCR_CH_SETTINGS&&objects.curve_balance_panel!=NULL&&!lv_obj_has_flag(objects.curve_balance_panel,LV_OBJ_FLAG_HIDDEN)){
        balancePointEditActive=false;
        balanceCanvasEditMode=false;
        outputProcessor.setBalancePositionOverride(editedChannel,false,0);
        mixer.setInputLock(false);
        balanceLockedXValue=0;
        for(uint8_t i=0;i<BALANCE_ENCODER_NAV_COUNT;i++){
            lv_obj_t *obj=getBalanceEncoderNavObject(i);
            if(obj!=NULL) lv_obj_remove_state(obj,LV_STATE_FOCUSED);
        }
        if(objects.curve_balance_canvas!=NULL) lv_obj_remove_state(objects.curve_balance_canvas,LV_STATE_CHECKED);
        if(objects.stick_position_panel!=NULL) lv_obj_remove_state(objects.stick_position_panel,LV_STATE_CHECKED);
        if(objects.magnet_panel!=NULL) lv_obj_remove_state(objects.magnet_panel,LV_STATE_CHECKED);
        if(objects.balance_editor_lock_panel!=NULL) lv_obj_remove_state(objects.balance_editor_lock_panel,LV_STATE_CHECKED);
        if(objects.balance_editor_settings_panel!=NULL) lv_obj_remove_state(objects.balance_editor_settings_panel,LV_STATE_CHECKED);
        if(objects.balance_editor_about_panel!=NULL) lv_obj_remove_state(objects.balance_editor_about_panel,LV_STATE_CHECKED);
        if(objects.ch_edit_changer_panel!=NULL)
            lv_obj_add_flag(objects.ch_edit_changer_panel,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.curve_balance_panel,LV_OBJ_FLAG_HIDDEN);
        if(objects.ch_editing_panel!=NULL)
            lv_obj_remove_flag(objects.ch_editing_panel,LV_OBJ_FLAG_HIDDEN);
        if(objects.topbar_panel!=NULL)
            lv_obj_remove_flag(objects.topbar_panel,LV_OBJ_FLAG_HIDDEN);
        if(objects.channel_editing_widget!=NULL)
            lv_obj_remove_flag(objects.channel_editing_widget,LV_OBJ_FLAG_HIDDEN);
        modelManager.requestSaveActiveModel();
        updateChannelBalanceUi(editedChannel);
        return;
    }
    if(current_active_screen==SCREEN_ID_SCR_CURVE&&objects.curve_edit_panel!=NULL&&!lv_obj_has_flag(objects.curve_edit_panel,LV_OBJ_FLAG_HIDDEN)){
        int8_t savedCurveIndex=editingCurveIndex;
        saveCurveEditor();
        saveEditorChangesOnExit();
        lv_obj_add_flag(objects.curve_edit_panel,LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_parent(objects.curve_diag,objects.curve_diag_panel);
        lv_obj_remove_flag(objects.curve_main_panel,LV_OBJ_FLAG_HIDDEN);
        if(curveEditorReturnToMixAction||curveEditorReturnToAileron){
            bool returnAileron=curveEditorReturnToAileron;
            curveEditorReturnToAileron=false;
            curveEditorReturnToMixAction=false;

            if(savedCurveIndex>=0&&savedCurveIndex<MAX_MODEL_CURVES&&activeMixCurveSlot<6){
                if(returnAileron){
                    aileronEditorState.curveSelection[activeMixCurveSlot]=savedCurveIndex;
                    saveAileronEditorState();
                }else mixActionCurveSelection[activeMixCurveSlot]=savedCurveIndex;
                modelManager.requestSaveActiveModel();
            }

            editingCurveIndex=-1;
            selectedCurveIndex=savedCurveIndex;

            openScreen(SCREEN_ID_SCR_MIXES);

            if(objects.mix_main_panel)
                lv_obj_add_flag(objects.mix_main_panel,LV_OBJ_FLAG_HIDDEN);

            if(objects.all_mixes_panel)
                lv_obj_add_flag(objects.all_mixes_panel,LV_OBJ_FLAG_HIDDEN);

            if(objects.mix_addedit_panel)
                lv_obj_remove_flag(objects.mix_addedit_panel,LV_OBJ_FLAG_HIDDEN);

            if(objects.free_mixes_edit_panel)
                lv_obj_add_flag(objects.free_mixes_edit_panel,LV_OBJ_FLAG_HIDDEN);

            if(objects.mix_edit_new_action_panel){
                if(returnAileron) lv_obj_add_flag(objects.mix_edit_new_action_panel,LV_OBJ_FLAG_HIDDEN);
                else lv_obj_remove_flag(objects.mix_edit_new_action_panel,LV_OBJ_FLAG_HIDDEN);
            }
            if(objects.aileron_mixes_edit_panel){
                if(returnAileron) lv_obj_remove_flag(objects.aileron_mixes_edit_panel,LV_OBJ_FLAG_HIDDEN);
                else lv_obj_add_flag(objects.aileron_mixes_edit_panel,LV_OBJ_FLAG_HIDDEN);
            }
            if(returnAileron) updateAileronEditorUi();

            curvePickerForMixAction=true;
            setMixGraphActionPreview(true);
            updateMixCurveSelectionUi(activeMixCurveSlot);

            if(selectedMixIndex>=0)
                drawMixGraph((uint8_t)selectedMixIndex);

            return;
        }

        if(curveEditorReturnToChannel){
            curveEditorReturnToChannel=false;
            if(savedCurveIndex>=0&&savedCurveIndex<MAX_MODEL_CURVES&&editedChannel<MAX_MODEL_CHANNELS){
                ModelConfig &model=modelManager.getActiveModel();
                model.outputs[editedChannel].curveIndex=savedCurveIndex;
                modelManager.requestSaveActiveModel();
            }
            editingCurveIndex=-1;
            selectedCurveIndex=savedCurveIndex;
            openScreen(SCREEN_ID_SCR_CH_SETTINGS);
            isChannelsScreenActive=false;
            isChSettingsScreenActive=true;
            load_channel_settings_to_ui(editedChannel);
            setup_all_inputs();
            update_dropdown_step_options();
            updateOutputCurveSelectionUi(editedChannel);
            return;
        }
        lv_obj_remove_flag(objects.curve_main_panel,LV_OBJ_FLAG_HIDDEN);
        selectedCurveIndex=savedCurveIndex;
        refreshCurveList();
        if(selectedCurveIndex>=0) drawCurveGraph((uint8_t)selectedCurveIndex);
        editingCurveIndex=-1;
        return;
    }
    // Leaving channel settings: save first, then return to the actual caller.
    if(current_active_screen==SCREEN_ID_SCR_CH_SETTINGS){
        action_save_channel_edit(NULL);
        isChSettingsScreenActive=false;

        if(channelEditorReturnToMix){
            int8_t returnMixIndex=channelEditorReturnMixIndex;
            lv_coord_t returnScrollY=channelEditorReturnMixScrollY;

            channelEditorReturnToMix=false;
            channelEditorReturnMixIndex=-1;
            channelEditorReturnMixScrollY=0;
            isChannelsScreenActive=false;

            openScreen(SCREEN_ID_SCR_MIXES);

            if(returnMixIndex>=0&&returnMixIndex<MAX_MIX_LINES){
                openExistingMixEditor((uint8_t)returnMixIndex);

                lv_obj_t *returnPanel=isAileronMixEditor()?objects.aileron_mixes_edit_panel:objects.free_mixes_edit_panel;
                if(returnPanel){
                    lv_obj_update_layout(returnPanel);
                    lv_obj_scroll_to_y(
                        returnPanel,
                        returnScrollY,
                        LV_ANIM_OFF
                    );
                }
            }

            return;
        }

        isChannelsScreenActive=true;
        load_channels_overview_ui();
    }
    if (current_active_screen == SCREEN_ID_SCR_GENERAL_SETTINGS)
        saveGeneralSettings();
    ScreensEnum parent = getParentScreen(current_active_screen);
    openScreen(parent);
}
void action_battery_action(lv_event_t *e){
    update_topbar_icon(50,-30); 
}

// Actions
void action_action_goto_home(lv_event_t *e){
    openScreen(SCREEN_ID_SCR_MAIN);
}
void action_action_goto_model(lv_event_t *e){
    openScreen(SCREEN_ID_SCR_MODEL);
}
void action_action_goto_telemetry(lv_event_t *e){
    openScreen(SCREEN_ID_SCR_TELEMETRY);
}
void action_action_goto_settings(lv_event_t *e){
    openScreen(SCREEN_ID_SCR_SETTINGS);
}
static void openChannelEditorDirect(uint8_t channel,bool returnToMix=false){
    ModelConfig &model=modelManager.getActiveModel();

    if(channel>=model.channelCount||channel>=MAX_MODEL_CHANNELS)
        return;

    channelEditorReturnToMix=returnToMix;
    channelEditorReturnMixIndex=returnToMix?editingMixIndex:-1;
    channelEditorReturnMixScrollY=0;

    if(returnToMix){
        lv_obj_t *panel=isAileronMixEditor()?objects.aileron_mixes_edit_panel:objects.free_mixes_edit_panel;
        if(panel) channelEditorReturnMixScrollY=lv_obj_get_scroll_y(panel);
    }

    editedChannel=channel;

    openScreen(SCREEN_ID_SCR_CH_SETTINGS);

    isChannelsScreenActive=false;
    isChSettingsScreenActive=true;

    load_channel_settings_to_ui(editedChannel);
    setup_all_inputs();
    update_dropdown_step_options();
}

void action_action_edit_channel(lv_event_t *e){
    if(!e) return;

    uintptr_t user_data_val=(uintptr_t)lv_event_get_user_data(e);

    // Existing Channels overview currently supplies CH1..CH8.
    if(user_data_val>=8) return;

    openChannelEditorDirect((uint8_t)user_data_val,false);
}
void action_channels_screen_open(lv_event_t *e){

}
void action_action_start_calib(lv_event_t *e){
    start_stick_calibration();
    lv_obj_remove_state(objects.done_calib, LV_STATE_DISABLED);
    lv_obj_add_state(objects.start_calib, LV_STATE_DISABLED);
}
void action_action_calib_done(lv_event_t * e){
    if (calibrationManager.isCalibrating()){
        if (calibrationManager.finish()){
            stop_stick_calibration();
            load_all_calibration_limits();
        }
    }
    lv_obj_add_state(objects.done_calib, LV_STATE_DISABLED);
    lv_obj_remove_state(objects.start_calib, LV_STATE_DISABLED);
}
void action_action_calib_cancel(lv_event_t * e){
    stop_stick_calibration();
    lv_obj_add_state(objects.done_calib, LV_STATE_DISABLED);
    lv_obj_remove_state(objects.start_calib, LV_STATE_DISABLED);
}
void action_action_calib_back(lv_event_t * e){
    stop_stick_calibration();
    openScreen(SCREEN_ID_SCR_SETTINGS);
}
void action_action_calib_settings(lv_event_t * e){
    stop_stick_calibration();
}
void showSavedCalibrationAsync(void *param){
    showSavedCalibration();
}
void action_action_goto_calib(lv_event_t * e){
    openScreen(SCREEN_ID_SCR_CALIBRATION_STICKS);
    lv_async_call(showSavedCalibrationAsync, nullptr);
}
void action_goto_sd_card_screen(lv_event_t * e){
    openScreen(SCREEN_ID_SCR_SD);    
    lv_obj_add_state(objects.settings_sd, LV_STATE_CHECKED);
    sd_fm_init();
}
void action_open_scr_vars(lv_event_t *e){
    (void)e;
    openScreen(SCREEN_ID_SCR_VARS);
}

void action_open_scr_hw(lv_event_t *e){
    (void)e;
    openScreen(SCREEN_ID_SCR_HW);
}

void action_open_scr_channel(lv_event_t *e){
    openScreen(SCREEN_ID_SCR_CHANNELS);
    isChSettingsScreenActive = false;
    isChannelsScreenActive = true;
    load_channels_overview_ui();    
}
void action_save_channel_edit(lv_event_t *e){
    if(editedChannel>=MAX_MODEL_CHANNELS) return;
    ModelConfig &model=modelManager.getActiveModel();
    OutputConfig &output=model.outputs[editedChannel];
    if(objects.ch_name_editor!=NULL){
        const char *txt=lv_textarea_get_text(objects.ch_name_editor);
        if(txt!=NULL){
            strncpy(output.name,txt,sizeof(output.name)-1);
            output.name[sizeof(output.name)-1]='\0';
        }
    }
    if(objects.ch_revers!=NULL)
        output.reversed=lv_obj_has_state(objects.ch_revers,LV_STATE_CHECKED);
    sync_ch_data_to_config(editedChannel);
    if(!modelManager.requestSaveActiveModel())
        Serial.printf("ERROR: CH%d model save failed\n",editedChannel+1);
    else
        Serial.printf("CH%d model saved\n",editedChannel+1);
}
void action_goback(lv_event_t *e){
    goBack();
}
void action_show_changed_panel(lv_event_t * e) {
    if (objects.ch_edit_changer_panel == NULL || e == NULL) return;
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // 1. Получаем индекс текущего TextArea из User Data EEZ Studio
        uintptr_t field_idx=(uintptr_t)lv_event_get_user_data(e);
        active_field_is_pwm=false;
        switch(field_idx){
            case 0:active_field=FIELD_MIN;break;
            case 1:active_field=FIELD_MAX;break;
            case 2:active_field=FIELD_CENTER;break;
            case 3:active_field_is_pwm=true;break;
            case 4:active_field=FIELD_EXPO_CURVE;break;
            case 5:active_field=FIELD_SLOW_UP;break;
            case 6:active_field=FIELD_SLOW_DOWN;break;
            default:return;
        }
        update_dropdown_step_options();
        // 2. Показываем панель
        lv_obj_remove_flag(objects.ch_edit_changer_panel, LV_OBJ_FLAG_HIDDEN);
        // 3. Подстраиваем слайдер под новое активное поле
        update_active_field_ui();
    } 
    else if (code == LV_EVENT_DEFOCUSED) {
        lv_obj_add_flag(objects.ch_edit_changer_panel, LV_OBJ_FLAG_HIDDEN);
    }
}
void action_show_curve_changed_panel(lv_event_t *e){
    if(objects.curve_changer_panel==NULL||e==NULL) return;
    lv_event_code_t code=lv_event_get_code(e);
    if(code==LV_EVENT_CLICKED){
        lv_obj_t *target=(lv_obj_t *)lv_event_get_current_target(e);
        if(target&&lv_obj_has_state(target,LV_STATE_DISABLED)) return;
        uintptr_t field_idx=(uintptr_t)lv_event_get_user_data(e);
        if(field_idx<=4){
            switch(field_idx){
                case 0:activeCurveEditField=CURVE_EDIT_EXPO_WEIGHT;break;
                case 1:activeCurveEditField=CURVE_EDIT_EXPO_OFFSET;break;
                case 2:activeCurveEditField=CURVE_EDIT_EXPO_EXPO;break;
                case 3:activeCurveEditField=CURVE_EDIT_FUNCTION_OFFSET;break;
                case 4:activeCurveEditField=CURVE_EDIT_CUSTOM_OFFSET;break;
                default:return;
            }
            activeCurveEditPoint=0;
        }else if(field_idx>=100&&field_idx<=120){
            activeCurveEditField=CURVE_EDIT_POINT_X;
            activeCurveEditPoint=(uint8_t)(field_idx-100);
            if(activeCurveEditPoint>=currentCurvePointCount) return;
        }else if(field_idx>=200&&field_idx<=220){
            activeCurveEditField=CURVE_EDIT_POINT_Y;
            activeCurveEditPoint=(uint8_t)(field_idx-200);
            if(activeCurveEditPoint>=currentCurvePointCount) return;
        }else return;
        updateCurveChangerUi();
        lv_obj_remove_flag(objects.curve_changer_panel,LV_OBJ_FLAG_HIDDEN);
    }else if(code==LV_EVENT_DEFOCUSED){
        lv_obj_add_flag(objects.curve_changer_panel,LV_OBJ_FLAG_HIDDEN);
    }
}
void action_set_curve_smooth(lv_event_t *e){
    if(selectedCurveIndex<0||selectedCurveIndex>=MAX_MODEL_CURVES) return;
    ModelConfig &model=modelManager.getActiveModel();
    CurveConfig &curve=model.curves[selectedCurveIndex];
    curve.smooth=lv_obj_has_state(objects.custom_curv_smooth_sw,LV_STATE_CHECKED);
    drawCurveGraph((uint8_t)selectedCurveIndex);
}
void action_edit_output_curve(lv_event_t *e){
    if(!e) return;

    lv_obj_t *target=(lv_obj_t *)lv_event_get_target(e);
    int8_t curveIndex=OUTPUT_CURVE_NONE;

    if(target==objects.ch_expo_curve_edit_panel){
        // Channel Editor.
        if(editedChannel>=MAX_MODEL_CHANNELS) return;

        curvePickerForMixAction=false;

        ModelConfig &model=modelManager.getActiveModel();
        curveIndex=getOutputCurveSelection(model.outputs[editedChannel]);

        curveEditorReturnToMixAction=false;
        curveEditorReturnToChannel=true;
    }else{
        // Free Mix Action Editor: userdata 0..5 selects the Curve row.
        uint8_t slot=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
        if(slot>=6) return;

        activeMixCurveSlot=slot;
        curvePickerForMixAction=true;
        curveIndex=isAileronMixEditor()?aileronEditorState.curveSelection[slot]:mixActionCurveSelection[slot];

        curveEditorReturnToAileron=isAileronMixEditor();
        curveEditorReturnToMixAction=!curveEditorReturnToAileron;
        curveEditorReturnToChannel=false;
    }

    if(curveIndex<0||curveIndex>=MAX_MODEL_CURVES) return;

    ModelConfig &model=modelManager.getActiveModel();
    if(!model.curves[curveIndex].enabled) return;

    curveEditorIsNew=false;
    selectedCurveIndex=curveIndex;
    editingCurveIndex=curveIndex;

    if(objects.ch_edit_curve_list)
        lv_obj_add_flag(objects.ch_edit_curve_list,LV_OBJ_FLAG_HIDDEN);

    openScreen(SCREEN_ID_SCR_CURVE);
    loadCurveToEditor((uint8_t)curveIndex);

    if(objects.curve_main_panel)
        lv_obj_add_flag(objects.curve_main_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.curve_edit_panel)
        lv_obj_remove_flag(objects.curve_edit_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.curve_diag&&objects.curve_diag_edit_panel)
        lv_obj_set_parent(objects.curve_diag,objects.curve_diag_edit_panel);

    drawCurveGraph((uint8_t)curveIndex);
}

void action_add_edit_curve_balance(lv_event_t *e){
    if(editedChannel>=MAX_MODEL_CHANNELS) return;
    uint8_t action=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(action==0){
        openChannelBalanceEditor(editedChannel);
        if(objects.topbar_panel!=NULL)
            lv_obj_add_flag(objects.topbar_panel,LV_OBJ_FLAG_HIDDEN);
        if(objects.channel_editing_widget!=NULL)
            lv_obj_add_flag(objects.channel_editing_widget,LV_OBJ_FLAG_HIDDEN);
        updateChannelBalanceUi(editedChannel);
        return;
    }
    if(action==1){
        clearChannelBalance(editedChannel);
        return;
    }
}
// Close the shared Edit/Delete popup before opening a confirmation dialog.
// Keep the selected item and popup target intact for Yes/No callbacks.
static void dismissSharedEditDeletePopup(){
    if(objects.curve_popup) lv_obj_add_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN);
    hidePopupDismissOverlay();
}

// Global confirmation dialog API, shared by Mixes, Curves, Vars and Channels.
// Implemented below using the same themed overlay as channel assignments.
static void showSystemConfirmation(const char *message,const char *primary,lv_event_cb_t primaryCb,const char *secondary,lv_event_cb_t secondaryCb);
static void closeSystemConfirmation();

void action_delete_menu_clicked(lv_event_t * e){
    if(sharedEditDeletePopupTarget==SharedEditDeletePopupTarget::ACTION){
        if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
        if(selectedMixActionIndex<0||selectedMixActionIndex>=MAX_MIX_ACTIONS) return;

        ModelConfig &model=modelManager.getActiveModel();
        const MixLine &mix=model.mixes[(uint8_t)editingMixIndex];
        uint8_t actionIndex=(uint8_t)selectedMixActionIndex;

        if(actionIndex>=mix.actionCount) return;
        if(!mix.actions[actionIndex].enabled) return;

        char message[120];
        snprintf(message,sizeof(message),"Delete %s action?",getMixActionTypeName(mix.actions[actionIndex].type));
        dismissSharedEditDeletePopup();
        showSystemConfirmation(message,"Delete",mixActionDeleteConfirmCb,"Cancel",mixActionDeleteCancelCb);
        return;
    }

    if(sharedEditDeletePopupTarget==SharedEditDeletePopupTarget::MIX){
        if(selectedMixIndex<0||selectedMixIndex>=MAX_MIX_LINES) return;

        ModelConfig &model=modelManager.getActiveModel();
        MixLine &mix=model.mixes[(uint8_t)selectedMixIndex];
        if(!mix.enabled) return;

        char message[120];
        snprintf(message,sizeof(message),"Delete \"%s\"?",mix.name[0]!='\0'?mix.name:"Free mix");
        dismissSharedEditDeletePopup();
        showSystemConfirmation(message,"Delete",mixDeleteConfirmCb,"Cancel",mixDeleteCancelCb);
        return;
    }

    if(selectedCurveIndex<0||selectedCurveIndex>=MAX_MODEL_CURVES) return;
    if(sharedEditDeletePopupTarget==SharedEditDeletePopupTarget::VAR) return;

    ModelConfig &model=modelManager.getActiveModel();
    CurveConfig &curve=model.curves[selectedCurveIndex];
    if(!curve.enabled) return;
    const uint16_t usageCount=getCurveUsageCount((uint8_t)selectedCurveIndex);
    char message[320];
    if(usageCount){
        char owners[220];
        formatCurveUsageOwners((uint8_t)selectedCurveIndex,owners,sizeof(owners));
        snprintf(message,sizeof(message),"This curve is used by:\n%s\nAre you sure you want to delete this curve?",owners);
    }else{
        snprintf(message,sizeof(message),"Delete curve \"%s\"?",curve.name[0]!='\0'?curve.name:"Curve");
    }
    dismissSharedEditDeletePopup();
    showSystemConfirmation(message,"Yes",curveDeleteConfirmCb,"No",curveDeleteCancelCb);
}

// -----------------------------------------------------------------------------
// Free Mix Action numeric value editor
// mix_changer_panel is an instance of the common EEZ User Widget changer_panel.\n// The same widget template is used by Channel editor, Curve editor and Mix editor.
// -----------------------------------------------------------------------------

static uint8_t activeMixActionValueId=0xFF;

static int16_t mixActionWeightValues[10]={100,100,100,100,100,100,100,100,100,100};
static int16_t mixActionCurveExpoValues[6]={0,0,0,0,0,0};
static int16_t mixActionDifferentialValue=0;
static int16_t mixActionOffsetActiveValue=0;
static int16_t mixActionOffsetInactiveValue=0;
// Slow is stored in tenths of a second: 0..250 == 0.0..25.0 s.
static int16_t mixActionSlowUpValue=0;
static int16_t mixActionSlowDownValue=0;
static int16_t mixOutputCountValue=1;

enum MixActionStepGroup:uint8_t{
    MIX_STEP_WEIGHT_OFFSET=0,
    MIX_STEP_PERCENT_100=1,
    MIX_STEP_SLOW=2,
    MIX_STEP_COUNT=3
};

static uint8_t mixActionStepSelected[4]={0,0,0,0};

static lv_obj_t *getAileronValueLabel(uint8_t id){
    lv_obj_t *weight[]={objects.aileron_mix_otherwise_weight_lbl,objects.aileron_mix_weight_weight_lbl1,objects.aileron_mix_weight_weight_lbl2,objects.aileron_mix_weight_weight_lbl3,objects.aileron_mix_weight_weight_lbl4,objects.aileron_mix_weight_weight_lbl5,objects.aileron_mix_weight_weight_lbl6,objects.aileron_mix_weight_weight_lbl7,objects.aileron_mix_weight_weight_lbl8,objects.aileron_mix_weight_weight_lbl9};
    lv_obj_t *curves[]={objects.aileron_mix_curve_otherwise_weight,objects.aileron_mix_curve_weight1,objects.aileron_mix_curve_weight2,objects.aileron_mix_curve_weight3,objects.aileron_mix_curve_weight4,objects.aileron_mix_curve_weight5};
    if(id<10) return weight[id];
    if(id>=10&&id<=15) return curves[id-10];
    if(id==20) return objects.aileron_mix_differential_weight;
    if(id==50) return objects.aileron_mix_output_count_lbl;
    return nullptr;
}

static lv_obj_t *getMixActionValueLabel(uint8_t id){
    if(isAileronMixEditor()) return getAileronValueLabel(id);
    switch(id){
        case 0:return objects.weight_otherwise_weight;
        case 1:return objects.weight_weight1;
        case 2:return objects.weight_weight2;
        case 3:return objects.weight_weight3;
        case 4:return objects.weight_weight4;
        case 5:return objects.weight_weight5;
        case 6:return objects.weight_weight6;
        case 7:return objects.weight_weight7;
        case 8:return objects.weight_weight8;
        case 9:return objects.weight_weight9;

        case 10:return objects.mix_curve_otherwise_weight;
        case 11:return objects.mix_curve_weight1;
        case 12:return objects.mix_curve_weight2;
        case 13:return objects.mix_curve_weight3;
        case 14:return objects.mix_curve_weight4;
        case 15:return objects.mix_curve_weight5;

        case 20:return objects.differential_weight;
        case 30:return objects.action_owa;
        case 31:return objects.action_owi;
        case 40:return objects.action_slowup_time;
        case 41:return objects.action_slowdown_time;
        case 50:return objects.mix_output_count_lbl;

        default:return nullptr;
    }
}

static int16_t *getMixActionValuePtr(uint8_t id){
    if(isAileronMixEditor()){
        if(id<10) return &aileronEditorState.weights[id];
        if(id>=10&&id<=15) return &aileronEditorState.curveExpo[id-10];
        if(id==20) return &aileronEditorState.differential;
        if(id==50) return &mixOutputCountValue;
        return nullptr;
    }
    if(id<=9) return &mixActionWeightValues[id];
    if(id>=10&&id<=15) return &mixActionCurveExpoValues[id-10];

    switch(id){
        case 20:return &mixActionDifferentialValue;
        case 30:return &mixActionOffsetActiveValue;
        case 31:return &mixActionOffsetInactiveValue;
        case 40:return &mixActionSlowUpValue;
        case 41:return &mixActionSlowDownValue;
        case 50:return &mixOutputCountValue;
        default:return nullptr;
    }
}

static void getMixActionValueBounds(uint8_t id,int16_t &minValue,int16_t &maxValue){
    if(id<=9){
        minValue=-500;
        maxValue=500;
        return;
    }

    if((id>=10&&id<=15)||id==20){
        minValue=-100;
        maxValue=100;
        return;
    }

    if(id==30||id==31){
        minValue=-200;
        maxValue=200;
        return;
    }

    if(id==40||id==41){
        minValue=0;
        maxValue=250;
        return;
    }

    if(id==50){
        minValue=1;
        maxValue=isAileronMixEditor()?4:MAX_MIX_OUTPUTS;
        return;
    }

    minValue=0;
    maxValue=0;
}

static MixActionStepGroup getMixActionStepGroup(uint8_t id){
    if(id<=9||id==30||id==31) return MIX_STEP_WEIGHT_OFFSET;
    if((id>=10&&id<=15)||id==20) return MIX_STEP_PERCENT_100;
    if(id==50) return MIX_STEP_COUNT;
    return MIX_STEP_SLOW;
}

static const char *getMixActionStepOptions(MixActionStepGroup group){
    switch(group){
        case MIX_STEP_WEIGHT_OFFSET:return "1%\n10%\n100%";
        case MIX_STEP_PERCENT_100:return "1%\n10%";
        case MIX_STEP_SLOW:return "0.1s\n1.0s";
        case MIX_STEP_COUNT:return "1";
        default:return "1%";
    }
}

static uint8_t getMixActionStepMaxIndex(MixActionStepGroup group){
    if(group==MIX_STEP_WEIGHT_OFFSET) return 2;
    if(group==MIX_STEP_COUNT) return 0;
    return 1;
}

static int16_t getMixActionStepValue(MixActionStepGroup group,uint8_t selected){
    switch(group){
        case MIX_STEP_WEIGHT_OFFSET:{
            static const int16_t steps[3]={1,10,100};
            if(selected>2) selected=2;
            return steps[selected];
        }

        case MIX_STEP_PERCENT_100:{
            static const int16_t steps[2]={1,10};
            if(selected>1) selected=1;
            return steps[selected];
        }

        case MIX_STEP_SLOW:{
            // Internal unit is 0.1 s.
            static const int16_t steps[2]={1,10};
            if(selected>1) selected=1;
            return steps[selected];
        }

        case MIX_STEP_COUNT:
            return 1;

        default:return 1;
    }
}

static void updateMixActionValueLabel(uint8_t id){
    lv_obj_t *label=getMixActionValueLabel(id);
    int16_t *value=getMixActionValuePtr(id);
    if(!label||!value) return;

    if(id==40||id==41){
        int16_t whole=*value/10;
        int16_t tenth=abs(*value%10);
        lv_label_set_text_fmt(label,"%d.%ds",(int)whole,(int)tenth);
        return;
    }

    if(id==50){
        lv_label_set_text_fmt(label,"%d",(int)*value);
        return;
    }

    lv_label_set_text_fmt(label,"%d%%",(int)*value);
}

static void updateMixActionChangerStepButtons(){
    if(activeMixActionValueId==0xFF) return;

    MixActionStepGroup group=getMixActionStepGroup(activeMixActionValueId);
    uint8_t maxIndex=getMixActionStepMaxIndex(group);
    uint8_t selected=mixActionStepSelected[group];
    if(selected>maxIndex) selected=maxIndex;
    mixActionStepSelected[group]=selected;

    if(objects.mix_changer_panel__dropdown_step){
        const char *options=getMixActionStepOptions(group);
        const char *current=lv_dropdown_get_options(objects.mix_changer_panel__dropdown_step);

        if(!current||strcmp(current,options)!=0)
            lv_dropdown_set_options(objects.mix_changer_panel__dropdown_step,options);

        lv_dropdown_set_selected(objects.mix_changer_panel__dropdown_step,selected);
    }

    if(objects.mix_changer_panel__reduce_step){
        if(selected==0) lv_obj_add_state(objects.mix_changer_panel__reduce_step,LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.mix_changer_panel__reduce_step,LV_STATE_DISABLED);
    }

    if(objects.mix_changer_panel__increase_step){
        if(selected>=maxIndex) lv_obj_add_state(objects.mix_changer_panel__increase_step,LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.mix_changer_panel__increase_step,LV_STATE_DISABLED);
    }
}

static void updateMixActionChangerLimitButtons(){
    if(activeMixActionValueId==0xFF) return;

    int16_t *value=getMixActionValuePtr(activeMixActionValueId);
    if(!value) return;

    int16_t minValue,maxValue;
    getMixActionValueBounds(activeMixActionValueId,minValue,maxValue);

    if(objects.mix_changer_panel__step_down){
        if(*value<=minValue) lv_obj_add_state(objects.mix_changer_panel__step_down,LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.mix_changer_panel__step_down,LV_STATE_DISABLED);
    }

    if(objects.mix_changer_panel__step_up){
        if(*value>=maxValue) lv_obj_add_state(objects.mix_changer_panel__step_up,LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.mix_changer_panel__step_up,LV_STATE_DISABLED);
    }
}

static void updateMixActionChangerUi(){
    if(activeMixActionValueId==0xFF) return;

    int16_t *value=getMixActionValuePtr(activeMixActionValueId);
    if(!value) return;

    int16_t minValue,maxValue;
    getMixActionValueBounds(activeMixActionValueId,minValue,maxValue);

    if(*value<minValue) *value=minValue;
    if(*value>maxValue) *value=maxValue;

    if(objects.mix_changer_panel__mcmsusd_slider){
        lv_slider_set_range(objects.mix_changer_panel__mcmsusd_slider,minValue,maxValue);
        lv_slider_set_value(objects.mix_changer_panel__mcmsusd_slider,*value,LV_ANIM_OFF);
    }

    updateMixActionValueLabel(activeMixActionValueId);
    updateMixActionChangerStepButtons();
    updateMixActionChangerLimitButtons();
}

static void hideMixActionChanger(){
    activeMixActionValueId=0xFF;

    if(objects.mix_changer_panel)
        lv_obj_add_flag(objects.mix_changer_panel,LV_OBJ_FLAG_HIDDEN);
}

static void resetMixActionValues(){
    for(uint8_t i=0;i<MAX_MIX_ACTION_VARIANTS;i++){
        mixActionVariantConditions[i]=ConditionConfig{};
        mixActionVariantConditions[i].type=ConditionType::ALWAYS;
    }

    for(uint8_t i=0;i<10;i++) mixActionWeightValues[i]=100;
    for(uint8_t i=0;i<6;i++){
        mixActionCurveExpoValues[i]=0;
        mixActionCurveSelection[i]=OUTPUT_CURVE_NONE;
        updateMixCurveSelectionUi(i);
    }

    mixActionDifferentialValue=0;
    mixActionOffsetActiveValue=0;
    mixActionOffsetInactiveValue=0;
    mixActionSlowUpValue=0;
    mixActionSlowDownValue=0;

    for(uint8_t i=0;i<4;i++) mixActionStepSelected[i]=0;

    for(uint8_t id=0;id<=9;id++) updateMixActionValueLabel(id);
    for(uint8_t id=10;id<=15;id++) updateMixActionValueLabel(id);
    updateMixActionValueLabel(20);
    updateMixActionValueLabel(30);
    updateMixActionValueLabel(31);
    updateMixActionValueLabel(40);
    updateMixActionValueLabel(41);

    hideMixActionChanger();
}

static void mixActionChangeStep(lv_event_t *e){
    if(!e||activeMixActionValueId==0xFF) return;

    uintptr_t direction=(uintptr_t)lv_event_get_user_data(e);
    MixActionStepGroup group=getMixActionStepGroup(activeMixActionValueId);
    uint8_t maxIndex=getMixActionStepMaxIndex(group);
    uint8_t selected=mixActionStepSelected[group];

    if(direction==0){
        if(selected>0) selected--;
    }else if(direction==1){
        if(selected<maxIndex) selected++;
    }else{
        return;
    }

    mixActionStepSelected[group]=selected;
    updateMixActionChangerStepButtons();
}

// Aileron supports only 1, 2 or 4 output channels in ETHOS.
static int16_t normalizeAileronOutputCount(int16_t count,int8_t direction=0){
    if(count<=1) return 1;
    if(count==2) return 2;
    if(count>=4) return 4;
    return direction<0?2:4;
}

static void mixActionDoStep(lv_event_t *e){
    if(!e||activeMixActionValueId==0xFF) return;

    int16_t *value=getMixActionValuePtr(activeMixActionValueId);
    if(!value) return;

    uintptr_t command=(uintptr_t)lv_event_get_user_data(e);

    int16_t minValue,maxValue;
    getMixActionValueBounds(activeMixActionValueId,minValue,maxValue);

    if(command==2){
        if(!objects.mix_changer_panel__mcmsusd_slider) return;
        *value=(int16_t)lv_slider_get_value(objects.mix_changer_panel__mcmsusd_slider);
    }else{
        MixActionStepGroup group=getMixActionStepGroup(activeMixActionValueId);
        uint8_t selected=mixActionStepSelected[group];
        int16_t step=getMixActionStepValue(group,selected);

        if(command==0) *value-=step;
        else if(command==1) *value+=step;
        else return;
    }

    if(*value<minValue) *value=minValue;
    if(*value>maxValue) *value=maxValue;
    if(isAileronMixEditor()&&activeMixActionValueId==50) *value=normalizeAileronOutputCount(*value,command==0?-1:1);

    updateMixActionValueLabel(activeMixActionValueId);
    if(isAileronMixEditor()&&activeMixActionValueId!=50) saveAileronEditorState();

    if(activeMixActionValueId==50)
        applyMixOutputCountFromEditor();

    if(objects.mix_changer_panel__mcmsusd_slider)
        lv_slider_set_value(objects.mix_changer_panel__mcmsusd_slider,*value,LV_ANIM_OFF);

    updateMixActionChangerLimitButtons();
}

void action_mix_action_value_edit(lv_event_t *e){
    if(!e) return;
    mixChangerOpenTarget=(lv_obj_t *)lv_event_get_current_target(e);

    uint8_t id=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(!getMixActionValuePtr(id)) return;

    activeMixActionValueId=id;

    if(objects.mix_changer_panel__button_panel)
        lv_obj_remove_flag(objects.mix_changer_panel__button_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.mix_changer_panel__slider_panel)
        lv_obj_add_flag(objects.mix_changer_panel__slider_panel,LV_OBJ_FLAG_HIDDEN);

    updateMixActionChangerUi();

    bindChangerDismissRecursive(isAileronMixEditor()?objects.aileron_mixes_edit_panel:objects.free_mixes_edit_panel,objects.mix_changer_panel,dismissMixChangerOnPress);
    if(objects.mix_changer_panel)
        lv_obj_remove_flag(objects.mix_changer_panel,LV_OBJ_FLAG_HIDDEN);
}

void action_swipe_changer(lv_event_t *e)
{
    if(e==NULL) return;
    if(eventFromChanger(e,objects.hw_changer_panel)){
        toggleChangerPanels(objects.hw_changer_panel__button_panel,objects.hw_changer_panel__slider_panel);
        return;
    }
    if(eventFromChanger(e,objects.mix_changer_panel)){
        toggleChangerPanels(objects.mix_changer_panel__button_panel,objects.mix_changer_panel__slider_panel);
        return;
    }
    if(eventFromChanger(e,objects.curve_changer_panel)){
        toggleChangerPanels(objects.curve_changer_panel__button_panel,objects.curve_changer_panel__slider_panel);
        return;
    }
    if(eventFromChanger(e,objects.vars_changer_panel)){
        toggleChangerPanels(objects.vars_changer_panel__button_panel,objects.vars_changer_panel__slider_panel);
        return;
    }
    toggleChangerPanels(objects.ch_edit_changer_panel__button_panel,objects.ch_edit_changer_panel__slider_panel);
}
void action_change_step(lv_event_t *e){
    if(e==NULL) return;
    if(eventFromChanger(e,objects.hw_changer_panel)) return;
    if(eventFromChanger(e,objects.mix_changer_panel)){
        mixActionChangeStep(e);
        return;
    }
    if(eventFromChanger(e,objects.curve_changer_panel)){
        curveChangeStep(e);
        return;
    }
    if(eventFromChanger(e,objects.vars_changer_panel)){
        varsNumericChangeStep(e);
        return;
    }
    if(balancePointEditActive&&objects.curve_balance_panel!=NULL&&!lv_obj_has_flag(objects.curve_balance_panel,LV_OBJ_FLAG_HIDDEN)){
        balancePointChangeStep(e);
        return;
    }
    if(objects.ch_edit_changer_panel__dropdown_step==NULL) return;
    const StepConfig* cfg=get_current_step_config();
    uintptr_t direction=(uintptr_t)lv_event_get_user_data(e);
    uint32_t selected=lv_dropdown_get_selected(objects.ch_edit_changer_panel__dropdown_step);
    if(direction==0){
        if(selected>0) selected--;
    }else if(direction==1){
        if(selected<cfg->max_idx) selected++;
    }
    lv_dropdown_set_selected(objects.ch_edit_changer_panel__dropdown_step,selected);
    if(objects.ch_edit_changer_panel__reduce_step!=NULL){
        if(selected==0) lv_obj_add_state(objects.ch_edit_changer_panel__reduce_step,LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.ch_edit_changer_panel__reduce_step,LV_STATE_DISABLED);
    }
    if(objects.ch_edit_changer_panel__increase_step!=NULL){
        if(selected>=cfg->max_idx) lv_obj_add_state(objects.ch_edit_changer_panel__increase_step,LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.ch_edit_changer_panel__increase_step,LV_STATE_DISABLED);
    }
    StepGroup group=get_current_step_group();
    last_selected_indices[group]=selected;
    update_dropdown_step_options();
}
void action_do_step(lv_event_t *e){
    if(e==NULL) return;
    if(eventFromChanger(e,objects.hw_changer_panel)) return;
    if(eventFromChanger(e,objects.mix_changer_panel)){
        mixActionDoStep(e);
        return;
    }
    if(eventFromChanger(e,objects.curve_changer_panel)){
        curveDoStep(e);
        return;
    }
    if(eventFromChanger(e,objects.vars_changer_panel)){
        varsNumericDoStep(e);
        return;
    }
    if(balancePointEditActive&&objects.curve_balance_panel!=NULL&&!lv_obj_has_flag(objects.curve_balance_panel,LV_OBJ_FLAG_HIDDEN)){
        balancePointDoStep(e);
        return;
    }
    uintptr_t user_cmd=(uintptr_t)lv_event_get_user_data(e);
    float step_val=1.0f;
    if(objects.ch_edit_changer_panel__dropdown_step!=NULL){
        const StepConfig* cfg=get_current_step_config();
        uint32_t sel=lv_dropdown_get_selected(objects.ch_edit_changer_panel__dropdown_step);
        if(sel>cfg->max_idx) sel=cfg->max_idx;
        step_val=cfg->steps[sel];
    }
    float* val_ptr=get_active_field_ptr();
    if(val_ptr==NULL) return;
    float min_b,max_b;
    get_current_field_bounds(min_b,max_b);
    if(user_cmd==0){
        *val_ptr-=step_val;
    }else if(user_cmd==1){
        *val_ptr+=step_val;
    }else if(user_cmd==2&&objects.ch_edit_changer_panel__mcmsusd_slider!=NULL){
        int32_t slider_val=lv_slider_get_value(objects.ch_edit_changer_panel__mcmsusd_slider);
        if(active_field_is_pwm||active_field==FIELD_EXPO_CURVE) *val_ptr=(float)slider_val;
        else *val_ptr=(float)slider_val/10.0f;
    }
    if(*val_ptr<min_b) *val_ptr=min_b;
    if(*val_ptr>max_b) *val_ptr=max_b;
    if(active_field==FIELD_MIN||active_field==FIELD_MAX) clamp_center_to_bounds();
    if(objects.ch_edit_changer_panel__mcmsusd_slider!=NULL&&user_cmd!=2){
        if(active_field_is_pwm||active_field==FIELD_EXPO_CURVE) lv_slider_set_value(objects.ch_edit_changer_panel__mcmsusd_slider,(int32_t)(*val_ptr),LV_ANIM_OFF);
        else lv_slider_set_value(objects.ch_edit_changer_panel__mcmsusd_slider,(int32_t)((*val_ptr)*10.0f),LV_ANIM_OFF);
    }
    update_active_field_ui();
    sync_ch_data_to_config(editedChannel);
}
void action_open_scr_trimm(lv_event_t *e){

}
void action_open_scr_general_settigs(lv_event_t *e){
    openScreen(SCREEN_ID_SCR_GENERAL_SETTINGS);
}
void action_toggle_display_menu(lv_event_t *e){
if (e == NULL) return;
    // Извлекаем user_data из события
    uintptr_t user_data = (uintptr_t)lv_event_get_user_data(e);
    if (user_data == 0) {
        // Проверяем DisplaySubmenuContainer и инвертируем флаг hidden
        if (lv_obj_has_flag(objects.display_submenu_container, LV_OBJ_FLAG_HIDDEN)){
            lv_obj_remove_flag(objects.display_submenu_container, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(objects.display_arrow_lbl, "\ue9f1");
        }
        else{
            lv_obj_add_flag(objects.display_submenu_container, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(objects.display_arrow_lbl, "\ue9f2");
        }
    } 
    else if (user_data == 1) {
        // Проверяем AudioSubmenuContainer и инвертируем флаг hidden
        if (lv_obj_has_flag(objects.audio_submenu_container, LV_OBJ_FLAG_HIDDEN)){
            lv_obj_remove_flag(objects.audio_submenu_container, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(objects.audio_arrow_lbl, "\ue9f1");
        }
        else{ 
            lv_obj_add_flag(objects.audio_submenu_container, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(objects.audio_arrow_lbl, "\ue9f2");
        }
    }
    else if (user_data == 3 || user_data == 4 || user_data == 5) {
        lv_obj_t *subpanel = NULL;
        lv_obj_t *arrow = NULL;
        if (user_data == 3) {
            subpanel = objects.hw_pots_subpanel;
            arrow = objects.pots_settings_arrow_lbl;
        } else if (user_data == 4) {
            subpanel = objects.hw_switches_subpanel;
            arrow = objects.switch_settings_arrow_lbl;
        } else {
            subpanel = objects.hw_wifi_subpanel;
            arrow = objects.wifi_settings_arrow_lbl;
        }
        if (subpanel == NULL) return;
        bool opening = lv_obj_has_flag(subpanel, LV_OBJ_FLAG_HIDDEN);
        if (opening) lv_obj_remove_flag(subpanel, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(subpanel, LV_OBJ_FLAG_HIDDEN);
        if (arrow != NULL) lv_label_set_text(arrow, opening ? "\ue9f1" : "\ue9f2");
    }
}

void action_show_colorwheel_pallete(lv_event_t *e){
    lv_obj_remove_flag(objects.color_palette, LV_OBJ_FLAG_HIDDEN);
    set_disabled_recursive(objects.main_list_container, true);
    if(colorPicker == nullptr){
        colorPicker = new ColorPicker(objects.color_palette);
        colorPicker->setApplyCallback([](lv_color_t color, void *user_data){
            lv_obj_t *panel = static_cast<lv_obj_t *>(user_data);
            uint32_t rgb = lv_color_to_u32(color);
            uint8_t r = (rgb >> 16) & 0xFF;
            uint8_t g = (rgb >> 8) & 0xFF;
            uint8_t b = rgb & 0xFF;
            uint32_t themeColor = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            theme_colors[THEME_ID_UI_THEME_DARK][COLOR_ID_MS_TEXT] = themeColor;
            theme_colors[THEME_ID_UI_THEME_LIGHT][COLOR_ID_MS_TEXT] = themeColor;
            change_color_theme(active_theme_index);
            lv_obj_set_style_bg_color(panel, color, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
            char hex[8];
            snprintf(hex, sizeof(hex), "#%02X%02X%02X", r, g, b);
            lv_label_set_text(objects.color_pick_lbl, hex);
            lv_obj_set_style_text_color(objects.color_pick_lbl, color, LV_PART_MAIN);
            lv_obj_add_flag(objects.color_palette, LV_OBJ_FLAG_HIDDEN);
            set_disabled_recursive(objects.main_list_container, false);
        }, objects.color_pick_panel);
    }
    uint32_t themeColor = theme_colors[active_theme_index][COLOR_ID_MS_TEXT];
    uint8_t r = (themeColor >> 16) & 0xFF;
    uint8_t g = (themeColor >> 8) & 0xFF;
    uint8_t b = themeColor & 0xFF;
    lv_color_t currentColor = lv_color_make(r, g, b);
    colorPicker->setColor(currentColor);
    lv_obj_set_style_bg_color(objects.color_pick_panel, currentColor, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(objects.color_pick_panel, LV_OPA_COVER, LV_PART_MAIN);
    char hex[8];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", r, g, b);
    lv_label_set_text(objects.color_pick_lbl, hex);
    lv_obj_set_style_text_color(objects.color_pick_lbl, currentColor, LV_PART_MAIN);
    lv_obj_remove_flag(colorPicker->getRoot(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(colorPicker->getRoot());
    lv_obj_move_foreground(colorPicker->getRoot());
    lv_obj_invalidate(colorPicker->getRoot());
}
void action_theme_change(lv_event_t *e){
    lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
    uint32_t theme = lv_obj_has_state(sw, LV_STATE_CHECKED) ? THEME_ID_UI_THEME_DARK : THEME_ID_UI_THEME_LIGHT;
    change_color_theme(theme);
}
void action_change_brightness(lv_event_t *e){
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    display_set_brightness((uint8_t)lv_slider_get_value(slider));
}


static lv_obj_t *getMixOutputRoot(uint8_t slot){
    if(isAileronMixEditor()){
        lv_obj_t *items[]={objects.aileron_mix_output_widget1,objects.aileron_mix_output_widget2,objects.aileron_mix_output_widget3,objects.aileron_mix_output_widget4};
        return slot<4?items[slot]:nullptr;
    }
    switch(slot){
        case 0:return objects.mix_output_1;
        case 1:return objects.mix_output_2;
        case 2:return objects.mix_output_3;
        case 3:return objects.mix_output_4;
        case 4:return objects.mix_output_5;
        case 5:return objects.mix_output_6;
        case 6:return objects.mix_output_7;
        case 7:return objects.mix_output_8;
        case 8:return objects.mix_output_9;
        case 9:return objects.mix_output_10;
        case 10:return objects.mix_output_11;
        case 11:return objects.mix_output_12;
        case 12:return objects.mix_output_13;
        case 13:return objects.mix_output_14;
        case 14:return objects.mix_output_15;
        case 15:return objects.mix_output_16;
        default:return nullptr;
    }
}

static lv_obj_t *getMixOutputNamePanel(uint8_t slot){
    if(isAileronMixEditor()){
        lv_obj_t *items[]={objects.aileron_mix_output_widget1__ch_monitor_in_mix_output_name_panel,objects.aileron_mix_output_widget2__ch_monitor_in_mix_output_name_panel,objects.aileron_mix_output_widget3__ch_monitor_in_mix_output_name_panel,objects.aileron_mix_output_widget4__ch_monitor_in_mix_output_name_panel};
        return slot<4?items[slot]:nullptr;
    }
    switch(slot){
        case 0:return objects.mix_output_1__ch_monitor_in_mix_output_name_panel;
        case 1:return objects.mix_output_2__ch_monitor_in_mix_output_name_panel;
        case 2:return objects.mix_output_3__ch_monitor_in_mix_output_name_panel;
        case 3:return objects.mix_output_4__ch_monitor_in_mix_output_name_panel;
        case 4:return objects.mix_output_5__ch_monitor_in_mix_output_name_panel;
        case 5:return objects.mix_output_6__ch_monitor_in_mix_output_name_panel;
        case 6:return objects.mix_output_7__ch_monitor_in_mix_output_name_panel;
        case 7:return objects.mix_output_8__ch_monitor_in_mix_output_name_panel;
        case 8:return objects.mix_output_9__ch_monitor_in_mix_output_name_panel;
        case 9:return objects.mix_output_10__ch_monitor_in_mix_output_name_panel;
        case 10:return objects.mix_output_11__ch_monitor_in_mix_output_name_panel;
        case 11:return objects.mix_output_12__ch_monitor_in_mix_output_name_panel;
        case 12:return objects.mix_output_13__ch_monitor_in_mix_output_name_panel;
        case 13:return objects.mix_output_14__ch_monitor_in_mix_output_name_panel;
        case 14:return objects.mix_output_15__ch_monitor_in_mix_output_name_panel;
        case 15:return objects.mix_output_16__ch_monitor_in_mix_output_name_panel;
        default:return nullptr;
    }
}

static lv_obj_t *getMixOutputNameLabel(uint8_t slot){
    if(isAileronMixEditor()){
        lv_obj_t *items[]={objects.aileron_mix_output_widget1__ch_monitor_in_mix_output_name,objects.aileron_mix_output_widget2__ch_monitor_in_mix_output_name,objects.aileron_mix_output_widget3__ch_monitor_in_mix_output_name,objects.aileron_mix_output_widget4__ch_monitor_in_mix_output_name};
        return slot<4?items[slot]:nullptr;
    }
    switch(slot){
        case 0:return objects.mix_output_1__ch_monitor_in_mix_output_name;
        case 1:return objects.mix_output_2__ch_monitor_in_mix_output_name;
        case 2:return objects.mix_output_3__ch_monitor_in_mix_output_name;
        case 3:return objects.mix_output_4__ch_monitor_in_mix_output_name;
        case 4:return objects.mix_output_5__ch_monitor_in_mix_output_name;
        case 5:return objects.mix_output_6__ch_monitor_in_mix_output_name;
        case 6:return objects.mix_output_7__ch_monitor_in_mix_output_name;
        case 7:return objects.mix_output_8__ch_monitor_in_mix_output_name;
        case 8:return objects.mix_output_9__ch_monitor_in_mix_output_name;
        case 9:return objects.mix_output_10__ch_monitor_in_mix_output_name;
        case 10:return objects.mix_output_11__ch_monitor_in_mix_output_name;
        case 11:return objects.mix_output_12__ch_monitor_in_mix_output_name;
        case 12:return objects.mix_output_13__ch_monitor_in_mix_output_name;
        case 13:return objects.mix_output_14__ch_monitor_in_mix_output_name;
        case 14:return objects.mix_output_15__ch_monitor_in_mix_output_name;
        case 15:return objects.mix_output_16__ch_monitor_in_mix_output_name;
        default:return nullptr;
    }
}

static lv_obj_t *getMixOutputMonitor(uint8_t slot){
    if(isAileronMixEditor()){
        lv_obj_t *items[]={objects.aileron_mix_output_widget1__ch_monitor_in_mix,objects.aileron_mix_output_widget2__ch_monitor_in_mix,objects.aileron_mix_output_widget3__ch_monitor_in_mix,objects.aileron_mix_output_widget4__ch_monitor_in_mix};
        return slot<4?items[slot]:nullptr;
    }
    switch(slot){
        case 0:return objects.mix_output_1__ch_monitor_in_mix;
        case 1:return objects.mix_output_2__ch_monitor_in_mix;
        case 2:return objects.mix_output_3__ch_monitor_in_mix;
        case 3:return objects.mix_output_4__ch_monitor_in_mix;
        case 4:return objects.mix_output_5__ch_monitor_in_mix;
        case 5:return objects.mix_output_6__ch_monitor_in_mix;
        case 6:return objects.mix_output_7__ch_monitor_in_mix;
        case 7:return objects.mix_output_8__ch_monitor_in_mix;
        case 8:return objects.mix_output_9__ch_monitor_in_mix;
        case 9:return objects.mix_output_10__ch_monitor_in_mix;
        case 10:return objects.mix_output_11__ch_monitor_in_mix;
        case 11:return objects.mix_output_12__ch_monitor_in_mix;
        case 12:return objects.mix_output_13__ch_monitor_in_mix;
        case 13:return objects.mix_output_14__ch_monitor_in_mix;
        case 14:return objects.mix_output_15__ch_monitor_in_mix;
        case 15:return objects.mix_output_16__ch_monitor_in_mix;
        default:return nullptr;
    }
}

static lv_obj_t *getMixOutputReverseSwitch(uint8_t slot){
    if(isAileronMixEditor()){
        lv_obj_t *items[]={objects.aileron_mix_output_widget1__ch_monitor_in_mix_sw,objects.aileron_mix_output_widget2__ch_monitor_in_mix_sw,objects.aileron_mix_output_widget3__ch_monitor_in_mix_sw,objects.aileron_mix_output_widget4__ch_monitor_in_mix_sw};
        return slot<4?items[slot]:nullptr;
    }
    switch(slot){
        case 0:return objects.mix_output_1__ch_monitor_in_mix_sw;
        case 1:return objects.mix_output_2__ch_monitor_in_mix_sw;
        case 2:return objects.mix_output_3__ch_monitor_in_mix_sw;
        case 3:return objects.mix_output_4__ch_monitor_in_mix_sw;
        case 4:return objects.mix_output_5__ch_monitor_in_mix_sw;
        case 5:return objects.mix_output_6__ch_monitor_in_mix_sw;
        case 6:return objects.mix_output_7__ch_monitor_in_mix_sw;
        case 7:return objects.mix_output_8__ch_monitor_in_mix_sw;
        case 8:return objects.mix_output_9__ch_monitor_in_mix_sw;
        case 9:return objects.mix_output_10__ch_monitor_in_mix_sw;
        case 10:return objects.mix_output_11__ch_monitor_in_mix_sw;
        case 11:return objects.mix_output_12__ch_monitor_in_mix_sw;
        case 12:return objects.mix_output_13__ch_monitor_in_mix_sw;
        case 13:return objects.mix_output_14__ch_monitor_in_mix_sw;
        case 14:return objects.mix_output_15__ch_monitor_in_mix_sw;
        case 15:return objects.mix_output_16__ch_monitor_in_mix_sw;
        default:return nullptr;
    }
}

static lv_obj_t *getMixOutputNumberLabel(uint8_t slot){
    if(isAileronMixEditor()){
        lv_obj_t *items[]={objects.aileron_mix_output_widget1__mix_output_output_number,objects.aileron_mix_output_widget2__mix_output_output_number,objects.aileron_mix_output_widget3__mix_output_output_number,objects.aileron_mix_output_widget4__mix_output_output_number};
        return slot<4?items[slot]:nullptr;
    }
    switch(slot){
        case 0:return objects.mix_output_1__mix_output_output_number;
        case 1:return objects.mix_output_2__mix_output_output_number;
        case 2:return objects.mix_output_3__mix_output_output_number;
        case 3:return objects.mix_output_4__mix_output_output_number;
        case 4:return objects.mix_output_5__mix_output_output_number;
        case 5:return objects.mix_output_6__mix_output_output_number;
        case 6:return objects.mix_output_7__mix_output_output_number;
        case 7:return objects.mix_output_8__mix_output_output_number;
        case 8:return objects.mix_output_9__mix_output_output_number;
        case 9:return objects.mix_output_10__mix_output_output_number;
        case 10:return objects.mix_output_11__mix_output_output_number;
        case 11:return objects.mix_output_12__mix_output_output_number;
        case 12:return objects.mix_output_13__mix_output_output_number;
        case 13:return objects.mix_output_14__mix_output_output_number;
        case 14:return objects.mix_output_15__mix_output_output_number;
        case 15:return objects.mix_output_16__mix_output_output_number;
        default:return nullptr;
    }
}

static lv_obj_t *getMixOutputChannelNumberLabel(uint8_t slot){
    if(isAileronMixEditor()){
        lv_obj_t *items[]={objects.aileron_mix_output_widget1__mix_output_ch_num,objects.aileron_mix_output_widget2__mix_output_ch_num,objects.aileron_mix_output_widget3__mix_output_ch_num,objects.aileron_mix_output_widget4__mix_output_ch_num};
        return slot<4?items[slot]:nullptr;
    }
    switch(slot){
        case 0:return objects.mix_output_1__mix_output_ch_num;
        case 1:return objects.mix_output_2__mix_output_ch_num;
        case 2:return objects.mix_output_3__mix_output_ch_num;
        case 3:return objects.mix_output_4__mix_output_ch_num;
        case 4:return objects.mix_output_5__mix_output_ch_num;
        case 5:return objects.mix_output_6__mix_output_ch_num;
        case 6:return objects.mix_output_7__mix_output_ch_num;
        case 7:return objects.mix_output_8__mix_output_ch_num;
        case 8:return objects.mix_output_9__mix_output_ch_num;
        case 9:return objects.mix_output_10__mix_output_ch_num;
        case 10:return objects.mix_output_11__mix_output_ch_num;
        case 11:return objects.mix_output_12__mix_output_ch_num;
        case 12:return objects.mix_output_13__mix_output_ch_num;
        case 13:return objects.mix_output_14__mix_output_ch_num;
        case 14:return objects.mix_output_15__mix_output_ch_num;
        case 15:return objects.mix_output_16__mix_output_ch_num;
        default:return nullptr;
    }
}

static lv_obj_t *getMixOutputChannelLabel(uint8_t slot){
    if(isAileronMixEditor()){
        lv_obj_t *items[]={objects.aileron_mix_output_widget1__ch_monitor_in_mix_ch_lbl,objects.aileron_mix_output_widget2__ch_monitor_in_mix_ch_lbl,objects.aileron_mix_output_widget3__ch_monitor_in_mix_ch_lbl,objects.aileron_mix_output_widget4__ch_monitor_in_mix_ch_lbl};
        return slot<4?items[slot]:nullptr;
    }
    switch(slot){
        case 0:return objects.mix_output_1__ch_monitor_in_mix_ch_lbl;
        case 1:return objects.mix_output_2__ch_monitor_in_mix_ch_lbl;
        case 2:return objects.mix_output_3__ch_monitor_in_mix_ch_lbl;
        case 3:return objects.mix_output_4__ch_monitor_in_mix_ch_lbl;
        case 4:return objects.mix_output_5__ch_monitor_in_mix_ch_lbl;
        case 5:return objects.mix_output_6__ch_monitor_in_mix_ch_lbl;
        case 6:return objects.mix_output_7__ch_monitor_in_mix_ch_lbl;
        case 7:return objects.mix_output_8__ch_monitor_in_mix_ch_lbl;
        case 8:return objects.mix_output_9__ch_monitor_in_mix_ch_lbl;
        case 9:return objects.mix_output_10__ch_monitor_in_mix_ch_lbl;
        case 10:return objects.mix_output_11__ch_monitor_in_mix_ch_lbl;
        case 11:return objects.mix_output_12__ch_monitor_in_mix_ch_lbl;
        case 12:return objects.mix_output_13__ch_monitor_in_mix_ch_lbl;
        case 13:return objects.mix_output_14__ch_monitor_in_mix_ch_lbl;
        case 14:return objects.mix_output_15__ch_monitor_in_mix_ch_lbl;
        case 15:return objects.mix_output_16__ch_monitor_in_mix_ch_lbl;
        default:return nullptr;
    }
}

static lv_obj_t *getMixOutputMixLabel(uint8_t slot){
    if(isAileronMixEditor()){
        lv_obj_t *items[]={objects.aileron_mix_output_widget1__ch_monitor_in_mix_mx_lbl,objects.aileron_mix_output_widget2__ch_monitor_in_mix_mx_lbl,objects.aileron_mix_output_widget3__ch_monitor_in_mix_mx_lbl,objects.aileron_mix_output_widget4__ch_monitor_in_mix_mx_lbl};
        return slot<4?items[slot]:nullptr;
    }
    switch(slot){
        case 0:return objects.mix_output_1__ch_monitor_in_mix_mx_lbl;
        case 1:return objects.mix_output_2__ch_monitor_in_mix_mx_lbl;
        case 2:return objects.mix_output_3__ch_monitor_in_mix_mx_lbl;
        case 3:return objects.mix_output_4__ch_monitor_in_mix_mx_lbl;
        case 4:return objects.mix_output_5__ch_monitor_in_mix_mx_lbl;
        case 5:return objects.mix_output_6__ch_monitor_in_mix_mx_lbl;
        case 6:return objects.mix_output_7__ch_monitor_in_mix_mx_lbl;
        case 7:return objects.mix_output_8__ch_monitor_in_mix_mx_lbl;
        case 8:return objects.mix_output_9__ch_monitor_in_mix_mx_lbl;
        case 9:return objects.mix_output_10__ch_monitor_in_mix_mx_lbl;
        case 10:return objects.mix_output_11__ch_monitor_in_mix_mx_lbl;
        case 11:return objects.mix_output_12__ch_monitor_in_mix_mx_lbl;
        case 12:return objects.mix_output_13__ch_monitor_in_mix_mx_lbl;
        case 13:return objects.mix_output_14__ch_monitor_in_mix_mx_lbl;
        case 14:return objects.mix_output_15__ch_monitor_in_mix_mx_lbl;
        case 15:return objects.mix_output_16__ch_monitor_in_mix_mx_lbl;
        default:return nullptr;
    }
}

static lv_obj_t *getMixOutputChannelBar(uint8_t slot){
    if(isAileronMixEditor()){
        lv_obj_t *items[]={objects.aileron_mix_output_widget1__ch_monitor_in_mix_ch,objects.aileron_mix_output_widget2__ch_monitor_in_mix_ch,objects.aileron_mix_output_widget3__ch_monitor_in_mix_ch,objects.aileron_mix_output_widget4__ch_monitor_in_mix_ch};
        return slot<4?items[slot]:nullptr;
    }
    switch(slot){
        case 0:return objects.mix_output_1__ch_monitor_in_mix_ch;
        case 1:return objects.mix_output_2__ch_monitor_in_mix_ch;
        case 2:return objects.mix_output_3__ch_monitor_in_mix_ch;
        case 3:return objects.mix_output_4__ch_monitor_in_mix_ch;
        case 4:return objects.mix_output_5__ch_monitor_in_mix_ch;
        case 5:return objects.mix_output_6__ch_monitor_in_mix_ch;
        case 6:return objects.mix_output_7__ch_monitor_in_mix_ch;
        case 7:return objects.mix_output_8__ch_monitor_in_mix_ch;
        case 8:return objects.mix_output_9__ch_monitor_in_mix_ch;
        case 9:return objects.mix_output_10__ch_monitor_in_mix_ch;
        case 10:return objects.mix_output_11__ch_monitor_in_mix_ch;
        case 11:return objects.mix_output_12__ch_monitor_in_mix_ch;
        case 12:return objects.mix_output_13__ch_monitor_in_mix_ch;
        case 13:return objects.mix_output_14__ch_monitor_in_mix_ch;
        case 14:return objects.mix_output_15__ch_monitor_in_mix_ch;
        case 15:return objects.mix_output_16__ch_monitor_in_mix_ch;
        default:return nullptr;
    }
}


static lv_obj_t *getMixOutputLeftLimitPanel(uint8_t slot){
    if(isAileronMixEditor()){
        lv_obj_t *items[]={objects.aileron_mix_output_widget1__mix_output_lbar,objects.aileron_mix_output_widget2__mix_output_lbar,objects.aileron_mix_output_widget3__mix_output_lbar,objects.aileron_mix_output_widget4__mix_output_lbar};
        return slot<4?items[slot]:nullptr;
    }
    switch(slot){
        case 0:return objects.mix_output_1__mix_output_lbar;
        case 1:return objects.mix_output_2__mix_output_lbar;
        case 2:return objects.mix_output_3__mix_output_lbar;
        case 3:return objects.mix_output_4__mix_output_lbar;
        case 4:return objects.mix_output_5__mix_output_lbar;
        case 5:return objects.mix_output_6__mix_output_lbar;
        case 6:return objects.mix_output_7__mix_output_lbar;
        case 7:return objects.mix_output_8__mix_output_lbar;
        case 8:return objects.mix_output_9__mix_output_lbar;
        case 9:return objects.mix_output_10__mix_output_lbar;
        case 10:return objects.mix_output_11__mix_output_lbar;
        case 11:return objects.mix_output_12__mix_output_lbar;
        case 12:return objects.mix_output_13__mix_output_lbar;
        case 13:return objects.mix_output_14__mix_output_lbar;
        case 14:return objects.mix_output_15__mix_output_lbar;
        case 15:return objects.mix_output_16__mix_output_lbar;
        default:return nullptr;
    }
}

static lv_obj_t *getMixOutputRightLimitPanel(uint8_t slot){
    if(isAileronMixEditor()){
        lv_obj_t *items[]={objects.aileron_mix_output_widget1__mix_output_rbar,objects.aileron_mix_output_widget2__mix_output_rbar,objects.aileron_mix_output_widget3__mix_output_rbar,objects.aileron_mix_output_widget4__mix_output_rbar};
        return slot<4?items[slot]:nullptr;
    }
    switch(slot){
        case 0:return objects.mix_output_1__mix_output_rbar;
        case 1:return objects.mix_output_2__mix_output_rbar;
        case 2:return objects.mix_output_3__mix_output_rbar;
        case 3:return objects.mix_output_4__mix_output_rbar;
        case 4:return objects.mix_output_5__mix_output_rbar;
        case 5:return objects.mix_output_6__mix_output_rbar;
        case 6:return objects.mix_output_7__mix_output_rbar;
        case 7:return objects.mix_output_8__mix_output_rbar;
        case 8:return objects.mix_output_9__mix_output_rbar;
        case 9:return objects.mix_output_10__mix_output_rbar;
        case 10:return objects.mix_output_11__mix_output_rbar;
        case 11:return objects.mix_output_12__mix_output_rbar;
        case 12:return objects.mix_output_13__mix_output_rbar;
        case 13:return objects.mix_output_14__mix_output_rbar;
        case 14:return objects.mix_output_15__mix_output_rbar;
        case 15:return objects.mix_output_16__mix_output_rbar;
        default:return nullptr;
    }
}

static lv_obj_t *getMixOutputMixBar(uint8_t slot){
    if(isAileronMixEditor()){
        lv_obj_t *items[]={objects.aileron_mix_output_widget1__ch_monitor_in_mix_mx,objects.aileron_mix_output_widget2__ch_monitor_in_mix_mx,objects.aileron_mix_output_widget3__ch_monitor_in_mix_mx,objects.aileron_mix_output_widget4__ch_monitor_in_mix_mx};
        return slot<4?items[slot]:nullptr;
    }
    switch(slot){
        case 0:return objects.mix_output_1__ch_monitor_in_mix_mx;
        case 1:return objects.mix_output_2__ch_monitor_in_mix_mx;
        case 2:return objects.mix_output_3__ch_monitor_in_mix_mx;
        case 3:return objects.mix_output_4__ch_monitor_in_mix_mx;
        case 4:return objects.mix_output_5__ch_monitor_in_mix_mx;
        case 5:return objects.mix_output_6__ch_monitor_in_mix_mx;
        case 6:return objects.mix_output_7__ch_monitor_in_mix_mx;
        case 7:return objects.mix_output_8__ch_monitor_in_mix_mx;
        case 8:return objects.mix_output_9__ch_monitor_in_mix_mx;
        case 9:return objects.mix_output_10__ch_monitor_in_mix_mx;
        case 10:return objects.mix_output_11__ch_monitor_in_mix_mx;
        case 11:return objects.mix_output_12__ch_monitor_in_mix_mx;
        case 12:return objects.mix_output_13__ch_monitor_in_mix_mx;
        case 13:return objects.mix_output_14__ch_monitor_in_mix_mx;
        case 14:return objects.mix_output_15__ch_monitor_in_mix_mx;
        case 15:return objects.mix_output_16__ch_monitor_in_mix_mx;
        default:return nullptr;
    }
}

// Dynamic rows replace the 32 EEZ-generated channel rows.
static lv_obj_t *dynamicMixChannelRows[65]={};
static lv_obj_t *dynamicBalanceChannelRow=nullptr;
static void mixChannelListRowClicked(lv_event_t *e);

static void clearDynamicChannelRows(lv_obj_t **rows,uint8_t count){
    for(uint8_t i=0;i<count;i++){
        if(rows[i]){ lv_obj_delete(rows[i]); rows[i]=nullptr; }
    }
}

static lv_obj_t *createDynamicChannelRow(lv_obj_t *parent,const char *name,uint8_t index,lv_event_cb_t callback){
    lv_obj_t *row=lv_obj_create(parent);
    lv_obj_set_width(row,lv_pct(97));
    lv_obj_set_height(row,50);
    lv_obj_set_style_min_height(row,50,LV_PART_MAIN);
    lv_obj_set_style_radius(row,0,LV_PART_MAIN);
    lv_obj_set_style_border_width(row,0,LV_PART_MAIN);
    lv_obj_set_style_pad_all(row,0,LV_PART_MAIN);
    lv_obj_set_style_bg_color(row,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_FRONT_BG1]),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_add_flag(row,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(row,LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label=lv_label_create(row);
    lv_label_set_text(label,name);
    lv_obj_set_size(label,lv_pct(85),30);
    lv_obj_align(label,LV_ALIGN_LEFT_MID,0,0);
    lv_obj_set_style_text_font(label,&ui_font_arial28,LV_PART_MAIN);
    lv_obj_set_style_text_color(label,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_TEXT]),LV_PART_MAIN);
    lv_obj_add_flag(label,LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(row,callback,LV_EVENT_CLICKED,(void *)(uintptr_t)index);
    return row;
}

static void formatMixOutputChannelName(uint8_t channel,char *buffer,size_t size){
    if(!buffer||size==0) return;

    ModelConfig &model=modelManager.getActiveModel();

    if(channel>=MAX_MODEL_CHANNELS){
        snprintf(buffer,size,"---");
        return;
    }

    const char *name=model.outputs[channel].name;

    if(name&&name[0]!='\0')
        snprintf(buffer,size,"CH%u (%s)",(unsigned)(channel+1),name);
    else
        snprintf(buffer,size,"CH%u",(unsigned)(channel+1));
}

static lv_obj_t *getMixOutputReversePanel(uint8_t slot){
    if(isAileronMixEditor()){
        lv_obj_t *panels[]={objects.aileron_mix_output_widget1__mix_output_revers_panel,objects.aileron_mix_output_widget2__mix_output_revers_panel,objects.aileron_mix_output_widget3__mix_output_revers_panel,objects.aileron_mix_output_widget4__mix_output_revers_panel};
        return slot<4?panels[slot]:nullptr;
    }
    lv_obj_t *panels[]={
        objects.mix_output_1__mix_output_revers_panel,
        objects.mix_output_2__mix_output_revers_panel,
        objects.mix_output_3__mix_output_revers_panel,
        objects.mix_output_4__mix_output_revers_panel,
        objects.mix_output_5__mix_output_revers_panel,
        objects.mix_output_6__mix_output_revers_panel,
        objects.mix_output_7__mix_output_revers_panel,
        objects.mix_output_8__mix_output_revers_panel,
        objects.mix_output_9__mix_output_revers_panel,
        objects.mix_output_10__mix_output_revers_panel,
        objects.mix_output_11__mix_output_revers_panel,
        objects.mix_output_12__mix_output_revers_panel,
        objects.mix_output_13__mix_output_revers_panel,
        objects.mix_output_14__mix_output_revers_panel,
        objects.mix_output_15__mix_output_revers_panel,
        objects.mix_output_16__mix_output_revers_panel
    };
    return slot<MAX_MIX_OUTPUTS?panels[slot]:nullptr;
}

static void updateMixOutputWidget(uint8_t slot){
    if(slot>=MAX_MIX_OUTPUTS) return;

    ModelConfig &model=modelManager.getActiveModel();

    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    lv_obj_t *root=getMixOutputRoot(slot);
    if(!root) return;

    uint8_t count=mix.outputCount;
    if(count<1) count=1;
    if(count>MAX_MIX_OUTPUTS) count=MAX_MIX_OUTPUTS;

    if(slot>=count){
        lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_remove_flag(root,LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *reversePanel=getMixOutputReversePanel(slot);
    if(reversePanel){
        if(isAileronMixEditor()) lv_obj_add_flag(reversePanel,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(reversePanel,LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t *numberLabel=getMixOutputNumberLabel(slot);
    if(numberLabel){
        if(isAileronMixEditor()) lv_label_set_text(numberLabel,(slot&1u)?"Output right":"Output left");
        else lv_label_set_text_fmt(numberLabel,"Output %u",(unsigned)(slot+1));
        // Four theme colors repeat in output order for both editors.
        const uint8_t colors[]={COLOR_ID_BAD_LBL,COLOR_ID_NOTGOOD,COLOR_ID_NOTBAD,COLOR_ID_EXELLENT};
        lv_obj_set_style_text_color(numberLabel,lv_color_hex(theme_colors[active_theme_index][colors[slot&3u]]),LV_PART_MAIN|LV_STATE_DEFAULT);
    }

    MixOutput &output=mix.mixOutputs[slot];

    lv_obj_t *reverseSwitch=getMixOutputReverseSwitch(slot);
    if(reverseSwitch){
        if(output.reversed) lv_obj_add_state(reverseSwitch,LV_STATE_CHECKED);
        else lv_obj_remove_state(reverseSwitch,LV_STATE_CHECKED);
    }

    lv_obj_t *nameLabel=getMixOutputNameLabel(slot);
    lv_obj_t *monitor=getMixOutputMonitor(slot);

    if(output.channel>=model.channelCount||output.channel>=MAX_MODEL_CHANNELS){
        output.channel=MIX_OUTPUT_NONE;

        if(nameLabel) lv_label_set_text(nameLabel,"None");
        if(monitor) lv_obj_add_flag(monitor,LV_OBJ_FLAG_HIDDEN);
        return;
    }

    char channelName[48];
    formatMixOutputChannelName(output.channel,channelName,sizeof(channelName));

    if(nameLabel) lv_label_set_text(nameLabel,channelName);

    lv_obj_t *chNum=getMixOutputChannelNumberLabel(slot);
    if(chNum)
        lv_label_set_text_fmt(chNum,"CH%u",(unsigned)(output.channel+1));

    if(monitor)
        lv_obj_remove_flag(monitor,LV_OBJ_FLAG_HIDDEN);
}

static void updateMixOutputsUi(){
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    if(mix.outputCount<1) mix.outputCount=1;
    if(mix.outputCount>(isAileronMixEditor()?4:MAX_MIX_OUTPUTS)) mix.outputCount=isAileronMixEditor()?4:MAX_MIX_OUTPUTS;
    if(isAileronMixEditor()) mix.outputCount=(uint8_t)normalizeAileronOutputCount(mix.outputCount);

    mixOutputCountValue=mix.outputCount;

    lv_obj_t *countLabel=isAileronMixEditor()?objects.aileron_mix_output_count_lbl:objects.mix_output_count_lbl;
    if(countLabel) lv_label_set_text_fmt(countLabel,"%u",(unsigned)mix.outputCount);

    for(uint8_t slot=0;slot<(isAileronMixEditor()?4:MAX_MIX_OUTPUTS);slot++)
        updateMixOutputWidget(slot);
}

static void updateMixOutputMonitors(){
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    // Не привязываем live monitor к lv_screen_active() и состоянию родительской
    // панели. EEZ может переключать/перепривязывать контейнеры, а сами pointers
    // mix_output_N при этом остаются валидными.
    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    uint8_t count=mix.outputCount;
    if(count>MAX_MIX_OUTPUTS) count=MAX_MIX_OUTPUTS;

    for(uint8_t slot=0;slot<count;slot++){
        MixOutput &mixOutput=mix.mixOutputs[slot];
        uint8_t channel=mixOutput.channel;

        if(channel>=model.channelCount||channel>=MAX_MODEL_CHANNELS)
            continue;

        lv_obj_t *monitor=getMixOutputMonitor(slot);
        if(monitor&&lv_obj_has_flag(monitor,LV_OBJ_FLAG_HIDDEN))
            lv_obj_remove_flag(monitor,LV_OBJ_FLAG_HIDDEN);

        // normalized: -1000..+1000 = -100.0%..+100.0%
        // Не используем %f: в текущей embedded printf-конфигурации float-format
        // не поддерживается и ломает последующие varargs (%d).
        int16_t mixNormalized=mixer.getChannel(channel);
        int16_t outputNormalized=outputProcessor.getChannelNormalized(channel);
        int outputUs=outputProcessor.getChannelUs(channel);

        lv_obj_t *chLabel=getMixOutputChannelLabel(slot);
        if(chLabel){
            int32_t absValue=outputNormalized<0?-(int32_t)outputNormalized:(int32_t)outputNormalized;
            char buffer[48];
            snprintf(
                buffer,
                sizeof(buffer),
                "Output: %s%ld.%ld%% (%d)",
                outputNormalized<0?"-":"",
                (long)(absValue/10),
                (long)(absValue%10),
                outputUs
            );
            lv_label_set_text(chLabel,buffer);
        }

        lv_obj_t *mixLabel=getMixOutputMixLabel(slot);
        if(mixLabel){
            int32_t absValue=mixNormalized<0?-(int32_t)mixNormalized:(int32_t)mixNormalized;
            char buffer[32];
            snprintf(
                buffer,
                sizeof(buffer),
                "Mixes: %s%ld.%ld%%",
                mixNormalized<0?"-":"",
                (long)(absValue/10),
                (long)(absValue%10)
            );
            lv_label_set_text(mixLabel,buffer);
        }

        lv_obj_t *chBar=getMixOutputChannelBar(slot);
        if(chBar){
            int monitorUs=1500+((int32_t)outputNormalized*512L)/1000L;
            int monitorSubtrimUs=1500+((int32_t)model.outputs[channel].subtrim*512L)/1000L;
            monitorUs=constrain(monitorUs,732,2268);
            monitorSubtrimUs=constrain(monitorSubtrimUs,732,2268);

            // На всякий случай восстанавливаем ожидаемую шкалу/режим,
            // даже если EEZ когда-либо изменит начальные свойства bar.
            lv_bar_set_range(chBar,732,2268);
            lv_bar_set_mode(chBar,LV_BAR_MODE_RANGE);

            if(monitorUs>=monitorSubtrimUs){
                lv_bar_set_start_value(chBar,monitorSubtrimUs,LV_ANIM_OFF);
                lv_bar_set_value(chBar,monitorUs,LV_ANIM_OFF);
            }else{
                lv_bar_set_start_value(chBar,monitorUs,LV_ANIM_OFF);
                lv_bar_set_value(chBar,monitorSubtrimUs,LV_ANIM_OFF);
            }

            lv_obj_invalidate(chBar);

            // Панели по краям bar показывают недоступную часть хода,
            // точно так же как в редакторе канала.
            const OutputConfig &outputCfg=model.outputs[channel];

            int32_t minPct=outputCfg.min<0?-(int32_t)outputCfg.min:(int32_t)outputCfg.min;
            int32_t maxPct=outputCfg.max<0?-(int32_t)outputCfg.max:(int32_t)outputCfg.max;
            minPct/=10;
            maxPct/=10;
            minPct=constrain(minPct,0L,150L);
            maxPct=constrain(maxPct,0L,150L);

            int32_t barWidth=lv_obj_get_width(chBar);
            if(barWidth<2) barWidth=550;

            int32_t halfWidth=barWidth/2;

            // При 150% панель = 0 px, при 100% закрывается 1/3 половины bar.
            int32_t leftWidth=map(minPct,0,150,halfWidth,0);
            int32_t rightWidth=map(maxPct,0,150,halfWidth,0);

            leftWidth=constrain(leftWidth,0L,halfWidth);
            rightWidth=constrain(rightWidth,0L,halfWidth);

            lv_obj_t *leftLimit=getMixOutputLeftLimitPanel(slot);
            if(leftLimit){
                lv_obj_set_x(leftLimit,0);
                lv_obj_set_width(leftLimit,leftWidth);
                lv_obj_invalidate(leftLimit);
            }

            lv_obj_t *rightLimit=getMixOutputRightLimitPanel(slot);
            if(rightLimit){
                // В EEZ правый overlay заканчивается за 2 px до правого края bar.
                int32_t rightX=barWidth-rightWidth-2;
                if(rightX<0) rightX=0;
                lv_obj_set_align(rightLimit,LV_ALIGN_TOP_LEFT);
                lv_obj_set_x(rightLimit,rightX);
                lv_obj_set_width(rightLimit,rightWidth);
                lv_obj_invalidate(rightLimit);
            }
        }

        lv_obj_t *mixBar=getMixOutputMixBar(slot);
        if(mixBar){
            const int MIX_CENTER_US=1500;

            // mixNormalized -1000..+1000 -> 988..2012 us,
            // при +/-1500 -> 732..2268 us.
            int mixUs=MIX_CENTER_US+((int32_t)mixNormalized*512L)/1000L;
            mixUs=constrain(mixUs,732,2268);

            // Нижний Mixes bar должен работать по той же шкале, что и
            // штатный channel monitor.
            lv_bar_set_range(mixBar,732,2268);
            lv_bar_set_mode(mixBar,LV_BAR_MODE_RANGE);

            if(mixUs>=MIX_CENTER_US){
                lv_bar_set_start_value(mixBar,MIX_CENTER_US,LV_ANIM_OFF);
                lv_bar_set_value(mixBar,mixUs,LV_ANIM_OFF);
            }else{
                lv_bar_set_start_value(mixBar,mixUs,LV_ANIM_OFF);
                lv_bar_set_value(mixBar,MIX_CENTER_US,LV_ANIM_OFF);
            }

            lv_obj_invalidate(mixBar);
        }
    }
}

static void applyMixOutputCountFromEditor(){
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    mixOutputCountValue=(int16_t)constrain((int32_t)mixOutputCountValue,1L,isAileronMixEditor()?4L:(int32_t)MAX_MIX_OUTPUTS);
    if(isAileronMixEditor()) mixOutputCountValue=normalizeAileronOutputCount(mixOutputCountValue);
    mix.outputCount=(uint8_t)mixOutputCountValue;

    updateMixOutputsUi();
    mixer.resetRuntime();

    if(isMixEditorMainVisible())
        applyMixEditorEncoderFocus();
}

static void mixOutputCountPanelClicked(lv_event_t *e){
    mixChangerOpenTarget=e?(lv_obj_t *)lv_event_get_current_target(e):(isAileronMixEditor()?objects.aileron_mix_output_count_panel:objects.mix_output_count_panel);

    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    mixOutputCountValue=(int16_t)constrain((int32_t)mix.outputCount,1L,isAileronMixEditor()?4L:(int32_t)MAX_MIX_OUTPUTS);
    if(isAileronMixEditor()) mixOutputCountValue=normalizeAileronOutputCount(mixOutputCountValue);
    activeMixActionValueId=50;

    if(objects.mix_changer_panel__button_panel)
        lv_obj_remove_flag(objects.mix_changer_panel__button_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.mix_changer_panel__slider_panel)
        lv_obj_add_flag(objects.mix_changer_panel__slider_panel,LV_OBJ_FLAG_HIDDEN);

    updateMixActionChangerUi();

    bindChangerDismissRecursive(isAileronMixEditor()?objects.aileron_mixes_edit_panel:objects.free_mixes_edit_panel,objects.mix_changer_panel,dismissMixChangerOnPress);
    if(objects.mix_changer_panel)
        lv_obj_remove_flag(objects.mix_changer_panel,LV_OBJ_FLAG_HIDDEN);
}

static void hideChannelOkButton(lv_obj_t *okButton){
    if(!okButton) return;
    lv_obj_t *panel=lv_obj_get_parent(okButton);
    if(panel) lv_obj_add_flag(panel,LV_OBJ_FLAG_HIDDEN);
}

static void showMixChannelList(uint8_t outputSlot){
    if(outputSlot>=MAX_MIX_OUTPUTS) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    activeMixOutputSlot=(int8_t)outputSlot;
    pendingMixOutputChannel=mix.mixOutputs[outputSlot].channel;

    lv_obj_t *list=objects.channels_list_in_mix__channel_list_panel;
    if(!list) return;
    clearDynamicChannelRows(dynamicMixChannelRows,65);
    lv_obj_set_height(list,245);
    lv_obj_set_flex_flow(list,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list,0,LV_PART_MAIN);
    lv_obj_set_scroll_dir(list,LV_DIR_VER);
    lv_obj_add_flag(list,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *noneRow=createDynamicChannelRow(list,"None",0xFF,mixChannelListRowClicked);
    dynamicMixChannelRows[0]=noneRow;
    // Always show the complete CH1..CH64 selection range in Mixes.
    // The model's channelCount can be lower; unsupported outputs must not be written into the model.
    for(uint8_t channel=0;channel<64;channel++){
        char channelName[48];
        if(channel<MAX_MODEL_CHANNELS)
            formatMixOutputChannelName(channel,channelName,sizeof(channelName));
        else
            snprintf(channelName,sizeof(channelName),"CH%u",(unsigned)(channel+1));
        dynamicMixChannelRows[channel+1]=createDynamicChannelRow(list,channelName,channel,mixChannelListRowClicked);
        
    }
    lv_obj_scroll_to_y(list,0,LV_ANIM_OFF);
    hideChannelOkButton(objects.channels_list_in_mix__ch_list_ok);

    if(objects.channels_list_in_mix__channel_list_lbl)
        lv_label_set_text(objects.channels_list_in_mix__channel_list_lbl,"Channels");

    if(objects.channels_list_in_mix){
        lv_obj_remove_flag(objects.channels_list_in_mix,LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(objects.channels_list_in_mix);
    }
}

static void mixOutputNamePanelClicked(lv_event_t *e){
    if(!e) return;
    uint8_t slot=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    showMixChannelList(slot);
}

static void mixOutputRootClicked(lv_event_t *e){
    if(!e) return;

    uint8_t slot=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(slot>=MAX_MIX_OUTPUTS) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    uint8_t count=mix.outputCount;
    if(count<1) count=1;
    if(count>MAX_MIX_OUTPUTS) count=MAX_MIX_OUTPUTS;
    if(slot>=count) return;

    uint8_t channel=mix.mixOutputs[slot].channel;
    if(channel>=model.channelCount||channel>=MAX_MODEL_CHANNELS)
        return;

    openChannelEditorDirect(channel,true);
}

static void mixOutputReverseChanged(lv_event_t *e){
    if(!e) return;
    uint8_t slot=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(slot>=MAX_MIX_OUTPUTS) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    lv_obj_t *sw=(lv_obj_t *)lv_event_get_target(e);
    if(!sw) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    mix.mixOutputs[slot].reversed=lv_obj_has_state(sw,LV_STATE_CHECKED);

    mixer.resetRuntime();
}

// Channel names belong to ModelConfig.outputs, never to an individual MixOutput.
// A MixOutput only stores its zero-based channel number.
static int8_t pendingChannelDialogMix=-1;
static int8_t pendingChannelDialogSlot=-1;
static uint8_t pendingChannelDialogOld=MIX_OUTPUT_NONE;
static uint8_t pendingChannelDialogNew=MIX_OUTPUT_NONE;
static bool pendingChannelDialogSwapped=false;

static bool isChannelUsedByAnyMix(uint8_t channel,int8_t exceptMix=-1,int8_t exceptSlot=-1){
    if(channel>=MAX_MODEL_CHANNELS) return false;
    ModelConfig &model=modelManager.getActiveModel();
    for(uint8_t i=0;i<MAX_MIX_LINES;i++){
        const MixLine &mix=model.mixes[i];
        if(!mix.enabled) continue;
        uint8_t count=mix.outputCount>MAX_MIX_OUTPUTS?MAX_MIX_OUTPUTS:mix.outputCount;
        for(uint8_t slot=0;slot<count;slot++){
            if((int8_t)i==exceptMix&&(int8_t)slot==exceptSlot) continue;
            if(mix.mixOutputs[slot].channel==channel) return true;
        }
    }
    return false;
}

static void clearPendingMixChannelDialog(){
    pendingChannelDialogMix=-1;
    pendingChannelDialogSlot=-1;
    pendingChannelDialogOld=MIX_OUTPUT_NONE;
    pendingChannelDialogNew=MIX_OUTPUT_NONE;
    pendingChannelDialogSwapped=false;
}

static void showMixChannelCopyNameQuestion();
static void showMixChannelClearNameQuestion();

static void finishMixChannelAssignment(){
    if(pendingChannelDialogMix<0||pendingChannelDialogMix>=MAX_MIX_LINES||pendingChannelDialogSlot<0||pendingChannelDialogSlot>=MAX_MIX_OUTPUTS){
        clearPendingMixChannelDialog();
        return;
    }
    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)pendingChannelDialogMix];
    if(!mix.enabled){clearPendingMixChannelDialog();return;}
    uint8_t slot=(uint8_t)pendingChannelDialogSlot;
    mix.mixOutputs[slot].channel=pendingChannelDialogNew;
    if(pendingChannelDialogNew<MAX_MODEL_CHANNELS&&model.channelCount<=pendingChannelDialogNew)
        model.channelCount=(uint8_t)(pendingChannelDialogNew+1);
    if(slot==0) mix.destination=pendingChannelDialogNew<MAX_MODEL_CHANNELS?pendingChannelDialogNew:0;
    activeMixOutputSlot=-1;
    pendingMixOutputChannel=MIX_OUTPUT_NONE;
    if(objects.channels_list_in_mix) lv_obj_add_flag(objects.channels_list_in_mix,LV_OBJ_FLAG_HIDDEN);
    updateMixOutputWidget(slot);
    mixer.resetRuntime();
    modelManager.requestSaveActiveModel();
    // Offer to clear the old channel name only after it becomes completely unused.
    uint8_t old=pendingChannelDialogOld;
    if(!pendingChannelDialogSwapped&&old<MAX_MODEL_CHANNELS&&old!=pendingChannelDialogNew&&!isChannelUsedByAnyMix(old)&&model.outputs[old].name[0]!='\0'){
        showMixChannelClearNameQuestion();
        return;
    }
    clearPendingMixChannelDialog();
}

// A single theme-aware confirmation overlay is shared by every channel question.
// Keep it separate from generated EEZ screens and standard LVGL msgboxes.
static lv_obj_t *mixChannelQuestionOverlay=nullptr;

static void closeMixChannelQuestion(){
    lv_obj_t *overlay=mixChannelQuestionOverlay;
    mixChannelQuestionOverlay=nullptr;
    if(overlay) lv_obj_delete_async(overlay);
}

static lv_obj_t *createMixChannelQuestion(const char *message,const char *thirdLabel=nullptr){
    closeMixChannelQuestion();
    lv_obj_t *overlay=lv_obj_create(lv_layer_top());
    mixChannelQuestionOverlay=overlay;
    lv_obj_remove_flag(overlay,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(overlay,lv_pct(100),lv_pct(100));
    lv_obj_align(overlay,LV_ALIGN_CENTER,0,0);
    lv_obj_set_style_pad_all(overlay,0,LV_PART_MAIN);
    lv_obj_set_style_border_width(overlay,0,LV_PART_MAIN);
    lv_obj_set_style_radius(overlay,0,LV_PART_MAIN);
    lv_obj_set_style_bg_color(overlay,lv_color_black(),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(overlay,LV_OPA_60,LV_PART_MAIN);
    lv_obj_add_flag(overlay,LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *panel=lv_obj_create(overlay);
    lv_obj_set_size(panel,570,224);
    lv_obj_center(panel);
    lv_obj_remove_flag(panel,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(panel,10,LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel,0,LV_PART_MAIN);
    lv_obj_set_style_border_width(panel,2,LV_PART_MAIN);
    lv_obj_set_style_border_color(panel,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_BORDER_COLOR]),LV_PART_MAIN);
    lv_obj_set_style_bg_color(panel,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_FRONT_BG]),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel,LV_OPA_COVER,LV_PART_MAIN);

    lv_obj_t *header=lv_obj_create(panel);
    lv_obj_set_size(header,566,48);
    lv_obj_align(header,LV_ALIGN_TOP_MID,0,0);
    lv_obj_remove_flag(header,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(header,0,LV_PART_MAIN);
    lv_obj_set_style_border_width(header,0,LV_PART_MAIN);
    lv_obj_set_style_radius(header,7,LV_PART_MAIN);
    lv_obj_set_style_bg_color(header,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_FRONT_BG1]),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header,LV_OPA_COVER,LV_PART_MAIN);

    lv_obj_t *title=lv_label_create(header);
    lv_label_set_text(title,"Confirm");
    lv_obj_set_style_text_font(title,&lv_font_montserrat_22,LV_PART_MAIN);
    lv_obj_set_style_text_color(title,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_TEXT]),LV_PART_MAIN);
    lv_obj_align(title,LV_ALIGN_LEFT_MID,18,0);

    lv_obj_t *body=lv_label_create(panel);
    lv_obj_set_width(body,524);
    lv_label_set_long_mode(body,LV_LABEL_LONG_WRAP);
    lv_label_set_text(body,message);
    lv_obj_set_style_text_font(body,&lv_font_montserrat_20,LV_PART_MAIN);
    lv_obj_set_style_text_color(body,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_TEXT]),LV_PART_MAIN);
    lv_obj_align(body,LV_ALIGN_TOP_LEFT,22,66);

    // Buttons belong to the same overlay and use the active system palette.
    (void)thirdLabel;
    return panel;
}

static lv_obj_t *addMixChannelQuestionButton(lv_obj_t *panel,const char *caption,uint8_t index,uint8_t count,lv_event_cb_t callback,uintptr_t choice){
    if(!panel) return nullptr;
    const int32_t buttonWidth=count==3?144:174;
    const int32_t gap=12;
    const int32_t total=buttonWidth*count+gap*(count-1);
    const int32_t x=-(total-buttonWidth)/2+(buttonWidth+gap)*index;
    lv_obj_t *button=lv_button_create(panel);
    lv_obj_set_size(button,buttonWidth,48);
    lv_obj_align(button,LV_ALIGN_BOTTOM_MID,x,-14);
    lv_obj_set_style_radius(button,7,LV_PART_MAIN);
    lv_obj_set_style_border_width(button,1,LV_PART_MAIN);
    lv_obj_set_style_border_color(button,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_BORDER_COLOR]),LV_PART_MAIN);
    const uint32_t colorId=index==0?COLOR_ID_DONE_BUTTON:(count==3&&index==2?COLOR_ID_CANCEL_BUTTON:COLOR_ID_FRONT_BG1);
    lv_obj_set_style_bg_color(button,lv_color_hex(theme_colors[active_theme_index][colorId]),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_bg_color(button,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_MS_TEXT]),LV_PART_MAIN|LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(button,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_MS_TEXT]),LV_PART_MAIN|LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(button,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_TEXT]),LV_PART_MAIN);
    lv_obj_set_style_text_font(button,&lv_font_montserrat_20,LV_PART_MAIN);
    lv_obj_t *text=lv_label_create(button);
    lv_label_set_text(text,caption);
    lv_obj_center(text);
    lv_obj_add_event_cb(button,callback,LV_EVENT_CLICKED,(void *)choice);
    if(index==0) lv_obj_add_state(button,LV_STATE_FOCUSED);
    return button;
}

// The same UI construction path is used by every confirmation in actions.cpp.
static void closeSystemConfirmation(){
    closeMixChannelQuestion();
}

static void showSystemConfirmation(const char *message,const char *primary,lv_event_cb_t primaryCb,const char *secondary,lv_event_cb_t secondaryCb){
    lv_obj_t *panel=createMixChannelQuestion(message,nullptr);
    if(message&&strncmp(message,"This curve is used by:",22)==0) lv_obj_set_height(panel,274);
    const uint8_t count=secondary?2:1;
    addMixChannelQuestionButton(panel,primary,0,count,primaryCb,0);
    if(secondary) addMixChannelQuestionButton(panel,secondary,1,count,secondaryCb,0);
}

static void mixChannelClearNameAnswer(lv_event_t *e){
    bool yes=(uintptr_t)lv_event_get_user_data(e)==1;
    closeMixChannelQuestion();
    if(yes&&pendingChannelDialogOld<MAX_MODEL_CHANNELS){
        ModelConfig &model=modelManager.getActiveModel();
        if(!isChannelUsedByAnyMix(pendingChannelDialogOld)){
            model.outputs[pendingChannelDialogOld].name[0]='\0';
            modelManager.requestSaveActiveModel();
        }
    }
    clearPendingMixChannelDialog();
}

static void showMixChannelClearNameQuestion(){
    if(pendingChannelDialogOld>=MAX_MODEL_CHANNELS){clearPendingMixChannelDialog();return;}
    char message[160];
    snprintf(message,sizeof(message),"Channel %u isn't used by any mix.\nDo you want to clear its name?",(unsigned)(pendingChannelDialogOld+1));
    lv_obj_t *panel=createMixChannelQuestion(message);
    addMixChannelQuestionButton(panel,"Yes",0,2,mixChannelClearNameAnswer,1);
    addMixChannelQuestionButton(panel,"No",1,2,mixChannelClearNameAnswer,0);
}

static void mixChannelCopyNameAnswer(lv_event_t *e){
    bool yes=(uintptr_t)lv_event_get_user_data(e)==1;
    closeMixChannelQuestion();
    if(yes&&pendingChannelDialogNew<MAX_MODEL_CHANNELS&&pendingChannelDialogMix>=0){
        ModelConfig &model=modelManager.getActiveModel();
        OutputConfig &output=model.outputs[pendingChannelDialogNew];
        if(output.name[0]=='\0'){
            const char *name=model.mixes[(uint8_t)pendingChannelDialogMix].name;
            if(name[0]!='\0') snprintf(output.name,sizeof(output.name),"%s",name);
        }
    }
    finishMixChannelAssignment();
}

static void showMixChannelCopyNameQuestion(){
    lv_obj_t *panel=createMixChannelQuestion("The channel doesn't have a name.\nDo you want to copy the mix's name to the channel?");
    addMixChannelQuestionButton(panel,"Yes",0,2,mixChannelCopyNameAnswer,1);
    addMixChannelQuestionButton(panel,"No",1,2,mixChannelCopyNameAnswer,0);
}

static void mixChannelSwapAnswer(lv_event_t *e){
    uintptr_t choice=(uintptr_t)lv_event_get_user_data(e);
    closeMixChannelQuestion();
    if(choice==2){clearPendingMixChannelDialog();return;}
    ModelConfig &model=modelManager.getActiveModel();
    if(choice==1&&pendingChannelDialogOld<MAX_MODEL_CHANNELS&&pendingChannelDialogNew<MAX_MODEL_CHANNELS){
        OutputConfig tmp=model.outputs[pendingChannelDialogOld];
        model.outputs[pendingChannelDialogOld]=model.outputs[pendingChannelDialogNew];
        model.outputs[pendingChannelDialogNew]=tmp;
        pendingChannelDialogSwapped=true;
    }
    finishMixChannelAssignment();
}

static void showMixChannelSwapQuestion(){
    char message[160];
    snprintf(message,sizeof(message),"Do you want to swap CH%u and CH%u channels settings?",(unsigned)(pendingChannelDialogOld+1),(unsigned)(pendingChannelDialogNew+1));
    lv_obj_t *panel=createMixChannelQuestion(message,"Cancel");
    addMixChannelQuestionButton(panel,"Yes",0,3,mixChannelSwapAnswer,1);
    addMixChannelQuestionButton(panel,"No",1,3,mixChannelSwapAnswer,0);
    addMixChannelQuestionButton(panel,"Cancel",2,3,mixChannelSwapAnswer,2);
}

static void mixChannelListRowClicked(lv_event_t *e){
    if(!e||activeMixOutputSlot<0) return;
    uint8_t channel=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(channel!=MIX_OUTPUT_NONE&&channel>=MAX_MODEL_CHANNELS) return;
    pendingMixOutputChannel=channel;
    mixChannelListOkClicked(nullptr);
}

static void mixChannelListOkClicked(lv_event_t *e){
    (void)e;
    if(activeMixOutputSlot<0||activeMixOutputSlot>=MAX_MIX_OUTPUTS) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];
    uint8_t slot=(uint8_t)activeMixOutputSlot;
    uint8_t old=mix.mixOutputs[slot].channel;
    uint8_t chosen=pendingMixOutputChannel;
    if(chosen==old){
        activeMixOutputSlot=-1;
        if(objects.channels_list_in_mix) lv_obj_add_flag(objects.channels_list_in_mix,LV_OBJ_FLAG_HIDDEN);
        return;
    }
    pendingChannelDialogMix=editingMixIndex;
    pendingChannelDialogSlot=(int8_t)slot;
    pendingChannelDialogOld=old;
    pendingChannelDialogNew=chosen;
    pendingChannelDialogSwapped=false;
    if(objects.channels_list_in_mix) lv_obj_add_flag(objects.channels_list_in_mix,LV_OBJ_FLAG_HIDDEN);
    // ETHOS-like swap question when changing an assigned channel to a channel
    // already used by another mix. Unassigned output never needs a swap.
    if(old<MAX_MODEL_CHANNELS&&chosen<MAX_MODEL_CHANNELS&&isChannelUsedByAnyMix(chosen,editingMixIndex,(int8_t)slot)){
        showMixChannelSwapQuestion();
        return;
    }
    if(chosen<MAX_MODEL_CHANNELS&&model.outputs[chosen].name[0]=='\0'&&mix.name[0]!='\0'){
        showMixChannelCopyNameQuestion();
        return;
    }
    finishMixChannelAssignment();
}

static void setupMixOutputUiCallbacks(){
    if(!objects.scr_mixes) return;
    if(mixOutputCallbacksBoundScreen==objects.scr_mixes) return;

    mixOutputCallbacksBoundScreen=objects.scr_mixes;

    bindChangerDismissRecursive(objects.free_mixes_edit_panel,objects.mix_changer_panel,dismissMixChangerOnPress);

    if(objects.mix_output_count_panel)
        lv_obj_add_event_cb(objects.mix_output_count_panel,mixOutputCountPanelClicked,LV_EVENT_CLICKED,nullptr);
    if(objects.aileron_mix_output_count_panel)
        lv_obj_add_event_cb(objects.aileron_mix_output_count_panel,mixOutputCountPanelClicked,LV_EVENT_CLICKED,nullptr);

    MixEditorKind priorKind=activeMixEditorKind;
    for(uint8_t kind=0;kind<2;kind++){
        activeMixEditorKind=kind?MixEditorKind::AILERON:MixEditorKind::FREE_MIX;
        for(uint8_t slot=0;slot<(kind?4:MAX_MIX_OUTPUTS);slot++){
        lv_obj_t *root=getMixOutputRoot(slot);
        if(root){
            lv_obj_add_flag(root,LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(
                root,
                mixOutputRootClicked,
                LV_EVENT_CLICKED,
                (void *)(uintptr_t)slot
            );
        }

        // The visible channel-monitor widget fills most of mix_output_N.
        // Bind it directly because child widgets do not reliably bubble CLICKED
        // to the mix_output_N root in this EEZ/LVGL hierarchy.
        lv_obj_t *monitor=getMixOutputMonitor(slot);
        if(monitor){
            lv_obj_add_flag(monitor,LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(
                monitor,
                mixOutputRootClicked,
                LV_EVENT_CLICKED,
                (void *)(uintptr_t)slot
            );
        }

        lv_obj_t *namePanel=getMixOutputNamePanel(slot);
        if(namePanel)
            lv_obj_add_event_cb(namePanel,mixOutputNamePanelClicked,LV_EVENT_CLICKED,(void *)(uintptr_t)slot);

        lv_obj_t *reverseSwitch=getMixOutputReverseSwitch(slot);
        if(reverseSwitch)
            lv_obj_add_event_cb(reverseSwitch,mixOutputReverseChanged,LV_EVENT_VALUE_CHANGED,(void *)(uintptr_t)slot);
    }

    }
    activeMixEditorKind=priorKind;
    if(objects.channels_list_in_mix__ch_list_ok)
        lv_obj_add_event_cb(objects.channels_list_in_mix__ch_list_ok,mixChannelListOkClicked,LV_EVENT_CLICKED,nullptr);
}




static void deletePopupDismissOverlayAsync(void *param){
    lv_obj_t *overlay=(lv_obj_t *)param;
    if(overlay) lv_obj_delete(overlay);
}

static void hidePopupDismissOverlay(){
    if(!popupDismissOverlay) return;

    lv_obj_t *overlay=popupDismissOverlay;
    popupDismissOverlay=nullptr;
    lv_async_call(deletePopupDismissOverlayAsync,overlay);
}

static void popupDismissOverlayClicked(lv_event_t *e){
    (void)e;

    stopMixSourceStickFocusDetect();

    lv_obj_t *sourceSelectorRoot=getMixSourceSelectorRoot();

    if(sourceSelectorRoot)
        lv_obj_add_flag(sourceSelectorRoot,LV_OBJ_FLAG_HIDDEN);

    if(objects.operation_panel)
        lv_obj_add_flag(objects.operation_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.curve_popup)
        lv_obj_add_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN);

    if(objects.mix_invert_panel)
        lv_obj_add_flag(objects.mix_invert_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.action_type_selcet_panel)
        lv_obj_add_flag(objects.action_type_selcet_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.trim_type_select_panel)
        lv_obj_add_flag(objects.trim_type_select_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.vars_active_condition_panel)
        lv_obj_add_flag(objects.vars_active_condition_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.var_option_panel)
        lv_obj_add_flag(objects.var_option_panel,LV_OBJ_FLAG_HIDDEN);

    hidePopupDismissOverlay();
}

static void showPopupDismissOverlay(lv_obj_t *popup){
    if(!popup) return;

    hidePopupDismissOverlay();

    // Все popup в этом интерфейсе открываются по центру экрана.
    lv_obj_set_align(popup,LV_ALIGN_CENTER);
    lv_obj_set_pos(popup,0,0);

    lv_obj_t *screen=lv_screen_active();
    if(!screen) return;

    popupDismissOverlay=lv_obj_create(screen);
    if(!popupDismissOverlay) return;

    lv_obj_set_pos(popupDismissOverlay,0,0);
    lv_obj_set_size(popupDismissOverlay,LV_PCT(100),LV_PCT(100));

    lv_obj_set_style_bg_opa(popupDismissOverlay,LV_OPA_TRANSP,LV_PART_MAIN);
    lv_obj_set_style_border_width(popupDismissOverlay,0,LV_PART_MAIN);
    lv_obj_set_style_pad_all(popupDismissOverlay,0,LV_PART_MAIN);

    lv_obj_add_flag(popupDismissOverlay,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(popupDismissOverlay,LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(
        popupDismissOverlay,
        popupDismissOverlayClicked,
        LV_EVENT_CLICKED,
        nullptr
    );

    // Overlay должен быть выше экрана, но ниже самого popup.
    lv_obj_move_foreground(popupDismissOverlay);
    lv_obj_move_foreground(popup);
}

static lv_obj_t *getMixSourceSelectorRoot(){
    if(!objects.category_panel) return nullptr;
    return lv_obj_get_parent(objects.category_panel);
}

static void clearMixSourceMembers(){
    mixSourceAnalogMembersActive=false;
    if(!objects.members_panel) return;

    // Между строками Member оставляем 1 px, как в Category.
    lv_obj_set_style_pad_row(objects.members_panel,1,LV_PART_MAIN);

    // Первый child в members_panel — статический заголовок "Member".
    // Все следующие children создаются динамически.
    while(lv_obj_get_child_count(objects.members_panel)>1){
        lv_obj_t *child=lv_obj_get_child(objects.members_panel,1);
        if(!child) break;
        lv_obj_delete(child);
    }
}

static SourceId getMixBaseSource(const MixLine &mix){
    SourceId source=mix.source;
    source.index&=(uint8_t)~MIX_SOURCE_HALF_RANGE_FLAG;
    return source;
}

static bool mixSourceSupportsHalfRange(const MixLine &mix){
    SourceId source=getMixBaseSource(mix);

    if(source.type==SourceType::LOGICAL_SWITCH)
        return source.index<MAX_LOGICAL_SWITCHES;

    if(source.type!=SourceType::SWITCH_INPUT)
        return false;

    // Physical switch IDs are contiguous from SW_3POS_1 through SW_SLOT_14.
    // The actual position count is configured in scr_hw, not by the enum name.
    const uint8_t firstSwitch=static_cast<uint8_t>(SwitchInput::SW_3POS_1);
    const uint8_t input=source.index;
    if(input<firstSwitch||input>=firstSwitch+14) return false;

    const uint8_t hardwareSlot=input-firstSwitch;
    const uint8_t type=hardwareSwitchType(hardwareSlot);
    return type==1||type==3; // 2-POS and Momentary are both two-state sources.

}

static bool mixSourceHalfRangeEnabled(const MixLine &mix){
    return mixSourceSupportsHalfRange(mix)&&(mix.source.index&MIX_SOURCE_HALF_RANGE_FLAG)!=0;
}

static void setMixSourceHalfRange(MixLine &mix,bool enabled){
    SourceId source=getMixBaseSource(mix);

    if(enabled&&mixSourceSupportsHalfRange(mix))
        source.index|=MIX_SOURCE_HALF_RANGE_FLAG;

    mix.source=source;
}

static int32_t applyMixSourceOptionsValue(int32_t value,const MixLine &mix){
    // Half range maps -100..+100 to 0..+100 first.
    // Invert then changes +100 to -100 without changing zero.
    if(mixSourceHalfRangeEnabled(mix)){
        value=constrain(value,-1000L,1000L);
        value=(value+1000L)/2L;
    }
    if(mix.reversed) value=-value;
    return value;
}

static int16_t getMixSourceRawLiveValue(const MixLine &mix){
    SourceId source=getMixBaseSource(mix);

    if(source.type==SourceType::MODEL_INPUT)
        return modelInputProcessor.getValue(source.index);

    if(source.type==SourceType::CHANNEL_OUTPUT)
        return mixer.getChannel(source.index);

    return sourceResolver.getValue(source);
}

static void updateMixSourceEditorUi(){
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    if(objects.mix_source_lbl){
        char name[32];
        formatMixSourceName(mix.source,name,sizeof(name));

        if(mix.reversed&&strcmp(name,"---")!=0){
            char invertedName[34];
            snprintf(invertedName,sizeof(invertedName),"-%s",name);
            lv_label_set_text(objects.mix_source_lbl,invertedName);
        }else{
            lv_label_set_text(objects.mix_source_lbl,name);
        }
    }

    if(objects.mix_source_percent){
        int16_t value=getMixSourceLiveValue(mix);
        int32_t percent=value/10;
        percent=constrain(percent,-100L,100L);

        char buffer[16];
        snprintf(buffer,sizeof(buffer),"%ld%%",(long)percent);
        lv_label_set_text(objects.mix_source_percent,buffer);
    }
}

static lv_obj_t *createMixSourceMemberRow(const char *name,uint8_t sourceIndex){
    if(!objects.members_panel||!name) return nullptr;

    lv_obj_t *row=lv_obj_create(objects.members_panel);
    lv_obj_set_size(row,LV_PCT(95),40);
    lv_obj_set_style_pad_left(row,0,LV_PART_MAIN);
    lv_obj_set_style_pad_top(row,0,LV_PART_MAIN);
    lv_obj_set_style_pad_right(row,0,LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(row,0,LV_PART_MAIN);
    lv_obj_set_style_border_width(row,0,LV_PART_MAIN);
    lv_obj_set_style_radius(row,0,LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row,255,LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        row,
        lv_color_hex(theme_colors[active_theme_index][11]),
        LV_PART_MAIN
    );
    lv_obj_set_style_text_color(
        row,
        lv_color_hex(theme_colors[active_theme_index][16]),
        LV_PART_MAIN
    );

    // Diagnostic focused visual: keep LV_STATE_FOCUSED, but change only paint properties.
    // No size/padding/layout/scroll properties are state-dependent.
    lv_obj_set_style_bg_opa(
        row,
        LV_OPA_COVER,
        ((lv_style_selector_t)LV_PART_MAIN | (lv_style_selector_t)LV_STATE_FOCUSED)
    );
    lv_obj_set_style_bg_color(
        row,
        lv_color_hex(theme_colors[active_theme_index][3]),
        ((lv_style_selector_t)LV_PART_MAIN | (lv_style_selector_t)LV_STATE_FOCUSED)
    );
    lv_obj_set_style_bg_color(
        row,
        lv_color_hex(theme_colors[active_theme_index][3]),
        ((lv_style_selector_t)LV_PART_MAIN | (lv_style_selector_t)LV_STATE_FOCUSED)
    );
    lv_obj_set_style_text_color(
        row,
        lv_color_hex(theme_colors[active_theme_index][9]),
        ((lv_style_selector_t)LV_PART_MAIN | (lv_style_selector_t)LV_STATE_FOCUSED)
    );
    lv_obj_set_style_text_font(row,&lv_font_montserrat_20,LV_PART_MAIN);
    lv_obj_add_flag(row,(lv_obj_flag_t)(LV_OBJ_FLAG_CHECKABLE|LV_OBJ_FLAG_CLICKABLE));
    lv_obj_add_event_cb(
        row,
        mixSourceMemberClicked,
        LV_EVENT_CLICKED,
        (void *)(uintptr_t)sourceIndex
    );

    lv_obj_t *label=lv_label_create(row);
    lv_label_set_text(label,name);
    lv_obj_center(label);

    return row;
}


static void formatMixActiveConditionLabelBase(
    const MixLine &mix,
    char *buffer,
    size_t size
){
    if(!buffer||size==0) return;

    if(mix.condition.type==ConditionType::NEVER){
        snprintf(buffer,size,"---");
        return;
    }

    if(mix.condition.type==ConditionType::ALWAYS||
       mix.condition.type==ConditionType::NONE){
        snprintf(buffer,size,"Always on");
        return;
    }

    if(mix.condition.type==ConditionType::SOURCE_POSITIVE&&
       mix.condition.sourceA.type==SourceType::LOGICAL_SWITCH){
        snprintf(
            buffer,
            size,
            "LS%u",
            (unsigned)(mix.condition.sourceA.index+1)
        );
        return;
    }

    if(mix.condition.type==ConditionType::FUNCTION_SWITCH){
        int16_t fsIndex=mix.condition.value1;
        if(fsIndex>=0&&fsIndex<FUNCTION_SWITCH_COUNT)
            snprintf(buffer,size,"FS%u",(unsigned)(fsIndex+1));
        else
            snprintf(buffer,size,"FS?");
        return;
    }

    if(mix.condition.type==ConditionType::TRIM_POSITION){
        switch((TrimDirection)mix.condition.value1){
            case TrimDirection::T4_LEFT:  snprintf(buffer,size,"T4 Left");  return;
            case TrimDirection::T4_RIGHT: snprintf(buffer,size,"T4 Right"); return;
            case TrimDirection::T3_DOWN:  snprintf(buffer,size,"T3 Down");  return;
            case TrimDirection::T3_UP:    snprintf(buffer,size,"T3 Up");    return;
            case TrimDirection::T2_DOWN:  snprintf(buffer,size,"T2 Down");  return;
            case TrimDirection::T2_UP:    snprintf(buffer,size,"T2 Up");    return;
            case TrimDirection::T1_LEFT:  snprintf(buffer,size,"T1 Left");  return;
            case TrimDirection::T1_RIGHT: snprintf(buffer,size,"T1 Right"); return;
            default:                      snprintf(buffer,size,"Trim ?");    return;
        }
    }

    if(mix.condition.type==ConditionType::FLIGHT_MODE){
        int16_t modeIndex=mix.condition.value1;

        if(modeIndex==0){
            snprintf(buffer,size,"Default flight mode");
            return;
        }

        if(modeIndex>0&&modeIndex<MAX_FLIGHT_MODES){
            ModelConfig &model=modelManager.getActiveModel();
            const FlightModeConfig &mode=model.flightModes[(uint8_t)modeIndex];

            if(mode.name[0]!='\0')
                snprintf(buffer,size,"%s",mode.name);
            else
                snprintf(buffer,size,"FM%u",(unsigned)(modeIndex+1));

            return;
        }

        snprintf(buffer,size,"Flight mode ?");
        return;
    }

    if(mix.condition.type==ConditionType::SYSTEM_EVENT){
        switch((SystemEventId)mix.condition.value1){
            case SystemEventId::THROTTLE_HOLD:         snprintf(buffer,size,"Throttle hold"); return;
            case SystemEventId::THROTTLE_CUT:          snprintf(buffer,size,"Throttle cut"); return;
            case SystemEventId::THROTTLE_ACTIVE:       snprintf(buffer,size,"Throttle active"); return;
            case SystemEventId::TELEMETRY_ACTIVE:      snprintf(buffer,size,"Telemetry active"); return;
            case SystemEventId::RSSI_LOW:              snprintf(buffer,size,"Rssi Low"); return;
            case SystemEventId::TRAINER_ACTIVE:        snprintf(buffer,size,"Trainer active"); return;
            case SystemEventId::FLIGHT_RESET:          snprintf(buffer,size,"Flight reset"); return;
            case SystemEventId::THROTTLE_IN_DEAD_BAND: snprintf(buffer,size,"Throttle in dead band"); return;
            default:                                   snprintf(buffer,size,"System event ?"); return;
        }
    }

    // Use the configured hardware switch name instead of mapping its slot to SA, SB, ... .
    // A switch slot may be renamed independently of its physical number.
    if(mix.condition.type==ConditionType::SOURCE_EQUAL&&
       mix.condition.sourceA.type==SourceType::SWITCH_INPUT){
        char switchName[48];
        formatMixSourceName(mix.condition.sourceA,switchName,sizeof(switchName));
        const char *position=mix.condition.value1>0?"↑":(mix.condition.value1<0?"↓":"-");
        snprintf(buffer,size,"%s%s",switchName,position);
        return;
    }

    formatMixConditionName(mix.condition,buffer,size);
}

static void formatMixActiveConditionLabel(
    const MixLine &mix,
    char *buffer,
    size_t size
){
    if(!buffer||size==0) return;

    formatMixActiveConditionLabelBase(mix,buffer,size);

    // ETHOS displays the inversion explicitly.
    // "Always on" + Invert is shown as "!Always on".
    // It is logically equivalent to "---", but the UI keeps the selected condition visible.
    if(!mix.condition.inverted) return;
    if(mix.condition.type==ConditionType::NEVER||mix.condition.type==ConditionType::NONE) return;

    char original[64];
    snprintf(original,sizeof(original),"%s",buffer);
    snprintf(buffer,size,"!%s",original);
}

static void updateMixActiveConditionEditorUi(){
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
    lv_obj_t *conditionLabel=isAileronMixEditor()?objects.aileron_mix_ac_lbl:objects.mix_ac_lbl;
    if(!conditionLabel) return;

    ModelConfig &model=modelManager.getActiveModel();
    const MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    char label[64];
    formatMixActiveConditionLabel(mix,label,sizeof(label));
    setConditionText(conditionLabel,label);
}

static bool isActionDirectionCondition(const ConditionConfig &condition){
    if(condition.value2!=ACTION_DIRECTION_MARKER) return false;
    return condition.type==ConditionType::SOURCE_POSITIVE||
           condition.type==ConditionType::SOURCE_NEGATIVE;
}

static const char *getActionDirectionLabel(const ConditionConfig &condition){
    if(!isActionDirectionCondition(condition)) return nullptr;

    switch((ActionDirection)condition.value1){
        case ActionDirection::RIGHT:return "Right";
        case ActionDirection::LEFT:return "Left";
        case ActionDirection::TOP:return "Top";
        case ActionDirection::BOTTOM:return "Bottom";
        default:return "Direction ?";
    }
}

static lv_obj_t *getMixActionConditionLabel(){
    return objects.mix_action_ac_lbl;
}

static void updateMixActionConditionEditorUi(){
    lv_obj_t *labelObj=getMixActionConditionLabel();
    if(!labelObj) return;

    char label[64];
    const char *direction=getActionDirectionLabel(mixActionEditorCondition);

    if(direction){
        if(mixActionEditorCondition.inverted)
            snprintf(label,sizeof(label),"!%s",direction);
        else
            snprintf(label,sizeof(label),"%s",direction);
    }else{
        MixLine temporaryMix{};
        temporaryMix.condition=mixActionEditorCondition;
        formatMixActiveConditionLabel(temporaryMix,label,sizeof(label));
    }

    setConditionText(labelObj,label);
}

static void updateMixActionVariantConditionEditorUi(MixActionType type,uint8_t slot){
    if(slot==0||slot>=MAX_MIX_ACTION_VARIANTS) return;

    lv_obj_t *panel=getMixActionVariantConditionPanel(type,slot);
    if(!panel||lv_obj_get_child_count(panel)==0) return;

    lv_obj_t *labelObj=lv_obj_get_child(panel,0);
    if(!labelObj) return;

    char label[64];
    const ConditionConfig &condition=isAileronMixEditor()?(type==MixActionType::WEIGHT?aileronEditorState.weightConditions[slot]:aileronEditorState.curveConditions[slot]):mixActionVariantConditions[slot];
    const char *direction=getActionDirectionLabel(condition);

    if(direction){
        if(condition.inverted) snprintf(label,sizeof(label),"!%s",direction);
        else snprintf(label,sizeof(label),"%s",direction);
    }else{
        MixLine temporaryMix{};
        temporaryMix.condition=condition;
        formatMixActiveConditionLabel(temporaryMix,label,sizeof(label));
    }

    setConditionText(labelObj,label);
}

static ConditionConfig *getSharedConditionTarget(){
    if(sharedSelectorMode==SharedSelectorMode::ACTION_ACTIVE_CONDITION)
        return &mixActionEditorCondition;

    if(sharedSelectorMode==SharedSelectorMode::VARIANT_ACTIVE_CONDITION){
        if(activeMixVariantConditionSlot==0||
           activeMixVariantConditionSlot>=MAX_MIX_ACTION_VARIANTS)
            return nullptr;
        if(isAileronMixEditor()){
            if(activeMixVariantConditionType==MixActionType::WEIGHT) return &aileronEditorState.weightConditions[activeMixVariantConditionSlot];
            if(activeMixVariantConditionType==MixActionType::CURVE) return &aileronEditorState.curveConditions[activeMixVariantConditionSlot];
        }
        return &mixActionVariantConditions[activeMixVariantConditionSlot];
    }

    if(sharedSelectorMode==SharedSelectorMode::ACTIVE_CONDITION){
        if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return nullptr;
        ModelConfig &model=modelManager.getActiveModel();
        return &model.mixes[(uint8_t)editingMixIndex].condition;
    }

    return nullptr;
}

static void updateSharedConditionTargetUi(){
    if(sharedSelectorMode==SharedSelectorMode::ACTION_ACTIVE_CONDITION)
        updateMixActionConditionEditorUi();
    else if(sharedSelectorMode==SharedSelectorMode::VARIANT_ACTIVE_CONDITION)
        updateMixActionVariantConditionEditorUi(
            activeMixVariantConditionType,
            activeMixVariantConditionSlot
        );
    else if(sharedSelectorMode==SharedSelectorMode::ACTIVE_CONDITION)
        updateMixActiveConditionEditorUi();

    if(isAileronMixEditor()) saveAileronEditorState();
    mixer.resetRuntime();
    if(editingMixIndex>=0)
        drawMixGraph((uint8_t)editingMixIndex);
}

static void closeSharedConditionSelector(){
    lv_obj_t *root=getMixSourceSelectorRoot();
    if(root) lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);
    hidePopupDismissOverlay();
}

static void setSharedConditionAlways(){
    ConditionConfig *condition=getSharedConditionTarget();
    if(!condition) return;
    *condition=ConditionConfig{};
    condition->type=ConditionType::ALWAYS;
    updateSharedConditionTargetUi();
}

static void setSharedConditionNever(){
    ConditionConfig *condition=getSharedConditionTarget();
    if(!condition) return;
    *condition=ConditionConfig{};
    condition->type=ConditionType::NEVER;
    updateSharedConditionTargetUi();
}

static void setMixActiveConditionAlways(){
    setSharedConditionAlways();
}

static void setMixActiveConditionNever(){
    setSharedConditionNever();
}

static lv_obj_t *createActiveConditionSwitchStubRow(
    const char *name,
    uint8_t stubIndex
){
    if(!objects.members_panel||!name) return nullptr;

    lv_obj_t *row=lv_obj_create(objects.members_panel);
    lv_obj_set_size(row,LV_PCT(95),40);
    lv_obj_set_style_pad_left(row,0,LV_PART_MAIN);
    lv_obj_set_style_pad_top(row,0,LV_PART_MAIN);
    lv_obj_set_style_pad_right(row,0,LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(row,0,LV_PART_MAIN);
    lv_obj_set_style_border_width(row,0,LV_PART_MAIN);
    lv_obj_set_style_radius(row,0,LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row,255,LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        row,
        lv_color_hex(theme_colors[active_theme_index][11]),
        LV_PART_MAIN
    );
    lv_obj_set_style_text_color(
        row,
        lv_color_hex(theme_colors[active_theme_index][16]),
        LV_PART_MAIN
    );
    lv_obj_set_style_bg_color(
        row,
        lv_color_hex(theme_colors[active_theme_index][3]),
        static_cast<lv_style_selector_t>(
            static_cast<uint32_t>(LV_PART_MAIN) |
            static_cast<uint32_t>(LV_STATE_FOCUSED)
        )
    );
    lv_obj_set_style_text_color(
        row,
        lv_color_hex(theme_colors[active_theme_index][9]),
        static_cast<lv_style_selector_t>(
            static_cast<uint32_t>(LV_PART_MAIN) |
            static_cast<uint32_t>(LV_STATE_FOCUSED)
        )
    );
    lv_obj_set_style_text_font(row,&lv_font_montserrat_20,LV_PART_MAIN);
    lv_obj_add_flag(
        row,
        static_cast<lv_obj_flag_t>(
            LV_OBJ_FLAG_CHECKABLE |
            LV_OBJ_FLAG_CLICKABLE
        )
    );
    lv_obj_add_event_cb(
        row,
        activeConditionSwitchStubClicked,
        LV_EVENT_CLICKED,
        (void *)(uintptr_t)stubIndex
    );

    lv_obj_t *label=lv_label_create(row);
    setConditionText(label,name);
    lv_obj_center(label);

    return row;
}


struct ActiveConditionStubEntry{
    const char *label;
};

static lv_obj_t *createActiveConditionGenericStubRow(
    const char *name,
    uintptr_t userData
){
    if(!objects.members_panel||!name) return nullptr;

    lv_obj_t *row=lv_obj_create(objects.members_panel);
    lv_obj_set_size(row,LV_PCT(95),40);
    lv_obj_set_style_pad_left(row,0,LV_PART_MAIN);
    lv_obj_set_style_pad_top(row,0,LV_PART_MAIN);
    lv_obj_set_style_pad_right(row,0,LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(row,0,LV_PART_MAIN);
    lv_obj_set_style_border_width(row,0,LV_PART_MAIN);
    lv_obj_set_style_radius(row,0,LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row,255,LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        row,
        lv_color_hex(theme_colors[active_theme_index][11]),
        LV_PART_MAIN
    );
    lv_obj_set_style_text_color(
        row,
        lv_color_hex(theme_colors[active_theme_index][16]),
        LV_PART_MAIN
    );
    lv_obj_set_style_bg_color(
        row,
        lv_color_hex(theme_colors[active_theme_index][3]),
        static_cast<lv_style_selector_t>(
            static_cast<uint32_t>(LV_PART_MAIN) |
            static_cast<uint32_t>(LV_STATE_FOCUSED)
        )
    );
    lv_obj_set_style_text_color(
        row,
        lv_color_hex(theme_colors[active_theme_index][9]),
        static_cast<lv_style_selector_t>(
            static_cast<uint32_t>(LV_PART_MAIN) |
            static_cast<uint32_t>(LV_STATE_FOCUSED)
        )
    );
    lv_obj_set_style_text_font(row,&lv_font_montserrat_20,LV_PART_MAIN);
    lv_obj_add_flag(
        row,
        static_cast<lv_obj_flag_t>(
            LV_OBJ_FLAG_CHECKABLE |
            LV_OBJ_FLAG_CLICKABLE
        )
    );
    lv_obj_add_event_cb(
        row,
        activeConditionGenericStubClicked,
        LV_EVENT_CLICKED,
        (void *)userData
    );

    lv_obj_t *label=lv_label_create(row);
    setConditionText(label,name);
    lv_obj_align(label,LV_ALIGN_LEFT_MID,12,0);

    return row;
}

static void selectActiveConditionStubLabel(const char *label){
    if(!label) return;

    if(objects.mix_ac_lbl)
        setConditionText(objects.mix_ac_lbl,label);

    lv_obj_t *root=getMixSourceSelectorRoot();
    if(root)
        lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);

    hidePopupDismissOverlay();
}

static void activeConditionGenericStubClicked(lv_event_t *e){
    if(!e) return;
    if(!sharedSelectorIsConditionMode()) return;

    lv_obj_t *target=(lv_obj_t *)lv_event_get_current_target(e);
    if(!target) return;

    lv_obj_t *label=lv_obj_get_child(target,0);
    if(!label) return;

    selectActiveConditionStubLabel(lv_label_get_text(label));
}

static void buildActiveConditionFunctionSwitchStubMembers(){
    clearMixSourceMembers();

    for(uint8_t i=0;i<FUNCTION_SWITCH_COUNT;i++){
        char label[8];
        snprintf(label,sizeof(label),"FS%u",(unsigned)(i+1));

        lv_obj_t *row=createActiveConditionGenericStubRow(label,i);
        if(!row) continue;

        lv_obj_remove_event_cb(row,activeConditionGenericStubClicked);
        lv_obj_add_event_cb(
            row,
            activeConditionFunctionSwitchClicked,
            LV_EVENT_CLICKED,
            (void *)(uintptr_t)i
        );
    }
}

static void buildActiveConditionLogicSwitchStubMembers(){
    clearMixSourceMembers();

    ModelConfig &model=modelManager.getActiveModel();
    bool any=false;

    for(uint8_t i=0;i<MAX_LOGICAL_SWITCHES;i++){
        if(!model.logicalSwitches[i].enabled) continue;

        char label[24];
        snprintf(label,sizeof(label),"LS%u",(unsigned)(i+1));

        lv_obj_t *row=createActiveConditionGenericStubRow(label,i);
        if(!row) continue;

        // Generic rows normally use the UI-only stub callback.
        // Logic switches are real backend conditions, so replace it.
        lv_obj_remove_event_cb(row,activeConditionGenericStubClicked);
        lv_obj_add_event_cb(
            row,
            activeConditionLogicSwitchClicked,
            LV_EVENT_CLICKED,
            (void *)(uintptr_t)i
        );

        any=true;
    }

    // ETHOS exposes this item even if no logic switches exist yet.
    // The logic-switch editor itself is not wired from this popup yet.
    lv_obj_t *addRow=createActiveConditionGenericStubRow(
        "+ Add a new logic switch",
        MAX_LOGICAL_SWITCHES
    );

    if(addRow){
        // Keep "+ Add..." UI-only for now.
        // It intentionally does not alter the Mix condition.
    }

    (void)any;
}

static void buildActiveConditionTrimStubMembers(){
    clearMixSourceMembers();

    struct TrimUiEntry{
        const char *label;
        TrimDirection direction;
    };

    static const TrimUiEntry items[]={
        {"T4 Left", TrimDirection::T4_LEFT},
        {"T4 Right",TrimDirection::T4_RIGHT},
        {"T3 Down", TrimDirection::T3_DOWN},
        {"T3 Up",   TrimDirection::T3_UP},
        {"T2 Down", TrimDirection::T2_DOWN},
        {"T2 Up",   TrimDirection::T2_UP},
        {"T1 Left", TrimDirection::T1_LEFT},
        {"T1 Right",TrimDirection::T1_RIGHT}
    };

    for(uint8_t i=0;i<sizeof(items)/sizeof(items[0]);i++){
        lv_obj_t *row=createActiveConditionGenericStubRow(
            items[i].label,
            (uint8_t)items[i].direction
        );

        if(!row) continue;

        lv_obj_remove_event_cb(row,activeConditionGenericStubClicked);
        lv_obj_add_event_cb(
            row,
            activeConditionTrimClicked,
            LV_EVENT_CLICKED,
            (void *)(uintptr_t)items[i].direction
        );
    }
}

static void buildActiveConditionFlightModeStubMembers(){
    clearMixSourceMembers();

    ModelConfig &model=modelManager.getActiveModel();

    // Default flight mode always exists as index 0.
    lv_obj_t *defaultRow=createActiveConditionGenericStubRow(
        "Default flight mode",
        0
    );

    if(defaultRow){
        lv_obj_remove_event_cb(defaultRow,activeConditionGenericStubClicked);
        lv_obj_add_event_cb(
            defaultRow,
            activeConditionFlightModeClicked,
            LV_EVENT_CLICKED,
            (void *)(uintptr_t)0
        );
    }

    // User-defined flight modes start at index 1.
    for(uint8_t i=1;i<MAX_FLIGHT_MODES;i++){
        const FlightModeConfig &mode=model.flightModes[i];
        if(!mode.enabled) continue;

        char label[32];

        if(mode.name[0]!='\0')
            snprintf(label,sizeof(label),"%s",mode.name);
        else
            snprintf(label,sizeof(label),"FM%u",(unsigned)(i+1));

        lv_obj_t *row=createActiveConditionGenericStubRow(label,i);
        if(!row) continue;

        lv_obj_remove_event_cb(row,activeConditionGenericStubClicked);
        lv_obj_add_event_cb(
            row,
            activeConditionFlightModeClicked,
            LV_EVENT_CLICKED,
            (void *)(uintptr_t)i
        );
    }
}

static void activeConditionSystemEventClicked(lv_event_t *e){
    if(!e) return;
    if(!sharedSelectorIsConditionMode()) return;

    uint8_t eventId=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(eventId>=(uint8_t)SystemEventId::COUNT) return;

    ConditionConfig *condition=getSharedConditionTarget();
    if(!condition) return;

    *condition=ConditionConfig{};
    condition->type=ConditionType::SYSTEM_EVENT;
    condition->value1=(int16_t)eventId;

    updateSharedConditionTargetUi();
    closeSharedConditionSelector();
}

static void buildActiveConditionSystemEventStubMembers(){
    clearMixSourceMembers();

    struct SystemEventUiEntry{const char *label;SystemEventId event;};
    static const SystemEventUiEntry items[]={
        {"Throttle hold",SystemEventId::THROTTLE_HOLD},
        {"Throttle cut",SystemEventId::THROTTLE_CUT},
        {"Throttle active",SystemEventId::THROTTLE_ACTIVE},
        {"Telemetry active",SystemEventId::TELEMETRY_ACTIVE},
        {"Rssi Low",SystemEventId::RSSI_LOW},
        {"Trainer active",SystemEventId::TRAINER_ACTIVE},
        {"Flight reset",SystemEventId::FLIGHT_RESET},
        {"Throttle in dead band",SystemEventId::THROTTLE_IN_DEAD_BAND}
    };

    for(uint8_t i=0;i<sizeof(items)/sizeof(items[0]);i++){
        lv_obj_t *row=createActiveConditionGenericStubRow(items[i].label,(uint8_t)items[i].event);
        if(!row) continue;
        lv_obj_remove_event_cb(row,activeConditionGenericStubClicked);
        lv_obj_add_event_cb(row,activeConditionSystemEventClicked,LV_EVENT_CLICKED,(void *)(uintptr_t)items[i].event);
    }
}

static void activeConditionFunctionSwitchClicked(lv_event_t *e){
    if(!e) return;
    if(!sharedSelectorIsConditionMode()) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    uint8_t fsIndex=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(fsIndex>=FUNCTION_SWITCH_COUNT) return;

    ConditionConfig *condition=getSharedConditionTarget();
    if(!condition) return;

    *condition=ConditionConfig{};
    condition->type=ConditionType::FUNCTION_SWITCH;
    condition->value1=(int16_t)fsIndex;

    updateSharedConditionTargetUi();

    lv_obj_t *root=getMixSourceSelectorRoot();
    if(root)
        lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);

    hidePopupDismissOverlay();
}

static void activeConditionFunctionSwitchCategoryClicked(lv_event_t *e){
    (void)e;
    if(!sharedSelectorIsConditionMode()) return;
    buildActiveConditionFunctionSwitchStubMembers();
}

static void activeConditionLogicSwitchClicked(lv_event_t *e){
    if(!e) return;
    if(!sharedSelectorIsConditionMode()) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    uint8_t logicIndex=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(logicIndex>=MAX_LOGICAL_SWITCHES) return;

    ModelConfig &model=modelManager.getActiveModel();

    if(!model.logicalSwitches[logicIndex].enabled)
        return;

    ConditionConfig *condition=getSharedConditionTarget();
    if(!condition) return;

    *condition=ConditionConfig{};
    condition->type=ConditionType::SOURCE_POSITIVE;
    condition->sourceA=Sources::logicalSwitch(logicIndex);

    updateSharedConditionTargetUi();

    lv_obj_t *root=getMixSourceSelectorRoot();
    if(root)
        lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);

    hidePopupDismissOverlay();
}

static void activeConditionLogicSwitchCategoryClicked(lv_event_t *e){
    (void)e;

    if(sharedSelectorMode==SharedSelectorMode::SOURCE){
        buildMixLogicSourceMembers();
        return;
    }

    if(!sharedSelectorIsConditionMode()) return;
    buildActiveConditionLogicSwitchStubMembers();
}

static void activeConditionTrimClicked(lv_event_t *e){
    if(!e) return;
    if(!sharedSelectorIsConditionMode()) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    uint8_t direction=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(direction>=(uint8_t)TrimDirection::COUNT) return;

    ConditionConfig *condition=getSharedConditionTarget();
    if(!condition) return;

    *condition=ConditionConfig{};
    condition->type=ConditionType::TRIM_POSITION;
    condition->value1=(int16_t)direction;

    updateSharedConditionTargetUi();

    lv_obj_t *root=getMixSourceSelectorRoot();
    if(root)
        lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);

    hidePopupDismissOverlay();
}

static void activeConditionTrimCategoryClicked(lv_event_t *e){
    (void)e;
    if(!sharedSelectorIsConditionMode()) return;
    buildActiveConditionTrimStubMembers();
}

static void activeConditionFlightModeClicked(lv_event_t *e){
    if(!e) return;
    if(!sharedSelectorIsConditionMode()) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    uint8_t modeIndex=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(modeIndex>=MAX_FLIGHT_MODES) return;

    ModelConfig &model=modelManager.getActiveModel();

    if(modeIndex>0&&!model.flightModes[modeIndex].enabled)
        return;

    ConditionConfig *condition=getSharedConditionTarget();
    if(!condition) return;

    *condition=ConditionConfig{};
    condition->type=ConditionType::FLIGHT_MODE;
    condition->value1=(int16_t)modeIndex;

    updateSharedConditionTargetUi();

    lv_obj_t *root=getMixSourceSelectorRoot();
    if(root)
        lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);

    hidePopupDismissOverlay();
}

static void activeConditionFlightModeCategoryClicked(lv_event_t *e){
    (void)e;
    if(!sharedSelectorIsConditionMode()) return;
    buildActiveConditionFlightModeStubMembers();
}

static bool mixSourceUsesHorizontalDirection(const MixLine &mix){
    if(mix.source.type!=SourceType::STICK_FUNCTION) return false;

    StickFunction function=static_cast<StickFunction>(mix.source.index);
    return function==StickFunction::RUDDER||
           function==StickFunction::AILERON;
}

static void syncMixDirectionConditionsToSource(MixLine &mix){
    uint8_t actionCount=mix.actionCount;
    if(actionCount>MAX_MIX_ACTIONS) actionCount=MAX_MIX_ACTIONS;

    for(uint8_t actionIndex=0;actionIndex<actionCount;actionIndex++){
        MixAction &action=mix.actions[actionIndex];
        if(!action.enabled) continue;

        if(action.condition.value2==ACTION_DIRECTION_MARKER&&
           (action.condition.type==ConditionType::SOURCE_POSITIVE||
            action.condition.type==ConditionType::SOURCE_NEGATIVE))
            action.condition.sourceA=getMixBaseSource(mix);

        uint8_t variantCount=action.variantCount;
        if(variantCount>MAX_MIX_ACTION_VARIANTS) variantCount=MAX_MIX_ACTION_VARIANTS;

        for(uint8_t variantIndex=1;variantIndex<variantCount;variantIndex++){
            MixActionVariant &variant=action.variants[variantIndex];
            if(!variant.enabled) continue;

            if(variant.condition.value2==ACTION_DIRECTION_MARKER&&
               (variant.condition.type==ConditionType::SOURCE_POSITIVE||
                variant.condition.type==ConditionType::SOURCE_NEGATIVE))
                variant.condition.sourceA=getMixBaseSource(mix);
        }
    }
}

static void activeConditionDirectionClicked(lv_event_t *e){
    if(!e) return;
    if(sharedSelectorMode!=SharedSelectorMode::ACTION_ACTIVE_CONDITION&&
   sharedSelectorMode!=SharedSelectorMode::VARIANT_ACTIVE_CONDITION) return;

    uint8_t raw=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(raw>(uint8_t)ActionDirection::BOTTOM) return;

    ActionDirection direction=(ActionDirection)raw;
    ConditionConfig *condition=getSharedConditionTarget();
    if(!condition) return;

    *condition=ConditionConfig{};
    condition->value1=(int16_t)raw;
    condition->value2=ACTION_DIRECTION_MARKER;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];
    condition->sourceA=getMixBaseSource(mix);

    if(direction==ActionDirection::RIGHT||direction==ActionDirection::TOP)
        condition->type=ConditionType::SOURCE_POSITIVE;
    else
        condition->type=ConditionType::SOURCE_NEGATIVE;

    updateSharedConditionTargetUi();
    closeSharedConditionSelector();
}

static void buildActiveConditionDirectionMembers(){
    clearMixSourceMembers();

    struct DirectionUiEntry{const char *label;ActionDirection direction;};
    static const DirectionUiEntry items[]={
        {"right",ActionDirection::RIGHT},
        {"left",ActionDirection::LEFT},
        {"Top",ActionDirection::TOP},
        {"Bottom",ActionDirection::BOTTOM}
    };

    for(uint8_t i=0;i<sizeof(items)/sizeof(items[0]);i++){
        lv_obj_t *row=createActiveConditionGenericStubRow(
            items[i].label,
            (uint8_t)items[i].direction
        );
        if(!row) continue;

        lv_obj_remove_event_cb(row,activeConditionGenericStubClicked);
        lv_obj_add_event_cb(
            row,
            activeConditionDirectionClicked,
            LV_EVENT_CLICKED,
            (void *)(uintptr_t)items[i].direction
        );
    }
}

static void focusCurrentActionDirectionSelection(){
    if(sharedSelectorMode!=SharedSelectorMode::ACTION_ACTIVE_CONDITION) return;
    if(!isActionDirectionCondition(mixActionEditorCondition)) return;

    if(objects.category_direction)
        lv_obj_add_state(objects.category_direction,LV_STATE_FOCUSED);

    buildActiveConditionDirectionMembers();

    uint8_t raw=(uint8_t)mixActionEditorCondition.value1;

    int32_t childIndex=-1;
    switch((ActionDirection)raw){
        case ActionDirection::RIGHT:  childIndex=1; break;
        case ActionDirection::LEFT:   childIndex=2; break;
        case ActionDirection::TOP:    childIndex=3; break;
        case ActionDirection::BOTTOM: childIndex=4; break;
        default: return;
    }

    if(!objects.members_panel) return;
    if(childIndex>=(int32_t)lv_obj_get_child_count(objects.members_panel)) return;

    lv_obj_t *row=lv_obj_get_child(objects.members_panel,childIndex);
    if(row) lv_obj_add_state(row,LV_STATE_FOCUSED);
}

static void activeConditionDirectionCategoryClicked(lv_event_t *e){
    (void)e;
    if(sharedSelectorMode!=SharedSelectorMode::ACTION_ACTIVE_CONDITION&&
   sharedSelectorMode!=SharedSelectorMode::VARIANT_ACTIVE_CONDITION) return;
    buildActiveConditionDirectionMembers();
}

static void activeConditionSystemEventCategoryClicked(lv_event_t *e){
    (void)e;
    if(!sharedSelectorIsConditionMode()) return;
    buildActiveConditionSystemEventStubMembers();
}

// Hardware switch metadata is shared by Mixes and condition selectors.
// Preferences namespace/key format is reserved for the scr_hw editor integration.
static uint8_t hardwareSwitchType(uint8_t slot){
    if(slot>=14) return 0;
    Preferences prefs;
    if(!prefs.begin("hwsettings",true)) return slot<6?2:1;
    char key[16];snprintf(key,sizeof(key),"swtype%u",(unsigned)(slot+1));
    uint8_t type=prefs.getUChar(key,slot<6?2:1);
    prefs.end();
    return type<=3?type:0;
}
static void hardwareSwitchName(uint8_t slot,char *out,size_t size){
    if(!out||!size) return;
    snprintf(out,size,"S%c",'A'+slot);
    if(slot>=14) return;
    Preferences prefs;
    if(!prefs.begin("hwsettings",true)) return;
    char key[16];snprintf(key,sizeof(key),"swname%u",(unsigned)(slot+1));
    String value=prefs.getString(key,"");
    prefs.end();
    value.trim();
    if(value.length()) snprintf(out,size,"%s",value.c_str());
}
static SwitchInput hardwareSwitchInput(uint8_t slot){
    return static_cast<SwitchInput>(static_cast<uint8_t>(SwitchInput::SW_3POS_1)+slot);
}
static constexpr uint8_t HARDWARE_SUPPORTED_SWITCHES=14; // All configured Hardware switch rows have stable SourceId indices.

static void buildActiveConditionSwitchStubMembers(){
    clearMixSourceMembers();
    for(uint8_t slot=0;slot<HARDWARE_SUPPORTED_SWITCHES;slot++){
        uint8_t type=hardwareSwitchType(slot);
        if(type==0) continue;
        char name[48];hardwareSwitchName(slot,name,sizeof(name));
        auto addRow=[&](const char *positionText,uint8_t position){
            char label[64];snprintf(label,sizeof(label),"%s %s",name,positionText);
            uint16_t packed=((uint16_t)static_cast<uint8_t>(hardwareSwitchInput(slot))<<8)|position;
            createActiveConditionSwitchStubRow(label,0);
            lv_obj_t *row=lv_obj_get_child(objects.members_panel,lv_obj_get_child_count(objects.members_panel)-1);
            if(row){
                lv_obj_remove_event_cb(row,activeConditionSwitchStubClicked);
                lv_obj_add_event_cb(row,activeConditionSwitchStubClicked,LV_EVENT_CLICKED,(void *)(uintptr_t)packed);
            }
        };
        addRow("↑",0);
        if(type==2) addRow("-",1);
        addRow("↓",2);
    }
}

static void activeConditionSwitchCategoryClicked(lv_event_t *e){
    (void)e;

    if(!sharedSelectorIsConditionMode())
        return;

    buildActiveConditionSwitchStubMembers();
}

static void activeConditionSwitchStubClicked(lv_event_t *e){
    if(!e) return;
    if(!sharedSelectorIsConditionMode()) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    uint16_t packed=(uint16_t)(uintptr_t)lv_event_get_user_data(e);
    uint8_t switchRaw=(uint8_t)(packed>>8);
    uint8_t position=(uint8_t)(packed&0xFF);

    if(switchRaw>=static_cast<uint8_t>(SwitchInput::COUNT))
        return;

    int16_t value=0;

    // MCP23017: first contact (Up) is +1000, second contact (Down) is -1000.
    if(position==0)
        value=1000;
    else if(position==1)
        value=0;
    else if(position==2)
        value=-1000;
    else
        return;

    ConditionConfig *condition=getSharedConditionTarget();
    if(!condition) return;

    *condition=ConditionConfig{};
    condition->type=ConditionType::SOURCE_EQUAL;
    condition->sourceA=Sources::switchInput(
        static_cast<SwitchInput>(switchRaw)
    );
    condition->value1=value;
    condition->tolerance=100;

    updateSharedConditionTargetUi();

    lv_obj_t *root=getMixSourceSelectorRoot();
    if(root)
        lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);

    hidePopupDismissOverlay();
}

static void stopMixSourceStickFocusDetect(){
    mixSourceStickFocusDetectActive=false;
}

static void startMixSourceStickFocusDetect(){
    for(uint8_t i=0;i<static_cast<uint8_t>(StickFunction::COUNT);i++)
        mixSourceStickFocusBaseline[i]=inputManager.getStick(static_cast<StickFunction>(i));
    for(uint8_t i=0;i<static_cast<uint8_t>(AnalogInput::COUNT);i++)
        mixSourceAnalogFocusBaseline[i]=inputManager.getAnalog(static_cast<AnalogInput>(i));
    for(uint8_t slot=0;slot<HARDWARE_SUPPORTED_SWITCHES;slot++){
        int16_t value=inputManager.getSwitch(hardwareSwitchInput(slot));
        mixSourceSwitchFocusBaseline[slot]=value;
        mixSourceSwitchLastSample[slot]=value;
        mixSourceSwitchStableCount[slot]=0;
    }
    mixSourceStickFocusDetectActive=true;
}

static void focusSharedSelectorMember(uint32_t childIndex){
    if(!objects.members_panel) return;
    uint32_t count=lv_obj_get_child_count(objects.members_panel);
    if(childIndex==0||childIndex>=count) return;
    for(uint32_t i=1;i<count;i++){
        lv_obj_t *row=lv_obj_get_child(objects.members_panel,(int32_t)i);
        if(row) lv_obj_remove_state(row,LV_STATE_FOCUSED);
    }
    lv_obj_t *row=lv_obj_get_child(objects.members_panel,(int32_t)childIndex);
    if(!row) return;
    lv_obj_add_state(row,LV_STATE_FOCUSED);
    lv_obj_scroll_to_view(row,LV_ANIM_OFF);
}

static void focusSharedSelectorCategory(lv_obj_t *selected){
    if(!objects.category_panel||!selected) return;
    uint32_t count=lv_obj_get_child_count(objects.category_panel);
    for(uint32_t i=0;i<count;i++){
        lv_obj_t *row=lv_obj_get_child(objects.category_panel,(int32_t)i);
        if(row) lv_obj_remove_state(row,LV_STATE_FOCUSED);
    }
    lv_obj_add_state(selected,LV_STATE_FOCUSED);
    lv_obj_scroll_to_view(selected,LV_ANIM_OFF);
}

static void focusMixAnalogMember(StickFunction function){
    uint8_t index=static_cast<uint8_t>(function);
    if(index>=static_cast<uint8_t>(StickFunction::COUNT)||!mixSourceAnalogMembersActive) return;
    focusSharedSelectorMember((uint32_t)index+1);
}

static void focusMixPhysicalAnalogMember(AnalogInput input){
    static const AnalogInput additionalInputs[]={
        AnalogInput::STICK_LZ,AnalogInput::STICK_RZ,
        AnalogInput::SLIDER_L,AnalogInput::SLIDER_R,
        AnalogInput::LINEAR_1,AnalogInput::LINEAR_2,
        AnalogInput::POT_1,AnalogInput::POT_2,
        AnalogInput::POT_3,AnalogInput::POT_4
    };
    uint32_t row=1+static_cast<uint8_t>(StickFunction::COUNT);
    for(AnalogInput item:additionalInputs){
        if(!inputManager.isAnalogEnabled(item)) continue;
        if(item==input){focusSharedSelectorMember(row);return;}
        row++;
    }
}

static void focusMixSwitchMember(uint8_t slot,int16_t value){
    if(slot>=HARDWARE_SUPPORTED_SWITCHES||hardwareSwitchType(slot)==0) return;
    bool condition=sharedSelectorIsConditionMode();
    if(condition){
        buildActiveConditionSwitchStubMembers();
        focusSharedSelectorCategory(objects.category_sp);
    }else{
        buildMixSwitchSourceMembers();
        focusSharedSelectorCategory(objects.category_switches);
    }
    uint32_t row=1;
    for(uint8_t i=0;i<slot;i++){
        uint8_t type=hardwareSwitchType(i);
        if(type==0) continue;
        row+=condition?(type==2?3:2):1;
    }
    if(condition){
        if(value==0&&hardwareSwitchType(slot)==2) row++;
        else if(value<0) row+=hardwareSwitchType(slot)==2?2:1;
    }
    focusSharedSelectorMember(row);
}

static void updateMixSourceStickFocusDetect(){
    if(!mixSourceStickFocusDetectActive) return;
    lv_obj_t *root=getMixSourceSelectorRoot();
    if(!root||lv_obj_has_flag(root,LV_OBJ_FLAG_HIDDEN)){
        stopMixSourceStickFocusDetect();
        return;
    }

    // Switch changes are monitored in both Source and Active Condition.
    for(uint8_t slot=0;slot<HARDWARE_SUPPORTED_SWITCHES;slot++){
        if(hardwareSwitchType(slot)==0) continue;
        int16_t current=inputManager.getSwitch(hardwareSwitchInput(slot));
        if(current!=mixSourceSwitchLastSample[slot]){
            mixSourceSwitchLastSample[slot]=current;
            mixSourceSwitchStableCount[slot]=0;
            continue;
        }
        if(mixSourceSwitchStableCount[slot]<MIX_SOURCE_SWITCH_DEBOUNCE_SAMPLES)
            mixSourceSwitchStableCount[slot]++;
        if(mixSourceSwitchStableCount[slot]<MIX_SOURCE_SWITCH_DEBOUNCE_SAMPLES) continue;
        if(current==mixSourceSwitchFocusBaseline[slot]) continue;
        mixSourceSwitchFocusBaseline[slot]=current;
        focusMixSwitchMember(slot,current);
        for(uint8_t i=0;i<static_cast<uint8_t>(StickFunction::COUNT);i++)
            mixSourceStickFocusBaseline[i]=inputManager.getStick(static_cast<StickFunction>(i));
        for(uint8_t i=0;i<static_cast<uint8_t>(AnalogInput::COUNT);i++)
            mixSourceAnalogFocusBaseline[i]=inputManager.getAnalog(static_cast<AnalogInput>(i));
        return;
    }

    // The Active Condition selector contains switch positions, not analog sources.
    if(sharedSelectorMode!=SharedSelectorMode::SOURCE) return;

    int32_t bestDelta=MIX_SOURCE_STICK_FOCUS_THRESHOLD-1;
    uint8_t bestStick=0xFF;
    uint8_t bestAnalog=0xFF;
    for(uint8_t i=0;i<static_cast<uint8_t>(StickFunction::COUNT);i++){
        int32_t delta=(int32_t)inputManager.getStick(static_cast<StickFunction>(i))-mixSourceStickFocusBaseline[i];
        if(delta<0) delta=-delta;
        if(delta>bestDelta){bestDelta=delta;bestStick=i;bestAnalog=0xFF;}
    }
    static const AnalogInput additionalInputs[]={
        AnalogInput::STICK_LZ,AnalogInput::STICK_RZ,
        AnalogInput::SLIDER_L,AnalogInput::SLIDER_R,
        AnalogInput::LINEAR_1,AnalogInput::LINEAR_2,
        AnalogInput::POT_1,AnalogInput::POT_2,
        AnalogInput::POT_3,AnalogInput::POT_4
    };
    for(AnalogInput input:additionalInputs){
        if(!inputManager.isAnalogEnabled(input)) continue;
        uint8_t index=static_cast<uint8_t>(input);
        int32_t delta=(int32_t)inputManager.getAnalog(input)-mixSourceAnalogFocusBaseline[index];
        if(delta<0) delta=-delta;
        if(delta>bestDelta){bestDelta=delta;bestAnalog=index;bestStick=0xFF;}
    }
    if(bestStick==0xFF&&bestAnalog==0xFF) return;

    if(!mixSourceAnalogMembersActive) buildMixAnalogMembers();
    focusSharedSelectorCategory(objects.category_analog);
    if(bestStick!=0xFF) focusMixAnalogMember(static_cast<StickFunction>(bestStick));
    else focusMixPhysicalAnalogMember(static_cast<AnalogInput>(bestAnalog));
    for(uint8_t i=0;i<static_cast<uint8_t>(StickFunction::COUNT);i++)
        mixSourceStickFocusBaseline[i]=inputManager.getStick(static_cast<StickFunction>(i));
    for(uint8_t i=0;i<static_cast<uint8_t>(AnalogInput::COUNT);i++)
        mixSourceAnalogFocusBaseline[i]=inputManager.getAnalog(static_cast<AnalogInput>(i));
}

static bool isMixSourceSelectorVisible(){
    lv_obj_t *root=getMixSourceSelectorRoot();
    if(!root) return false;
    return !lv_obj_has_flag(root,LV_OBJ_FLAG_HIDDEN);
}

static bool moveMixSourceMemberFocus(int8_t direction){
    if(!isMixSourceSelectorVisible()) return false;
    if(sharedSelectorMode!=SharedSelectorMode::SOURCE) return false;
    if(!objects.members_panel) return false;

    uint32_t childCount=lv_obj_get_child_count(objects.members_panel);
    if(childCount<=1) return false;

    int32_t focusedIndex=-1;

    // Child 0 is the static "Member" header.
    for(uint32_t i=1;i<childCount;i++){
        lv_obj_t *row=lv_obj_get_child(objects.members_panel,(int32_t)i);
        if(row&&lv_obj_has_state(row,LV_STATE_FOCUSED)){
            focusedIndex=(int32_t)i;
            break;
        }
    }

    int32_t targetIndex=focusedIndex;

    if(targetIndex<1)
        targetIndex=direction>=0?1:(int32_t)childCount-1;
    else if(direction>0&&targetIndex<(int32_t)childCount-1)
        targetIndex++;
    else if(direction<0&&targetIndex>1)
        targetIndex--;

    for(uint32_t i=1;i<childCount;i++){
        lv_obj_t *row=lv_obj_get_child(objects.members_panel,(int32_t)i);
        if(row) lv_obj_remove_state(row,LV_STATE_FOCUSED);
    }

    lv_obj_t *target=lv_obj_get_child(objects.members_panel,targetIndex);
    if(!target) return false;

    lv_obj_add_state(target,LV_STATE_FOCUSED);
    return true;
}

static bool pressFocusedMixSourceMember(){
    if(!isMixSourceSelectorVisible()) return false;
    if(sharedSelectorMode!=SharedSelectorMode::SOURCE) return false;
    if(!objects.members_panel) return false;

    uint32_t childCount=lv_obj_get_child_count(objects.members_panel);

    for(uint32_t i=1;i<childCount;i++){
        lv_obj_t *row=lv_obj_get_child(objects.members_panel,(int32_t)i);
        if(!row) continue;
        if(!lv_obj_has_state(row,LV_STATE_FOCUSED)) continue;

        lv_obj_send_event(row,LV_EVENT_CLICKED,nullptr);
        return true;
    }

    return false;
}

static void mixSourcePhysicalAnalogMemberClicked(lv_event_t *e){
    if(!e || sharedSelectorMode!=SharedSelectorMode::SOURCE) return;
    if(editingMixIndex<0 || editingMixIndex>=MAX_MIX_LINES) return;
    uint8_t index=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(index>=static_cast<uint8_t>(AnalogInput::COUNT)) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];
    mix.source=Sources::analog(static_cast<AnalogInput>(index));
    syncMixDirectionConditionsToSource(mix);
    updateMixSourceEditorUi();
    lv_obj_t *root=getMixSourceSelectorRoot();
    if(root) lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);
    hidePopupDismissOverlay();
    mixer.resetRuntime();
    drawMixGraph((uint8_t)editingMixIndex);
}

static void buildMixAnalogMembers(){
    clearMixSourceMembers();
    mixSourceAnalogMembersActive=true;

    // Сейчас реально работающие аналоговые органы в InputManager —
    // четыре основных функциональных стика. Используем StickFunction,
    // чтобы выбор не зависел от Mode 1/2/3/4.
    for(uint8_t i=0;i<static_cast<uint8_t>(StickFunction::COUNT);i++){
        SourceId source=Sources::stick(static_cast<StickFunction>(i));
        const char *name=sourceResolver.getName(source);
        createMixSourceMemberRow(name,i);
    }

    // Physical axes and controls beyond the four flight-stick functions.
    // SourceId::ANALOG_INPUT remains stable and is resolved by InputManager.
    static const AnalogInput additionalInputs[]={
        AnalogInput::STICK_LZ,AnalogInput::STICK_RZ,
        AnalogInput::SLIDER_L,AnalogInput::SLIDER_R,
        AnalogInput::LINEAR_1,AnalogInput::LINEAR_2,
        AnalogInput::POT_1,AnalogInput::POT_2,
        AnalogInput::POT_3,AnalogInput::POT_4
    };
    for(AnalogInput input:additionalInputs){
        if(!inputManager.isAnalogEnabled(input)) continue;
        SourceId source=Sources::analog(input);
        const char *name=sourceResolver.getName(source);
        uint8_t index=static_cast<uint8_t>(input);
        lv_obj_t *row=createMixSourceMemberRow(name,index);
        if(!row) continue;
        lv_obj_remove_event_cb(row,mixSourceMemberClicked);
        lv_obj_add_event_cb(row,mixSourcePhysicalAnalogMemberClicked,LV_EVENT_CLICKED,(void *)(uintptr_t)index);
    }
}

static void mixSourceCategoryNoneClicked(lv_event_t *e){
    (void)e;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    if(sharedSelectorIsConditionMode()){
        setSharedConditionNever();

        lv_obj_t *root=getMixSourceSelectorRoot();
        if(root) lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);
        hidePopupDismissOverlay();
        return;
    }

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    mix.source=SourceId{};
    syncMixDirectionConditionsToSource(mix);
    updateMixSourceEditorUi();

    lv_obj_t *root=getMixSourceSelectorRoot();
    if(root) lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);

    mixer.resetRuntime();
}

static void buildMixSwitchSourceMembers(){
    clearMixSourceMembers();
    for(uint8_t slot=0;slot<HARDWARE_SUPPORTED_SWITCHES;slot++){
        if(hardwareSwitchType(slot)==0) continue;
        char name[48];hardwareSwitchName(slot,name,sizeof(name));
        uint8_t input=static_cast<uint8_t>(hardwareSwitchInput(slot));
        lv_obj_t *row=createMixSourceMemberRow(name,input);
        if(!row) continue;
        lv_obj_remove_event_cb(row,mixSourceMemberClicked);
        lv_obj_add_event_cb(row,mixSourceSwitchMemberClicked,LV_EVENT_CLICKED,(void *)(uintptr_t)input);
    }
}

static void buildMixLogicSourceMembers(){
    clearMixSourceMembers();

    ModelConfig &model=modelManager.getActiveModel();

    for(uint8_t i=0;i<MAX_LOGICAL_SWITCHES;i++){
        if(!model.logicalSwitches[i].enabled) continue;

        char label[32];
        if(model.logicalSwitches[i].name[0]!='\0')
            snprintf(label,sizeof(label),"LS%u (%s)",(unsigned)(i+1),model.logicalSwitches[i].name);
        else
            snprintf(label,sizeof(label),"LS%u",(unsigned)(i+1));

        lv_obj_t *row=createMixSourceMemberRow(label,i);
        if(!row) continue;

        lv_obj_remove_event_cb(row,mixSourceMemberClicked);
        lv_obj_add_event_cb(
            row,
            mixSourceLogicMemberClicked,
            LV_EVENT_CLICKED,
            (void *)(uintptr_t)i
        );
    }
}

static void mixSourceSwitchCategoryClicked(lv_event_t *e){
    (void)e;
    if(sharedSelectorMode!=SharedSelectorMode::SOURCE) return;
    buildMixSwitchSourceMembers();
}

static void mixSourceSwitchMemberClicked(lv_event_t *e){
    if(!e) return;
    if(sharedSelectorMode!=SharedSelectorMode::SOURCE) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    uint8_t raw=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(raw>=static_cast<uint8_t>(SwitchInput::COUNT)) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    mix.source=Sources::switchInput(static_cast<SwitchInput>(raw));
    syncMixDirectionConditionsToSource(mix);
    updateMixSourceEditorUi();

    lv_obj_t *root=getMixSourceSelectorRoot();
    if(root) lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);

    hidePopupDismissOverlay();
    mixer.resetRuntime();
    drawMixGraph((uint8_t)editingMixIndex);
}

static void mixSourceLogicMemberClicked(lv_event_t *e){
    if(!e) return;
    if(sharedSelectorMode!=SharedSelectorMode::SOURCE) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    uint8_t index=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(index>=MAX_LOGICAL_SWITCHES) return;

    ModelConfig &model=modelManager.getActiveModel();
    if(!model.logicalSwitches[index].enabled) return;

    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    mix.source=Sources::logicalSwitch(index);
    syncMixDirectionConditionsToSource(mix);
    updateMixSourceEditorUi();

    lv_obj_t *root=getMixSourceSelectorRoot();
    if(root) lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);

    hidePopupDismissOverlay();
    mixer.resetRuntime();
    drawMixGraph((uint8_t)editingMixIndex);
}

static void mixSourceCategoryAnalogClicked(lv_event_t *e){
    (void)e;

    if(sharedSelectorIsConditionMode()){
        setSharedConditionAlways();

        lv_obj_t *root=getMixSourceSelectorRoot();
        if(root) lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);
        hidePopupDismissOverlay();
        return;
    }

    buildMixAnalogMembers();
}

static void mixSourceMemberClicked(lv_event_t *e){
    if(!e) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    uint8_t index=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(index>=static_cast<uint8_t>(StickFunction::COUNT)) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    mix.source=Sources::stick(static_cast<StickFunction>(index));
    syncMixDirectionConditionsToSource(mix);
    updateMixSourceEditorUi();

    lv_obj_t *root=getMixSourceSelectorRoot();
    if(root) lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);

    mixer.resetRuntime();
}


static void setSharedSelectorRowVisible(lv_obj_t *row,bool visible){
    if(!row) return;

    if(visible)
        lv_obj_remove_flag(row,LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(row,LV_OBJ_FLAG_HIDDEN);
}

static void setSharedSelectorRowText(lv_obj_t *row,const char *text){
    if(!row||!text) return;

    lv_obj_t *label=lv_obj_get_child(row,0);
    if(label)
        lv_label_set_text(label,text);
}

static void orderSharedSelectorRows(lv_obj_t *const *rows,uint8_t count){
    if(!objects.category_panel||!rows) return;

    // Keep the EEZ "Category" header permanently at the top.
    // It is the only direct child of category_panel that is not one
    // of the category rows.
    lv_obj_t *header=nullptr;
    uint32_t childCount=lv_obj_get_child_count(objects.category_panel);

    for(uint32_t childIndex=0;childIndex<childCount&&!header;childIndex++){
        lv_obj_t *child=lv_obj_get_child(objects.category_panel,(int32_t)childIndex);
        if(!child) continue;

        bool isCategoryRow=
            child==objects.category_none||
            child==objects.category_analog||
            child==objects.category_direction||
            child==objects.category_sp||
            child==objects.category_switches||
            child==objects.category_fs||
            child==objects.category_ls||
            child==objects.category_tp||
            child==objects.category_gyro||
            child==objects.category_trims||
            child==objects.category_channels||
            child==objects.category_vars||
            child==objects.category_trainer||
            child==objects.category_timers||
            child==objects.category_sv||
            child==objects.category_special||
            child==objects.category_fm||
            child==objects.category_se;

        if(!isCategoryRow)
            header=child;
    }

    if(header)
        lv_obj_move_to_index(header,0);

    for(uint8_t i=0;i<count;i++){
        lv_obj_t *row=rows[i];
        if(!row) continue;
        lv_obj_move_to_index(row,(int32_t)i+1);
    }

    lv_obj_update_layout(objects.category_panel);
}

static void configureSharedSelectorForSource(){
    sharedSelectorMode=SharedSelectorMode::SOURCE;

    clearMixSourceMembers();

    // ETHOS Source order: photo 3 + photo 4.
    setSharedSelectorRowVisible(objects.category_none,true);
    setSharedSelectorRowVisible(objects.category_analog,true);
    setSharedSelectorRowVisible(objects.category_switches,true);
    setSharedSelectorRowVisible(objects.category_fs,true);
    setSharedSelectorRowVisible(objects.category_ls,true);
    setSharedSelectorRowVisible(objects.category_trims,true);
    setSharedSelectorRowVisible(objects.category_channels,true);
    setSharedSelectorRowVisible(objects.category_vars,true);
    setSharedSelectorRowVisible(objects.category_gyro,true);
    setSharedSelectorRowVisible(objects.category_trainer,true);
    setSharedSelectorRowVisible(objects.category_timers,true);
    setSharedSelectorRowVisible(objects.category_sv,true);
    setSharedSelectorRowVisible(objects.category_special,true);

    setSharedSelectorRowVisible(objects.category_sp,false);
    setSharedSelectorRowVisible(objects.category_tp,false);
    setSharedSelectorRowVisible(objects.category_fm,false);
    setSharedSelectorRowVisible(objects.category_se,false);
    setSharedSelectorRowVisible(objects.category_direction,false);

    setSharedSelectorRowText(objects.category_analog,"Analogs");
    setSharedSelectorRowText(objects.category_switches,"Switches");
    setSharedSelectorRowText(objects.category_trims,"Trims");
    setSharedSelectorRowText(objects.category_channels,"Channels");
    setSharedSelectorRowText(objects.category_vars,"Vars");
    setSharedSelectorRowText(objects.category_gyro,"Gyro");
    setSharedSelectorRowText(objects.category_trainer,"Trainer");
    setSharedSelectorRowText(objects.category_timers,"Timers");
    setSharedSelectorRowText(objects.category_sv,"System value");
    setSharedSelectorRowText(objects.category_special,"Special");

    lv_obj_t *rows[]={
        objects.category_none,
        objects.category_analog,
        objects.category_switches,
        objects.category_fs,
        objects.category_ls,
        objects.category_trims,
        objects.category_channels,
        objects.category_vars,
        objects.category_gyro,
        objects.category_trainer,
        objects.category_timers,
        objects.category_sv,
        objects.category_special
    };
    orderSharedSelectorRows(rows,sizeof(rows)/sizeof(rows[0]));

    if(objects.category_panel)
        lv_obj_scroll_to_y(objects.category_panel,0,LV_ANIM_OFF);
}

static void configureSharedSelectorForActiveCondition(){
    sharedSelectorMode=SharedSelectorMode::ACTIVE_CONDITION;

    clearMixSourceMembers();

    // ETHOS Mix Active Condition order: photo 1 + photo 2.
    setSharedSelectorRowVisible(objects.category_none,true);
    setSharedSelectorRowVisible(objects.category_analog,true);
    setSharedSelectorRowVisible(objects.category_sp,true);
    setSharedSelectorRowVisible(objects.category_fs,true);
    setSharedSelectorRowVisible(objects.category_ls,true);
    setSharedSelectorRowVisible(objects.category_tp,true);
    setSharedSelectorRowVisible(objects.category_fm,true);
    setSharedSelectorRowVisible(objects.category_se,true);

    setSharedSelectorRowVisible(objects.category_switches,false);
    setSharedSelectorRowVisible(objects.category_trims,false);
    setSharedSelectorRowVisible(objects.category_channels,false);
    setSharedSelectorRowVisible(objects.category_vars,false);
    setSharedSelectorRowVisible(objects.category_gyro,false);
    setSharedSelectorRowVisible(objects.category_trainer,false);
    setSharedSelectorRowVisible(objects.category_timers,false);
    setSharedSelectorRowVisible(objects.category_sv,false);
    setSharedSelectorRowVisible(objects.category_special,false);
    setSharedSelectorRowVisible(objects.category_direction,false);

    setSharedSelectorRowText(objects.category_analog,"Always on");
    setSharedSelectorRowText(objects.category_sp,"Switch positions");
    setSharedSelectorRowText(objects.category_fs,"Function switches");
    setSharedSelectorRowText(objects.category_ls,"Logic switches");
    setSharedSelectorRowText(objects.category_tp,"Trim positions");
    setSharedSelectorRowText(objects.category_fm,"Flight mode");
    setSharedSelectorRowText(objects.category_se,"System event");

    lv_obj_t *rows[]={
        objects.category_none,
        objects.category_analog,
        objects.category_sp,
        objects.category_fs,
        objects.category_ls,
        objects.category_tp,
        objects.category_fm,
        objects.category_se
    };
    orderSharedSelectorRows(rows,sizeof(rows)/sizeof(rows[0]));

    if(objects.category_panel)
        lv_obj_scroll_to_y(objects.category_panel,0,LV_ANIM_OFF);
}

static void configureSharedSelectorForActionActiveCondition(){
    configureSharedSelectorForActiveCondition();
    sharedSelectorMode=SharedSelectorMode::ACTION_ACTIVE_CONDITION;

    // ETHOS Action Active Condition order: photo 5 + photo 6.
    setSharedSelectorRowVisible(objects.category_direction,true);
    setSharedSelectorRowVisible(objects.category_gyro,true);
    setSharedSelectorRowVisible(objects.category_fm,false);

    setSharedSelectorRowText(objects.category_gyro,"Gyro switches");

    lv_obj_t *rows[]={
        objects.category_none,
        objects.category_analog,
        objects.category_direction,
        objects.category_sp,
        objects.category_fs,
        objects.category_ls,
        objects.category_tp,
        objects.category_gyro,
        objects.category_se
    };
    orderSharedSelectorRows(rows,sizeof(rows)/sizeof(rows[0]));

    if(objects.category_panel)
        lv_obj_scroll_to_y(objects.category_panel,0,LV_ANIM_OFF);
}

static void configureSharedSelectorForVariantActiveCondition(){
    configureSharedSelectorForActiveCondition();
    sharedSelectorMode=SharedSelectorMode::VARIANT_ACTIVE_CONDITION;

    // Variant AC inside Weight/Curve uses the same ETHOS category set
    // and order as the Action-level Active Condition.
    setSharedSelectorRowVisible(objects.category_direction,true);
    setSharedSelectorRowVisible(objects.category_gyro,true);
    setSharedSelectorRowVisible(objects.category_fm,false);

    setSharedSelectorRowText(objects.category_gyro,"Gyro switches");

    lv_obj_t *rows[]={
        objects.category_none,
        objects.category_analog,
        objects.category_direction,
        objects.category_sp,
        objects.category_fs,
        objects.category_ls,
        objects.category_tp,
        objects.category_gyro,
        objects.category_se
    };
    orderSharedSelectorRows(rows,sizeof(rows)/sizeof(rows[0]));

    if(objects.category_panel)
        lv_obj_scroll_to_y(objects.category_panel,0,LV_ANIM_OFF);
}

static void actionMixActionActiveConditionShow(lv_event_t *e){
    stopMixSourceStickFocusDetect();
    (void)e;

    // LVGL sends CLICKED after LONG_PRESSED on release.
    // If the long press opened the Invert popup, consume exactly that trailing click.
    if(suppressActionActiveConditionClick){
        suppressActionActiveConditionClick=false;
        return;
    }

    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
    if(!objects.mix_edit_new_action_panel||lv_obj_has_flag(objects.mix_edit_new_action_panel,LV_OBJ_FLAG_HIDDEN)) return;

    ensureInputRuntimeStarted();
    setupMixSourceSelectorCallbacks();
    configureSharedSelectorForActionActiveCondition();

    lv_obj_t *root=getMixSourceSelectorRoot();
    if(root){
        lv_obj_remove_flag(root,LV_OBJ_FLAG_HIDDEN);
        showPopupDismissOverlay(root);
        startMixSourceStickFocusDetect();
        focusCurrentActionDirectionSelection();
    }
}


static void actionMixVariantActiveConditionShow(lv_event_t *e){
    stopMixSourceStickFocusDetect();
    if(!e) return;

    if(suppressVariantActiveConditionClick){
        suppressVariantActiveConditionClick=false;
        return;
    }

    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
    if(!isAileronMixEditor()&&(!objects.mix_edit_new_action_panel||
       lv_obj_has_flag(objects.mix_edit_new_action_panel,LV_OBJ_FLAG_HIDDEN)))
        return;

    lv_obj_t *target=(lv_obj_t *)lv_event_get_current_target(e);
    MixActionType type;
    uint8_t slot=0;
    if(!getMixActionVariantConditionTarget(target,type,slot)) return;

    activeMixVariantConditionType=type;
    activeMixVariantConditionSlot=slot;

    ensureInputRuntimeStarted();
    setupMixSourceSelectorCallbacks();
    configureSharedSelectorForVariantActiveCondition();

    lv_obj_t *root=getMixSourceSelectorRoot();
    if(root){
        lv_obj_remove_flag(root,LV_OBJ_FLAG_HIDDEN);
        showPopupDismissOverlay(root);
        startMixSourceStickFocusDetect();
    }
}

static void actionMixActiveConditionShow(lv_event_t *e){
    stopMixSourceStickFocusDetect();
    (void)e;

    // LVGL sends CLICKED after LONG_PRESSED on release.
    // If the long press opened the Invert popup, consume exactly that trailing click.
    if(suppressMixActiveConditionClick){
        suppressMixActiveConditionClick=false;
        return;
    }

    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    ensureInputRuntimeStarted();
    setupMixSourceSelectorCallbacks();
    configureSharedSelectorForActiveCondition();

    lv_obj_t *root=getMixSourceSelectorRoot();
    if(root){
        lv_obj_remove_flag(root,LV_OBJ_FLAG_HIDDEN);
        showPopupDismissOverlay(root);
        startMixSourceStickFocusDetect();
    }
}

static void setupMixSourceSelectorCallbacks(){
    if(!objects.scr_mixes) return;
    if(sourceSelectorCallbacksBoundScreen==objects.scr_mixes) return;

    sourceSelectorCallbacksBoundScreen=objects.scr_mixes;

    if(objects.category_none)
        lv_obj_add_event_cb(
            objects.category_none,
            mixSourceCategoryNoneClicked,
            LV_EVENT_CLICKED,
            nullptr
        );

    if(objects.category_analog)
        lv_obj_add_event_cb(
            objects.category_analog,
            mixSourceCategoryAnalogClicked,
            LV_EVENT_CLICKED,
            nullptr
        );

    if(objects.category_switches)
        lv_obj_add_event_cb(
            objects.category_switches,
            mixSourceSwitchCategoryClicked,
            LV_EVENT_CLICKED,
            nullptr
        );

    if(objects.category_sp)
        lv_obj_add_event_cb(
            objects.category_sp,
            activeConditionSwitchCategoryClicked,
            LV_EVENT_CLICKED,
            nullptr
        );

    if(objects.category_fs)
        lv_obj_add_event_cb(
            objects.category_fs,
            activeConditionFunctionSwitchCategoryClicked,
            LV_EVENT_CLICKED,
            nullptr
        );

    if(objects.category_ls)
        lv_obj_add_event_cb(
            objects.category_ls,
            activeConditionLogicSwitchCategoryClicked,
            LV_EVENT_CLICKED,
            nullptr
        );

    if(objects.category_tp)
        lv_obj_add_event_cb(
            objects.category_tp,
            activeConditionTrimCategoryClicked,
            LV_EVENT_CLICKED,
            nullptr
        );

    if(objects.category_fm)
        lv_obj_add_event_cb(
            objects.category_fm,
            activeConditionFlightModeCategoryClicked,
            LV_EVENT_CLICKED,
            nullptr
        );

    if(objects.category_se)
        lv_obj_add_event_cb(
            objects.category_se,
            activeConditionSystemEventCategoryClicked,
            LV_EVENT_CLICKED,
            nullptr
        );

    if(objects.category_direction)
        lv_obj_add_event_cb(
            objects.category_direction,
            activeConditionDirectionCategoryClicked,
            LV_EVENT_CLICKED,
            nullptr
        );

    if(objects.aileron_mix_ac_panel){
        lv_obj_add_flag(objects.aileron_mix_ac_panel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(objects.aileron_mix_ac_panel,actionMixActiveConditionShow,LV_EVENT_CLICKED,nullptr);
    }
    if(objects.mix_ac_panel)
        lv_obj_add_event_cb(
            objects.mix_ac_panel,
            actionMixActiveConditionShow,
            LV_EVENT_CLICKED,
            nullptr
        );

    if(objects.mix_source_panel){
        lv_obj_add_flag(objects.mix_source_panel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(
            objects.mix_source_panel,
            action_invert_panel_show,
            LV_EVENT_LONG_PRESSED,
            nullptr
        );
    }

    if(objects.mix_action_ac_panel){
        lv_obj_add_flag(objects.mix_action_ac_panel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(
            objects.mix_action_ac_panel,
            actionMixActionActiveConditionShow,
            LV_EVENT_CLICKED,
            nullptr
        );
        lv_obj_add_event_cb(
            objects.mix_action_ac_panel,
            action_invert_panel_show,
            LV_EVENT_LONG_PRESSED,
            nullptr
        );
    }

    MixEditorKind priorKind=activeMixEditorKind;
    for(uint8_t kind=0;kind<2;kind++){
        activeMixEditorKind=kind?MixEditorKind::AILERON:MixEditorKind::FREE_MIX;
    for(uint8_t slot=1;slot<10;slot++){
        lv_obj_t *panel=getMixActionVariantConditionPanel(MixActionType::WEIGHT,slot);
        if(!panel) continue;
        lv_obj_add_flag(panel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(panel,actionMixVariantActiveConditionShow,LV_EVENT_CLICKED,nullptr);
        lv_obj_add_event_cb(panel,action_invert_panel_show,LV_EVENT_LONG_PRESSED,nullptr);
    }

    for(uint8_t slot=1;slot<6;slot++){
        lv_obj_t *panel=getMixActionVariantConditionPanel(MixActionType::CURVE,slot);
        if(!panel) continue;
        lv_obj_add_flag(panel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(panel,actionMixVariantActiveConditionShow,LV_EVENT_CLICKED,nullptr);
        lv_obj_add_event_cb(panel,action_invert_panel_show,LV_EVENT_LONG_PRESSED,nullptr);
    }
    }
    activeMixEditorKind=priorKind;
}

void action_source_selector_show(lv_event_t *e){
    (void)e;

    if(suppressMixSourceClick){
        suppressMixSourceClick=false;
        return;
    }

    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    ensureInputRuntimeStarted();
    setupMixSourceSelectorCallbacks();
    configureSharedSelectorForSource();

    lv_obj_t *root=getMixSourceSelectorRoot();
    if(root){
        lv_obj_remove_flag(root,LV_OBJ_FLAG_HIDDEN);
        showPopupDismissOverlay(root);
        startMixSourceStickFocusDetect();
    }

    ModelConfig &model=modelManager.getActiveModel();
    const MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    // Если текущий Source уже относится к Analog, сразу показываем members.
    if(mix.source.type==SourceType::STICK_FUNCTION || mix.source.type==SourceType::ANALOG_INPUT)
        buildMixAnalogMembers();
}

static void updateInputRuntime(lv_timer_t *timer){
    (void)timer;

    // InputManager, Model Inputs, Mixer and OutputProcessor are updated once
    // by the main control loop. This UI timer only refreshes editor widgets:
    // never run a second control-frame calculation from an LVGL callback.
    updateMixSourceStickFocusDetect();
    updateVarConditionAutoDetect();

    // Live monitor of the selected Free Mix outputs.
    updateMixOutputMonitors();

    // Live value of the currently selected Free Mix Source.
    if(editingMixIndex>=0&&editingMixIndex<MAX_MIX_LINES)
        updateMixSourceEditorUi();
}

static bool mixGraphSourceIsTwoState(const MixLine &mix);

static int16_t getMixSourceLiveValue(const MixLine &mix){
    int32_t value=getMixSourceRawLiveValue(mix);
    // A 2-POS / Momentary switch has no neutral position. An unassigned
    // physical input may report zero, but it must not appear as 0% Source.
    if(mixGraphSourceIsTwoState(mix)) value=value>0?1000L:-1000L;
    value=applyMixSourceOptionsValue(value,mix);
    return (int16_t)constrain(value,-1000L,1000L);
}

static bool mixGraphSourceIsTwoState(const MixLine &mix){
    SourceId source=getMixBaseSource(mix);
    if(source.type==SourceType::LOGICAL_SWITCH) return source.index<MAX_LOGICAL_SWITCHES;
    if(source.type!=SourceType::SWITCH_INPUT) return false;

    const uint8_t input=source.index;
    if(input==static_cast<uint8_t>(SwitchInput::STICK_BTN_L)||input==static_cast<uint8_t>(SwitchInput::STICK_BTN_R)) return true;
    const uint8_t first=static_cast<uint8_t>(SwitchInput::SW_3POS_1);
    if(input<first||input>=first+HARDWARE_SUPPORTED_SWITCHES) return false;
    const uint8_t type=hardwareSwitchType(input-first);
    return type==1||type==3;
}

static int32_t getMixGraphSourceInput(int32_t graphInput,const MixLine &mix){
    // 2-POS / Momentary: a step at 0%, as in ETHOS.
    // 3-POS: a straight line through -100%, 0%, +100%.
    // Keep X as the physical source position; Invert and Half range change Y only.
    if(mixGraphSourceIsTwoState(mix)) return graphInput<0?-1000L:1000L;
    return graphInput;
}


static void formatMixSourceName(const SourceId &sourceWithOptions,char *buffer,size_t size){
    if(!buffer||size==0) return;
    buffer[0]='\0';

    SourceId source=sourceWithOptions;
    source.index&=(uint8_t)~MIX_SOURCE_HALF_RANGE_FLAG;

    switch(source.type){
        case SourceType::SWITCH_INPUT:{
            const uint8_t first=static_cast<uint8_t>(SwitchInput::SW_3POS_1);
            if(source.index>=first&&source.index<first+HARDWARE_SUPPORTED_SWITCHES){
                hardwareSwitchName(source.index-first,buffer,size);
                return;
            }
            const char *name=sourceResolver.getName(source);
            snprintf(buffer,size,"%s",name?name:"Switch ?");
            return;
        }
        case SourceType::LOGICAL_SWITCH:
            snprintf(buffer,size,"LS%u",(unsigned)(source.index+1));
            return;
        case SourceType::CHANNEL_OUTPUT:
            snprintf(buffer,size,"CH%u",(unsigned)(source.index+1));
            return;
        case SourceType::CONSTANT_VALUE:
            snprintf(buffer,size,"Constant");
            return;
        default:
            break;
    }

    const char *name=sourceResolver.getName(source);
    snprintf(buffer,size,"%s",name?name:"---");
}

static void formatMixConditionName(const ConditionConfig &condition,char *buffer,size_t size){
    if(!buffer||size==0) return;
    if(!condition.isValid()){
        snprintf(buffer,size,"Always on");
        return;
    }

    if(condition.value2==ACTION_DIRECTION_MARKER&&
       (condition.type==ConditionType::SOURCE_POSITIVE||
        condition.type==ConditionType::SOURCE_NEGATIVE)){
        const char *direction=getActionDirectionLabel(condition);
        if(direction){
            if(condition.inverted) snprintf(buffer,size,"!%s",direction);
            else snprintf(buffer,size,"%s",direction);
            return;
        }
    }

    char sourceA[32];
    char sourceB[32];
    formatMixSourceName(condition.sourceA,sourceA,sizeof(sourceA));
    formatMixSourceName(condition.sourceB,sourceB,sizeof(sourceB));

    switch(condition.type){
        case ConditionType::ALWAYS:
            snprintf(buffer,size,"Always on");
            break;
        case ConditionType::NEVER:
            snprintf(buffer,size,"---");
            break;
        case ConditionType::SOURCE_POSITIVE:
            snprintf(buffer,size,"%s positive",sourceA);
            break;
        case ConditionType::SOURCE_NEGATIVE:
            snprintf(buffer,size,"%s negative",sourceA);
            break;
        case ConditionType::SOURCE_ABOVE:
            snprintf(buffer,size,"%s > %.1f%%",sourceA,condition.value1/10.0f);
            break;
        case ConditionType::SOURCE_BELOW:
            snprintf(buffer,size,"%s < %.1f%%",sourceA,condition.value1/10.0f);
            break;
        case ConditionType::SOURCE_EQUAL:
            if(condition.sourceA.type==SourceType::SWITCH_INPUT){
                const char *position=condition.value1>0?"↑":(condition.value1<0?"↓":"-");
                snprintf(buffer,size,"%s%s",sourceA,position);
            }else{
                snprintf(buffer,size,"%s = %.1f%%",sourceA,condition.value1/10.0f);
            }
            break;
        case ConditionType::SOURCE_RANGE:
            snprintf(buffer,size,"%s %.1f..%.1f%%",sourceA,condition.value1/10.0f,condition.value2/10.0f);
            break;
        case ConditionType::SOURCE_ABOVE_SOURCE:
            snprintf(buffer,size,"%s > %s",sourceA,sourceB);
            break;
        case ConditionType::SOURCE_BELOW_SOURCE:
            snprintf(buffer,size,"%s < %s",sourceA,sourceB);
            break;
        case ConditionType::NONE:
        default:
            snprintf(buffer,size,"Always on");
            break;
    }

    if(condition.inverted){
        char temp[64];
        snprintf(temp,sizeof(temp),"!%s",buffer);
        snprintf(buffer,size,"%s",temp);
    }
}


static bool mixGraphConditionActive(const ConditionConfig &condition,int32_t sourceValue){
    if(condition.type==ConditionType::NEVER) return false;
    if(condition.type==ConditionType::ALWAYS) return !condition.inverted;
    if(condition.type==ConditionType::NONE) return true;
    if(!condition.isValid()) return false;

    if(condition.value2==ACTION_DIRECTION_MARKER&&
       (condition.type==ConditionType::SOURCE_POSITIVE||
        condition.type==ConditionType::SOURCE_NEGATIVE)){
        bool result=condition.type==ConditionType::SOURCE_POSITIVE?
            sourceValue>0:
            sourceValue<0;
        return condition.inverted?!result:result;
    }

    return conditionEvaluator.evaluate(condition);
}

static const MixActionVariant *getMixGraphVariant(const MixAction &action,int32_t sourceValue){
    uint8_t count=action.variantCount;
    if(count==0) return nullptr;
    if(count>MAX_MIX_ACTION_VARIANTS) count=MAX_MIX_ACTION_VARIANTS;

    for(uint8_t i=1;i<count;i++){
        const MixActionVariant &variant=action.variants[i];
        if(!variant.enabled) continue;

        // Пока полноценный Condition Selector не подключен,
        // пустой ConditionConfig означает "Always on".
        // Первый активный variant имеет приоритет над Otherwise.
        if(mixGraphConditionActive(variant.condition,sourceValue))
            return &variant;
    }

    return action.variants[0].enabled?&action.variants[0]:nullptr;
}

static int32_t applyOneMixActionForGraph(
    int32_t value,
    const MixAction &action,
    const ModelConfig &model,
    int32_t sourceValue
){
    if(!action.enabled) return value;

    bool active=mixGraphConditionActive(action.condition,sourceValue);

    switch(action.type){
        case MixActionType::WEIGHT:{
            if(!active) break;

            const MixActionVariant *variant=getMixGraphVariant(action,sourceValue);
            if(variant)
                // ETHOS Free Mix graph saturates the displayed Weight at +/-100%.
                // The real Mixer keeps the full saved -500%..+500% range.
                value=(value*(int32_t)constrain((int32_t)variant->value,-1000L,1000L))/1000L;

            break;
        }

        case MixActionType::CURVE:{
            if(!active) break;

            const MixActionVariant *variant=getMixGraphVariant(action,sourceValue);
            if(!variant) break;

            if(variant->curveIndex==OUTPUT_CURVE_EXPO){
                value=curveProcessor.applyExpo(
                    (int16_t)constrain(value,-1000L,1000L),
                    (int8_t)constrain((int32_t)variant->expo,-100L,100L)
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

        case MixActionType::DIFFERENTIAL:{
            if(!active) break;

            int32_t diff=constrain((int32_t)action.value1,-1000L,1000L);

            if(diff>0&&value<0)
                value=(value*(1000L-diff))/1000L;
            else if(diff<0&&value>0)
                value=(value*(1000L+diff))/1000L;

            break;
        }

        case MixActionType::OFFSET:
            value+=constrain(
                (int32_t)(active?action.value1:action.value2),
                -2000L,
                2000L
            );
            break;

        case MixActionType::SLOW:
        case MixActionType::TRIM:
            break;
    }

    // Keep the mathematical Free Mix result. Output limits belong to OutputProcessor,
    // not to the Action chain or its graph.
    return constrain(value,-30000L,30000L);
}

static int32_t applyMixActionsForGraph(
    int32_t value,
    const MixLine &mix,
    const ModelConfig &model
){
    const int32_t sourceValue=value;

    uint8_t count=mix.actionCount;
    if(count>MAX_MIX_ACTIONS) count=MAX_MIX_ACTIONS;

    for(uint8_t i=0;i<count;i++)
        value=applyOneMixActionForGraph(value,mix.actions[i],model,sourceValue);

    return value;
}

static int32_t applyMixGraphPreview(
    int32_t value,
    const MixLine &mix,
    const ModelConfig &model,
    const MixAction &previewAction
){
    const int32_t sourceValue=value;

    uint8_t count=mix.actionCount;
    if(count>MAX_MIX_ACTIONS) count=MAX_MIX_ACTIONS;

    if(mixActionEditorIsNew){
        for(uint8_t i=0;i<count;i++)
            value=applyOneMixActionForGraph(value,mix.actions[i],model,sourceValue);

        return applyOneMixActionForGraph(value,previewAction,model,sourceValue);
    }

    if(editingMixActionIndex<0||editingMixActionIndex>=count)
        return applyOneMixActionForGraph(value,previewAction,model,sourceValue);

    for(uint8_t i=0;i<(uint8_t)editingMixActionIndex;i++)
        value=applyOneMixActionForGraph(value,mix.actions[i],model,sourceValue);

    return applyOneMixActionForGraph(value,previewAction,model,sourceValue);
}

static void setMixGraphActionPreview(bool enabled){
    // enabled == true: we are inside Mix Editor / Action Editor.
    // Both windows use the SAME parent: mix_editing_diag_panel.
    // enabled == false: we are in the Mix list and use mix_diag_panel.
    mixGraphActionPreview=enabled;

    if(!objects.mix_diag) return;

    lv_obj_t *targetParent=enabled?objects.mix_editing_diag_panel:objects.mix_diag_panel;
    if(!targetParent) return;

    if(lv_obj_get_parent(objects.mix_diag)!=targetParent)
        lv_obj_set_parent(objects.mix_diag,targetParent);

    // Keep canvas aligned to the 200x200 parent after reparenting.
    lv_obj_set_pos(objects.mix_diag,0,0);
    lv_obj_set_size(objects.mix_diag,200,200);
}

static void clearMixGraph(){
    if(mixGraphLine)
        lv_obj_add_flag(mixGraphLine,LV_OBJ_FLAG_HIDDEN);
    if(mixGraphCursorLine)
        lv_obj_add_flag(mixGraphCursorLine,LV_OBJ_FLAG_HIDDEN);
    if(mixGraphCurrentPoint)
        lv_obj_add_flag(mixGraphCurrentPoint,LV_OBJ_FLAG_HIDDEN);
}

// The red transfer line describes the Free Mix Actions. Invert and Half range
// only modify the live Source value and cursor position; neither reshapes it.
static int32_t evaluateMixGraphTransfer(int32_t physicalInput,const MixLine &mix,const ModelConfig &model,bool havePreview,const MixAction &previewAction){
    int32_t value=getMixGraphSourceInput(physicalInput,mix);
    if(havePreview) value=applyMixGraphPreview(value,mix,model,previewAction);
    else if(mix.actionCount>0) value=applyMixActionsForGraph(value,mix,model);
    else{
        value=(value*(int32_t)mix.weight)/1000L;
        value+=mix.offset;
        if(mix.curveIndex>=0&&mix.curveIndex<MAX_MODEL_CURVES) value=curveProcessor.process((int16_t)constrain(value,-1000L,1000L),model.curves[mix.curveIndex]);
    }
    return constrain(value,-30000L,30000L);
}

static void drawMixGraph(uint8_t mixIndex){
    if(objects.mix_diag==nullptr) return;
    if(mixIndex>=MAX_MIX_LINES){
        clearMixGraph();
        return;
    }

    ModelConfig &model=modelManager.getActiveModel();
    const MixLine &mix=model.mixes[mixIndex];
    if(!mix.enabled){
        clearMixGraph();
        return;
    }

    // ETHOS-like behavior:
    // an inactive Mix has no transfer graph and no live cursor/point.
    // "---" is stored as ConditionType::NEVER, so the graph must be empty.
    // Evaluate the complete ConditionConfig, including the inverted flag.
    // "!Always on" must behave exactly like "---".
    bool mixActive=true;

    if(mix.condition.type==ConditionType::NEVER)
        mixActive=false;
    else if(mix.condition.type==ConditionType::NONE)
        mixActive=true;
    else
        mixActive=conditionEvaluator.evaluate(mix.condition);

    if(!mixActive){
        clearMixGraph();
        return;
    }

    if(mixGraphLine==nullptr){
        mixGraphLine=lv_line_create(objects.mix_diag);
        lv_obj_set_style_line_color(mixGraphLine,lv_color_hex(0xFF0000),LV_PART_MAIN);
        lv_obj_set_style_line_width(mixGraphLine,2,LV_PART_MAIN);
        lv_obj_remove_flag(mixGraphLine,LV_OBJ_FLAG_CLICKABLE);
    }else{
        lv_obj_set_style_line_color(mixGraphLine,lv_color_hex(0xFF0000),LV_PART_MAIN);
    }

    if(mixGraphCursorLine==nullptr){
        mixGraphCursorLine=lv_line_create(objects.mix_diag);
        lv_obj_set_style_line_color(mixGraphCursorLine,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_MS_TEXT]),LV_PART_MAIN);
        lv_obj_set_style_line_width(mixGraphCursorLine,2,LV_PART_MAIN);
        lv_obj_remove_flag(mixGraphCursorLine,LV_OBJ_FLAG_CLICKABLE);
    }else{
        lv_obj_set_style_line_color(mixGraphCursorLine,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_MS_TEXT]),LV_PART_MAIN);
    }

    if(mixGraphCurrentPoint==nullptr){
        mixGraphCurrentPoint=lv_obj_create(objects.mix_diag);
        lv_obj_set_size(mixGraphCurrentPoint,8,8);
        lv_obj_set_style_radius(mixGraphCurrentPoint,LV_RADIUS_CIRCLE,LV_PART_MAIN);
        lv_obj_set_style_bg_opa(mixGraphCurrentPoint,LV_OPA_COVER,LV_PART_MAIN);
        lv_obj_set_style_bg_color(mixGraphCurrentPoint,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_TEXT]),LV_PART_MAIN);
        lv_obj_set_style_border_width(mixGraphCurrentPoint,0,LV_PART_MAIN);
        lv_obj_set_style_pad_all(mixGraphCurrentPoint,0,LV_PART_MAIN);
        lv_obj_remove_flag(mixGraphCurrentPoint,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(mixGraphCurrentPoint,LV_OBJ_FLAG_SCROLLABLE);
    }else{
        lv_obj_set_style_bg_color(mixGraphCurrentPoint,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_TEXT]),LV_PART_MAIN);
    }

    constexpr int32_t W=200;
    constexpr int32_t H=200;

    MixAction previewAction{};
    // Preview only while the Action Editor is actually visible. Its controls
    // retain old values after closing, so using them in Free Mix would draw
    // a removed or previously edited Action instead of the saved mix.
    bool actionEditorVisible=objects.mix_edit_new_action_panel&&
        !lv_obj_has_flag(objects.mix_edit_new_action_panel,LV_OBJ_FLAG_HIDDEN);
    bool havePreview=
        mixGraphActionPreview&&
        actionEditorVisible&&
        editingMixIndex==(int8_t)mixIndex&&
        (mixActionEditorIsNew||editingMixActionIndex>=0)&&
        buildMixActionFromEditor(previewAction);

    // ETHOS expands the Y scale when Actions produce values outside +/-100%.
    // Example:
    //   Offset inactive +100% -> graph range +/-200%
    //   Offset inactive +200% -> graph range +/-300%
    int32_t graphValues[MIX_GRAPH_DRAW_POINTS];
    int32_t graphMaxAbs=1000;

    for(uint16_t px=0;px<MIX_GRAPH_DRAW_POINTS;px++){
        int32_t input=-1000L+(2000L*px)/(MIX_GRAPH_DRAW_POINTS-1);
        int32_t value=evaluateMixGraphTransfer(input,mix,model,havePreview,previewAction);
        graphValues[px]=value;

        int32_t absValue=value<0?-value:value;
        if(absValue>graphMaxAbs) graphMaxAbs=absValue;
    }

    // Round upward to the next whole 100% step.
    int32_t graphRange=((graphMaxAbs+999L)/1000L)*1000L;
    if(graphRange<1000L) graphRange=1000L;

    // LVGL keeps the pointer passed to lv_line_set_points(). If that same
    // array is overwritten before the next call, the line may fail to
    // invalidate its previous shape after Action deletion.
    mixGraphPointBuffer^=1;
    lv_point_precise_t *points=mixGraphPoints[mixGraphPointBuffer];
    for(uint16_t px=0;px<MIX_GRAPH_DRAW_POINTS;px++){
        int32_t value=constrain(graphValues[px],-graphRange,graphRange);
        points[px].x=(int32_t)px*(W-1)/(MIX_GRAPH_DRAW_POINTS-1);
        points[px].y=((graphRange-value)*(H-1))/(graphRange*2L);
    }

    // Discrete inputs need a vertical step at x=0%, not a diagonal segment.
    // Two adjacent points share the same X while preserving both Y levels.
    if(mixGraphSourceIsTwoState(mix)){
        const uint16_t middle=MIX_GRAPH_DRAW_POINTS/2;
        points[middle-1].x=(W-1)/2;
        points[middle].x=(W-1)/2;
    }

    lv_obj_invalidate(mixGraphLine);
    lv_line_set_points(mixGraphLine,points,MIX_GRAPH_DRAW_POINTS);
    lv_obj_invalidate(mixGraphLine);
    lv_obj_set_pos(mixGraphLine,0,0);
    lv_obj_remove_flag(mixGraphLine,LV_OBJ_FLAG_HIDDEN);

    // The live cursor follows the displayed Source percentage, *after*
    // Invert / Half range, while the red step remains unchanged.
    int32_t graphSourceValue=constrain((int32_t)getMixSourceLiveValue(mix),-1000L,1000L);
    int32_t cursorX=((graphSourceValue+1000L)*(W-1))/2000;

    // Keep the 2 px live cursor fully inside the 200 px graph.
    // At +100% the old +1 offset produced x=200, one pixel outside.
    cursorX=constrain(cursorX+1,1L,W-3);

    // Keep the live point on the unchanged graph at the cursor X position.
    int32_t currentValue=evaluateMixGraphTransfer(graphSourceValue,mix,model,havePreview,previewAction);

    currentValue=constrain(currentValue,-graphRange,graphRange);
    int32_t cursorY=((graphRange-currentValue)*(H-1))/(graphRange*2L);
    cursorY-=1;

    mixGraphCursorPoints[0].x=cursorX;
    mixGraphCursorPoints[0].y=-1;
    mixGraphCursorPoints[1].x=cursorX;
    mixGraphCursorPoints[1].y=H-2;

    lv_line_set_points(mixGraphCursorLine,mixGraphCursorPoints,2);
    lv_obj_set_pos(mixGraphCursorLine,0,0);
    lv_obj_remove_flag(mixGraphCursorLine,LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_pos(mixGraphCurrentPoint,cursorX-4,cursorY-4);
    lv_obj_remove_flag(mixGraphCurrentPoint,LV_OBJ_FLAG_HIDDEN);

    lv_obj_move_foreground(mixGraphLine);
    lv_obj_move_foreground(mixGraphCursorLine);
    lv_obj_move_foreground(mixGraphCurrentPoint);

    // The main control loop owns Mixer/OutputProcessor calculations.
    // Reuse the completed frame for the live Output monitor.
    updateMixOutputMonitors();
}


static void updateMixGraphLive(lv_timer_t *timer){
    (void)timer;
    if(selectedMixIndex<0) return;
    if(objects.scr_mixes==nullptr) return;
    if(lv_screen_active()!=objects.scr_mixes) return;

    // Входы и Model Inputs обновляются основным циклом управления.
    // Числовое значение Source должно обновляться в реальном времени
    // независимо от того, активен сейчас Mix или нет.
    if(editingMixIndex>=0&&editingMixIndex<MAX_MIX_LINES)
        updateMixSourceEditorUi();

    drawMixGraph((uint8_t)selectedMixIndex);
}


static void setSharedPopupItemVisible(lv_obj_t *obj,bool visible){
    if(!obj) return;
    if(visible) lv_obj_remove_flag(obj,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj,LV_OBJ_FLAG_HIDDEN);
}

static void exclusiveSelectionPressed(lv_event_t *e){
    if(!e) return;
    lv_obj_t *current=(lv_obj_t *)lv_event_get_current_target(e);
    if(!current) current=(lv_obj_t *)lv_event_get_target(e);
    if(!current) return;
    lv_obj_t *parent=lv_obj_get_parent(current);
    if(!parent) return;
    uint32_t count=lv_obj_get_child_count(parent);
    for(uint32_t i=0;i<count;i++){
        lv_obj_t *child=lv_obj_get_child(parent,i);
        if(!child||child==current) continue;
        lv_obj_remove_state(child,LV_STATE_CHECKED);
        lv_obj_remove_state(child,LV_STATE_FOCUSED);
    }
    // Popup selection is exclusive even for non-checkable EEZ rows.
    lv_obj_add_state(current,LV_STATE_CHECKED);
    lv_obj_add_state(current,LV_STATE_FOCUSED);
}

// Clear the previous row on PRESSED, before CLICKED paints the new selection.
// Keep the pressed row's state so a second tap still opens its popup.
static void exclusiveListRowPressed(lv_event_t *e){
    if(!e) return;
    lv_obj_t *row=(lv_obj_t *)lv_event_get_current_target(e);
    if(!row) return;
    lv_obj_t *parent=lv_obj_get_parent(row);
    if(!parent) return;
    uint32_t count=lv_obj_get_child_count(parent);
    for(uint32_t i=0;i<count;i++){
        lv_obj_t *other=lv_obj_get_child(parent,i);
        if(!other||other==row) continue;
        lv_obj_remove_state(other,LV_STATE_CHECKED);
        lv_obj_remove_state(other,LV_STATE_FOCUSED);
    }
}

static void resetSharedPopupSelectionState(){
    lv_obj_t *items[]={
        objects.curve_popup__popup_item1,
        objects.curve_popup__popup_item2,
        objects.curve_popup__popup_item3,
        objects.curve_popup__popup_item4,
        objects.curve_popup__popup_item5,
        objects.curve_popup__popup_item6,
        objects.curve_popup__popup_item7
    };
    for(lv_obj_t *item:items){
        if(!item) continue;
        lv_obj_remove_flag(item,LV_OBJ_FLAG_CHECKABLE);
        lv_obj_remove_state(item,LV_STATE_CHECKED);
        lv_obj_remove_state(item,LV_STATE_FOCUSED);
        lv_obj_remove_event_cb(item,exclusiveSelectionPressed);
        lv_obj_add_event_cb(item,exclusiveSelectionPressed,LV_EVENT_PRESSED,nullptr);
    }
}

static void prepareSharedEditDeletePopupForCurve(){
    if(!objects.curve_popup||!objects.scr_curve) return;

    sharedEditDeletePopupTarget=SharedEditDeletePopupTarget::CURVE;
    resetSharedPopupSelectionState();

    if(lv_obj_get_parent(objects.curve_popup)!=objects.scr_curve)
        lv_obj_set_parent(objects.curve_popup,objects.scr_curve);

    // Для Curve сейчас реально работают только:
    // 1. Edit
    // 2. Clone
    // 3. Delete
    // Остальные пункты User Widget пока не реализованы и не должны отображаться.
    setSharedPopupItemVisible(objects.curve_popup__popup_item1,true);
    setSharedPopupItemVisible(objects.curve_popup__popup_item2,true);
    setSharedPopupItemVisible(objects.curve_popup__popup_item3,true);
    setSharedPopupItemVisible(objects.curve_popup__popup_item4,false);
    setSharedPopupItemVisible(objects.curve_popup__popup_item5,false);
    setSharedPopupItemVisible(objects.curve_popup__popup_item6,false);
    setSharedPopupItemVisible(objects.curve_popup__popup_item7,false);

    if(objects.curve_popup__popup_item3_lbl)
        lv_label_set_text(objects.curve_popup__popup_item3_lbl,"Delete");

    if(objects.curve_popup__popup_item3){
        lv_obj_remove_event_cb(objects.curve_popup__popup_item3,mixActionAddMenuClicked);
        lv_obj_remove_event_cb(objects.curve_popup__popup_item3,action_delete_menu_clicked);
        lv_obj_add_event_cb(
            objects.curve_popup__popup_item3,
            action_delete_menu_clicked,
            LV_EVENT_CLICKED,
            nullptr
        );
    }

    if(objects.curve_popup__popup_item5)
        lv_obj_remove_event_cb(objects.curve_popup__popup_item5,action_delete_menu_clicked);

    if(objects.curve_popup__edited_obj_name&&selectedCurveIndex>=0&&selectedCurveIndex<MAX_MODEL_CURVES){
        ModelConfig &model=modelManager.getActiveModel();
        CurveConfig &curve=model.curves[(uint8_t)selectedCurveIndex];
        lv_label_set_text(
            objects.curve_popup__edited_obj_name,
            curve.name[0]!='\0'?curve.name:"Curve"
        );
    }
}

static void prepareSharedEditDeletePopupForMix(uint8_t mixIndex){
    if(!objects.curve_popup||!objects.scr_mixes) return;
    if(mixIndex>=MAX_MIX_LINES) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[mixIndex];
    if(!mix.enabled) return;

    sharedEditDeletePopupTarget=SharedEditDeletePopupTarget::MIX;
    resetSharedPopupSelectionState();

    if(lv_obj_get_parent(objects.curve_popup)!=objects.scr_mixes)
        lv_obj_set_parent(objects.curve_popup,objects.scr_mixes);

    // Для Mix сейчас нужны только Edit и Delete.
    setSharedPopupItemVisible(objects.curve_popup__popup_item1,true);
    setSharedPopupItemVisible(objects.curve_popup__popup_item2,false);
    setSharedPopupItemVisible(objects.curve_popup__popup_item3,true);
    setSharedPopupItemVisible(objects.curve_popup__popup_item4,false);
    setSharedPopupItemVisible(objects.curve_popup__popup_item5,false);
    setSharedPopupItemVisible(objects.curve_popup__popup_item6,false);
    setSharedPopupItemVisible(objects.curve_popup__popup_item7,false);

    if(objects.curve_popup__popup_item3_lbl)
        lv_label_set_text(objects.curve_popup__popup_item3_lbl,"Delete");

    if(objects.curve_popup__popup_item3){
        lv_obj_remove_event_cb(objects.curve_popup__popup_item3,mixActionAddMenuClicked);
        lv_obj_remove_event_cb(objects.curve_popup__popup_item3,action_delete_menu_clicked);
        lv_obj_add_event_cb(
            objects.curve_popup__popup_item3,
            action_delete_menu_clicked,
            LV_EVENT_CLICKED,
            nullptr
        );
    }

    if(objects.curve_popup__popup_item5)
        lv_obj_remove_event_cb(objects.curve_popup__popup_item5,action_delete_menu_clicked);

    if(objects.curve_popup__edited_obj_name)
        lv_label_set_text(
            objects.curve_popup__edited_obj_name,
            mix.name[0]!='\0'?mix.name:"Free mix"
        );

    // Сохраняем те же координаты, что у popup в Curve editor.
    lv_obj_set_pos(objects.curve_popup,0,0);
    lv_obj_remove_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN);
    showPopupDismissOverlay(objects.curve_popup);

    mixPopupEncoderIndex=0;
    applyMixPopupEncoderFocus();
}

static void prepareSharedEditDeletePopupForAction(uint8_t actionIndex){
    if(!objects.curve_popup||!objects.scr_mixes) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
    if(actionIndex>=MAX_MIX_ACTIONS) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    if(actionIndex>=mix.actionCount) return;
    if(!mix.actions[actionIndex].enabled) return;

    selectedMixActionIndex=(int8_t)actionIndex;
    sharedEditDeletePopupTarget=SharedEditDeletePopupTarget::ACTION;
    resetSharedPopupSelectionState();

    if(lv_obj_get_parent(objects.curve_popup)!=objects.scr_mixes)
        lv_obj_set_parent(objects.curve_popup,objects.scr_mixes);

    setSharedPopupItemVisible(objects.curve_popup__popup_item1,true);
    setSharedPopupItemVisible(objects.curve_popup__popup_item2,true);
    setSharedPopupItemVisible(objects.curve_popup__popup_item3,true);
    setSharedPopupItemVisible(objects.curve_popup__popup_item4,true);
    setSharedPopupItemVisible(objects.curve_popup__popup_item5,true);
    setSharedPopupItemVisible(objects.curve_popup__popup_item6,false);
    setSharedPopupItemVisible(objects.curve_popup__popup_item7,false);

    if(objects.curve_popup__popup_item3_lbl)
        lv_label_set_text(objects.curve_popup__popup_item3_lbl,"Add");

    if(objects.curve_popup__popup_item5_lbl)
        lv_label_set_text(objects.curve_popup__popup_item5_lbl,"Delete");

    if(objects.curve_popup__popup_item3){
        // In EEZ item3 is the normal Delete row. For Action popup it becomes Add.
        lv_obj_remove_event_cb(
            objects.curve_popup__popup_item3,
            action_delete_menu_clicked
        );
        lv_obj_remove_event_cb(
            objects.curve_popup__popup_item3,
            mixActionAddMenuClicked
        );
        lv_obj_add_event_cb(
            objects.curve_popup__popup_item3,
            mixActionAddMenuClicked,
            LV_EVENT_CLICKED,
            nullptr
        );
        mixActionAddPopupBound=true;
    }

    if(objects.curve_popup__popup_item5){
        lv_obj_remove_event_cb(
            objects.curve_popup__popup_item5,
            action_delete_menu_clicked
        );
        lv_obj_add_event_cb(
            objects.curve_popup__popup_item5,
            action_delete_menu_clicked,
            LV_EVENT_CLICKED,
            nullptr
        );
        mixActionDeletePopupBound=true;
    }

    if(objects.curve_popup__popup_item2){
        uint32_t childCount=lv_obj_get_child_count(objects.curve_popup__popup_item2);
        for(uint32_t i=0;i<childCount;i++){
            lv_obj_t *child=lv_obj_get_child(objects.curve_popup__popup_item2,i);
            if(child&&lv_obj_check_type(child,&lv_label_class)){
                lv_label_set_text(child,"Clone");
                break;
            }
        }

        if(!mixActionClonePopupBound){
            lv_obj_add_event_cb(
                objects.curve_popup__popup_item2,
                mixActionCloneMenuClicked,
                LV_EVENT_CLICKED,
                nullptr
            );
            mixActionClonePopupBound=true;
        }
    }

    if(objects.curve_popup__popup_item4_lbl)
        lv_label_set_text(objects.curve_popup__popup_item4_lbl,"Move");

    if(!mixActionMovePopupBound&&objects.curve_popup__popup_item4){
        lv_obj_add_event_cb(
            objects.curve_popup__popup_item4,
            mixActionMoveMenuClicked,
            LV_EVENT_CLICKED,
            nullptr
        );
        mixActionMovePopupBound=true;
    }

    if(objects.curve_popup__edited_obj_name)
        lv_label_set_text(objects.curve_popup__edited_obj_name,"Action");

    lv_obj_set_pos(objects.curve_popup,0,0);
    lv_obj_remove_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN);
    showPopupDismissOverlay(objects.curve_popup);

    mixPopupEncoderIndex=0;
    applyMixPopupEncoderFocus();
}

static void loadMixNameToEditor(){
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
    lv_obj_t *nameField=isAileronMixEditor()?objects.aileron_mix_name:objects.mix_name;
    if(!nameField) return;

    ModelConfig &model=modelManager.getActiveModel();
    const MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    lv_textarea_set_text(
        nameField,
        mix.name[0]!='\0'?mix.name:(isAileronMixEditor()?"Ailerons":"Free mix")
    );
}

static void saveMixNameFromEditor(){
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
    lv_obj_t *nameField=isAileronMixEditor()?objects.aileron_mix_name:objects.mix_name;
    if(!nameField) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    const char *name=lv_textarea_get_text(nameField);
    if(!name) name="";

    // Empty name falls back to the normal Free Mix name.
    if(name[0]=='\0')
        name=isAileronMixEditor()?"Ailerons":"Free mix";

    strncpy(mix.name,name,sizeof(mix.name)-1);
    mix.name[sizeof(mix.name)-1]='\0';

    // Keep textarea normalized to what was actually stored.
    lv_textarea_set_text(nameField,mix.name);
}

static void openExistingMixEditor(uint8_t mixIndex){
    if(mixIndex>=MAX_MIX_LINES) return;

    hideMixOperationPopup();

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[mixIndex];
    if(!mix.enabled) return;

    editingMixIndex=(int8_t)mixIndex;
    {
        char key[16];
        aileronPreferencesKey(key,sizeof(key),mixIndex);
        Preferences prefs;
        bool isAileron=false;
        if(prefs.begin("aileronMix",true)){
            isAileron=prefs.getBytesLength(key)==sizeof(AileronEditorState);
            prefs.end();
        }
        isAileron=isAileron||(mix.flightModeMask&MIX_AILERON_TYPE_FLAG)!=0;
        activeMixEditorKind=isAileron?MixEditorKind::AILERON:MixEditorKind::FREE_MIX;
        if(isAileron){
            mix.flightModeMask|=MIX_AILERON_TYPE_FLAG;
            loadAileronEditorState(mixIndex);
            syncAileronEngineState();
        }
    }
    editingMixActionIndex=-1;
    mixActionEditorIsNew=false;
    selectedMixIndex=(int8_t)mixIndex;

    setMixGraphActionPreview(true);

    if(objects.curve_popup)
        lv_obj_add_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN);
    hidePopupDismissOverlay();

    if(objects.all_mixes_panel)
        lv_obj_add_flag(objects.all_mixes_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.mix_main_panel)
        lv_obj_add_flag(objects.mix_main_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.mix_addedit_panel)
        lv_obj_remove_flag(objects.mix_addedit_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.free_mixes_edit_panel){
        if(isAileronMixEditor()) lv_obj_add_flag(objects.free_mixes_edit_panel,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(objects.free_mixes_edit_panel,LV_OBJ_FLAG_HIDDEN);
    }
    if(objects.aileron_mixes_edit_panel){
        if(isAileronMixEditor()) lv_obj_remove_flag(objects.aileron_mixes_edit_panel,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(objects.aileron_mixes_edit_panel,LV_OBJ_FLAG_HIDDEN);
    }
    if(isAileronMixEditor()) updateAileronEditorUi();

    if(objects.mix_edit_new_action_panel)
        lv_obj_add_flag(objects.mix_edit_new_action_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.action_type_selcet_panel)
        lv_obj_add_flag(objects.action_type_selcet_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.trim_type_select_panel)
        lv_obj_add_flag(objects.trim_type_select_panel,LV_OBJ_FLAG_HIDDEN);

    hideMixActionChanger();

    setupMixOutputUiCallbacks();
    updateMixOutputsUi();

    setupMixSourceSelectorCallbacks();
    if(!isAileronMixEditor()) updateMixSourceEditorUi();

    lv_obj_t *sourceSelectorRoot=getMixSourceSelectorRoot();
    if(sourceSelectorRoot)
        lv_obj_add_flag(sourceSelectorRoot,LV_OBJ_FLAG_HIDDEN);

    if(!isAileronMixEditor()){
        clearFreeMixActionList();
        rebuildFreeMixActionList();
    }

    if(objects.global_status_bar__caption)
        lv_label_set_text(objects.global_status_bar__caption,"Mixes");

    loadMixNameToEditor();
    updateMixActiveConditionEditorUi();
    updateMixSourceEditorUi();
    updateMixOperationEditorUi();
    drawMixGraph(mixIndex);
    resetMixEditorEncoderNavigation();
}

static void deleteSelectedMix(){
    if(selectedMixIndex<0||selectedMixIndex>=MAX_MIX_LINES) return;

    uint8_t mixIndex=(uint8_t)selectedMixIndex;
    ModelConfig &model=modelManager.getActiveModel();

    if(!model.mixes[mixIndex].enabled) return;

    model.mixes[mixIndex]=MixLine{};
    {
        char key[16];
        aileronPreferencesKey(key,sizeof(key),mixIndex);
        Preferences prefs;
        if(prefs.begin("aileronMix",false)){prefs.remove(key);prefs.end();}
    }

    if(editingMixIndex==(int8_t)mixIndex){
        editingMixIndex=-1;
        editingMixActionIndex=-1;
        mixActionEditorIsNew=false;
    }

    selectedMixIndex=-1;
    mixer.resetRuntime();

    if(objects.curve_popup)
        lv_obj_add_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN);
    hidePopupDismissOverlay();

    hidePopupDismissOverlay();

    modelManager.requestSaveActiveModel();
    refreshMixList();

    mixEncoderFocusIndex=MIX_ENCODER_FOCUS_ADD;
    applyMixEncoderFocus();
}

static void deleteSelectedMixAction(){
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
    if(selectedMixActionIndex<0||selectedMixActionIndex>=MAX_MIX_ACTIONS) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];
    uint8_t actionIndex=(uint8_t)selectedMixActionIndex;

    if(actionIndex>=mix.actionCount) return;

    for(uint8_t i=actionIndex;i+1<mix.actionCount;i++)
        mix.actions[i]=mix.actions[i+1];

    if(mix.actionCount>0){
        mix.actionCount--;
        mix.actions[mix.actionCount]=MixAction{};
    }

    if(editingMixActionIndex==(int8_t)actionIndex){
        editingMixActionIndex=-1;
        mixActionEditorIsNew=false;
    }else if(editingMixActionIndex>(int8_t)actionIndex){
        editingMixActionIndex--;
    }

    selectedMixActionIndex=-1;

    if(objects.curve_popup)
        lv_obj_add_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN);
    hidePopupDismissOverlay();

    rebuildFreeMixActionList();
    mixer.resetRuntime();

    // The edited mix is authoritative: the selection can refer to an
    // earlier list item after popup interactions.
    selectedMixIndex=editingMixIndex;
    drawMixGraph((uint8_t)editingMixIndex);
}

static void mixDeleteConfirmCb(lv_event_t *e){
    closeSystemConfirmation();
    deleteSelectedMix();
}

static void mixDeleteCancelCb(lv_event_t *e){
    closeSystemConfirmation();
}

static void mixActionDeleteConfirmCb(lv_event_t *e){
    closeSystemConfirmation();
    deleteSelectedMixAction();
}

static void mixActionDeleteCancelCb(lv_event_t *e){
    closeSystemConfirmation();
}

static void hideMixInfo(){
    selectedMixIndex=-1;
    clearMixGraph();
    if(objects.mix_type_lbl_info) lv_obj_add_flag(objects.mix_type_lbl_info,LV_OBJ_FLAG_HIDDEN);
    if(objects.mix_type_name_lbl) lv_obj_add_flag(objects.mix_type_name_lbl,LV_OBJ_FLAG_HIDDEN);
    if(objects.mix_ac_lbl_info) lv_obj_add_flag(objects.mix_ac_lbl_info,LV_OBJ_FLAG_HIDDEN);
    if(objects.mix_ac_name_lbl) lv_obj_add_flag(objects.mix_ac_name_lbl,LV_OBJ_FLAG_HIDDEN);
}

static void previewMixInfo(uint8_t mixIndex){
    if(mixIndex>=MAX_MIX_LINES) return;

    ModelConfig &model=modelManager.getActiveModel();
    const MixLine &mix=model.mixes[mixIndex];
    if(!mix.enabled) return;

    if(objects.mix_type_lbl_info)
        lv_obj_remove_flag(objects.mix_type_lbl_info,LV_OBJ_FLAG_HIDDEN);

    if(objects.mix_type_name_lbl){
        const char *typeName=(mix.flightModeMask&MIX_AILERON_TYPE_FLAG)!=0?"Aileron":"Free mix";
        lv_label_set_text(objects.mix_type_name_lbl,typeName);
        lv_obj_remove_flag(objects.mix_type_name_lbl,LV_OBJ_FLAG_HIDDEN);
    }

    if(objects.mix_ac_lbl_info)
        lv_obj_remove_flag(objects.mix_ac_lbl_info,LV_OBJ_FLAG_HIDDEN);

    if(objects.mix_ac_name_lbl){
        char conditionName[64];
        formatMixActiveConditionLabel(mix,conditionName,sizeof(conditionName));
        setConditionText(objects.mix_ac_name_lbl,conditionName);
        lv_obj_remove_flag(objects.mix_ac_name_lbl,LV_OBJ_FLAG_HIDDEN);
    }

    drawMixGraph(mixIndex);
}

static void showMixInfo(uint8_t mixIndex){
    if(mixIndex>=MAX_MIX_LINES) return;

    selectedMixIndex=(int8_t)mixIndex;
    previewMixInfo(mixIndex);
}


static int8_t findFirstEnabledMixIndex(){
    ModelConfig &model=modelManager.getActiveModel();

    for(uint8_t i=0;i<MAX_MIX_LINES;i++){
        if(model.mixes[i].enabled) return (int8_t)i;
    }

    return -1;
}

static int8_t findLastEnabledMixIndex(){
    ModelConfig &model=modelManager.getActiveModel();

    for(int16_t i=MAX_MIX_LINES-1;i>=0;i--){
        if(model.mixes[i].enabled) return (int8_t)i;
    }

    return -1;
}

static int8_t findNextEnabledMixIndex(int8_t current,int8_t direction){
    ModelConfig &model=modelManager.getActiveModel();

    // Navigation order:
    // Add -> first enabled Mix -> ... -> last enabled Mix.
    // No cyclic transition at either end.
    if(direction>0){
        if(current==MIX_ENCODER_FOCUS_ADD){
            int8_t first=findFirstEnabledMixIndex();
            return first>=0?first:MIX_ENCODER_FOCUS_ADD;
        }

        if(current==MIX_ENCODER_FOCUS_NONE){
            int8_t first=findFirstEnabledMixIndex();
            return first>=0?first:MIX_ENCODER_FOCUS_ADD;
        }

        for(int16_t i=(int16_t)current+1;i<MAX_MIX_LINES;i++){
            if(model.mixes[i].enabled) return (int8_t)i;
        }

        // Last item: stay on it.
        return current;
    }

    if(direction<0){
        if(current==MIX_ENCODER_FOCUS_ADD)
            return MIX_ENCODER_FOCUS_ADD;

        if(current==MIX_ENCODER_FOCUS_NONE)
            return MIX_ENCODER_FOCUS_ADD;

        for(int16_t i=(int16_t)current-1;i>=0;i--){
            if(model.mixes[i].enabled) return (int8_t)i;
        }

        // Above the first Mix is the Add label.
        return MIX_ENCODER_FOCUS_ADD;
    }

    return current;
}

static lv_obj_t *getMixListRowForMixIndex(uint8_t mixIndex){
    if(!objects.mix_list_panel||mixIndex>=MAX_MIX_LINES) return nullptr;

    ModelConfig &model=modelManager.getActiveModel();
    uint32_t childIndex=0;

    for(uint8_t i=0;i<MAX_MIX_LINES;i++){
        if(!model.mixes[i].enabled) continue;

        if(i==mixIndex)
            return lv_obj_get_child(objects.mix_list_panel,(int32_t)childIndex);

        childIndex++;
    }

    return nullptr;
}

static void applyMixEncoderFocus(){
    if(objects.mix_add_btn)
        lv_obj_remove_state(objects.mix_add_btn,LV_STATE_FOCUSED);

    if(objects.mix_list_panel){
        uint32_t childCount=lv_obj_get_child_count(objects.mix_list_panel);

        for(uint32_t i=0;i<childCount;i++){
            lv_obj_t *row=lv_obj_get_child(objects.mix_list_panel,(int32_t)i);
            if(row) lv_obj_remove_state(row,LV_STATE_FOCUSED);
        }
    }

    if(mixEncoderFocusIndex==MIX_ENCODER_FOCUS_ADD){
        if(objects.mix_add_btn)
            lv_obj_add_state(objects.mix_add_btn,LV_STATE_FOCUSED);

        // Add has no transfer graph.
        clearMixGraph();
        if(objects.mix_type_lbl_info)
            lv_obj_add_flag(objects.mix_type_lbl_info,LV_OBJ_FLAG_HIDDEN);
        if(objects.mix_ac_lbl_info)
            lv_obj_add_flag(objects.mix_ac_lbl_info,LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if(!objects.mix_list_panel) return;
    if(mixEncoderFocusIndex<0||mixEncoderFocusIndex>=MAX_MIX_LINES) return;

    lv_obj_t *row=getMixListRowForMixIndex((uint8_t)mixEncoderFocusIndex);
    if(!row) return;

    lv_obj_add_state(row,LV_STATE_FOCUSED);
    lv_obj_scroll_to_view(row,LV_ANIM_OFF);

    // Graph and Mix info follow encoder focus immediately.
    // No click/press is required for the preview.
    previewMixInfo((uint8_t)mixEncoderFocusIndex);
}


static uint8_t buildMixEditorEncoderItems(
    MixEditorEncoderItem *items,
    uint8_t maxItems
){
    if(!items||maxItems==0) return 0;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return 0;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    uint8_t count=0;

    auto addItem=[&](lv_obj_t *obj,MixEditorEncoderItemType type,uint8_t data=0){
        if(!obj||count>=maxItems) return;
        if(lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN)) return;

        items[count].obj=obj;
        items[count].type=type;
        items[count].data=data;
        count++;
    };

    addItem(objects.mix_name,MixEditorEncoderItemType::NAME);
    addItem(objects.mix_ac_panel,MixEditorEncoderItemType::ACTIVE_CONDITION);
    addItem(objects.mix_source_panel,MixEditorEncoderItemType::SOURCE);
    addItem(objects.mix_operation_panel,MixEditorEncoderItemType::OPERATION);

    if(objects.freemix_actions_list){
        uint8_t actionCount=mix.actionCount;
        if(actionCount>MAX_MIX_ACTIONS) actionCount=MAX_MIX_ACTIONS;

        uint32_t childIndex=0;

        for(uint8_t actionIndex=0;actionIndex<actionCount;actionIndex++){
            if(!mix.actions[actionIndex].enabled) continue;

            lv_obj_t *row=lv_obj_get_child(
                objects.freemix_actions_list,
                (int32_t)childIndex
            );
            childIndex++;

            addItem(
                row,
                MixEditorEncoderItemType::ACTION,
                actionIndex
            );
        }
    }

    addItem(
        objects.add_new_action_panel,
        MixEditorEncoderItemType::ADD_ACTION
    );

    addItem(
        objects.mix_output_count_panel,
        MixEditorEncoderItemType::OUTPUT_COUNT
    );

    uint8_t outputCount=mix.outputCount;
    if(outputCount<1) outputCount=1;
    if(outputCount>MAX_MIX_OUTPUTS) outputCount=MAX_MIX_OUTPUTS;

    for(uint8_t slot=0;slot<outputCount;slot++){
        lv_obj_t *root=getMixOutputRoot(slot);
        if(!root||lv_obj_has_flag(root,LV_OBJ_FLAG_HIDDEN)) continue;

        addItem(
            getMixOutputReverseSwitch(slot),
            MixEditorEncoderItemType::OUTPUT_REVERSE,
            slot
        );

        addItem(
            getMixOutputNamePanel(slot),
            MixEditorEncoderItemType::OUTPUT_CHANNEL,
            slot
        );
    }

    return count;
}

static bool isMixEditorMainVisible(){
    if(current_active_screen!=SCREEN_ID_SCR_MIXES) return false;

    if(objects.operation_panel&&
       !lv_obj_has_flag(objects.operation_panel,LV_OBJ_FLAG_HIDDEN))
        return false;

    if(!objects.mix_addedit_panel||
       lv_obj_has_flag(objects.mix_addedit_panel,LV_OBJ_FLAG_HIDDEN))
        return false;

    if(!objects.free_mixes_edit_panel||
       lv_obj_has_flag(objects.free_mixes_edit_panel,LV_OBJ_FLAG_HIDDEN))
        return false;

    if(objects.mix_edit_new_action_panel&&
       !lv_obj_has_flag(objects.mix_edit_new_action_panel,LV_OBJ_FLAG_HIDDEN))
        return false;

    lv_obj_t *sourceSelectorRoot=getMixSourceSelectorRoot();
    if(sourceSelectorRoot&&
       !lv_obj_has_flag(sourceSelectorRoot,LV_OBJ_FLAG_HIDDEN))
        return false;

    if(objects.channels_list_in_mix&&
       !lv_obj_has_flag(objects.channels_list_in_mix,LV_OBJ_FLAG_HIDDEN))
        return false;

    if(objects.mix_changer_panel&&
       !lv_obj_has_flag(objects.mix_changer_panel,LV_OBJ_FLAG_HIDDEN))
        return false;

    return editingMixIndex>=0&&editingMixIndex<MAX_MIX_LINES;
}

static const char *getMixOperationName(MixOperation operation){
    switch(operation){
        case MixOperation::ADD:
            return "Add";

        case MixOperation::MULTIPLY:
            return "Multiply";

        case MixOperation::REPLACE:
            return "Replace";

        case MixOperation::LOCK:
            return "Lock";
    }

    return "Add";
}

static void updateMixOperationEditorUi(){
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
    if(!objects.mix_operation_lbl) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixOperation operation=model.mixes[(uint8_t)editingMixIndex].operation;

    lv_label_set_text(
        objects.mix_operation_lbl,
        getMixOperationName(operation)
    );
}

static lv_obj_t *getMixOperationPopupItem(MixOperation operation){
    switch(operation){
        case MixOperation::ADD:
            return objects.add_operation_panel;

        case MixOperation::MULTIPLY:
            return objects.multiply_operation_panel;

        case MixOperation::REPLACE:
            return objects.replace_operation_panel;

        case MixOperation::LOCK:
            return objects.lock_operation_panel;
    }

    return nullptr;
}

static void updateMixOperationPopupFocus(){
    lv_obj_t *items[4]={
        objects.add_operation_panel,
        objects.multiply_operation_panel,
        objects.replace_operation_panel,
        objects.lock_operation_panel
    };

    for(uint8_t i=0;i<4;i++){
        if(items[i])
            lv_obj_remove_state(items[i],LV_STATE_FOCUSED);
    }

    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    ModelConfig &model=modelManager.getActiveModel();
    lv_obj_t *selected=getMixOperationPopupItem(
        model.mixes[(uint8_t)editingMixIndex].operation
    );

    if(selected)
        lv_obj_add_state(selected,LV_STATE_FOCUSED);
}

static void hideMixOperationPopup(){
    if(objects.operation_panel)
        lv_obj_add_flag(objects.operation_panel,LV_OBJ_FLAG_HIDDEN);

    hidePopupDismissOverlay();
}

static void syncMixEditorEncoderIndexFromCurrentFocus(
    MixEditorEncoderItem *items,
    uint8_t count
){
    if(!items||count==0) return;

    for(uint8_t i=0;i<count;i++){
        lv_obj_t *obj=items[i].obj;
        if(!obj) continue;
        if(!lv_obj_has_state(obj,LV_STATE_FOCUSED)) continue;

        mixEditorEncoderIndex=(int16_t)i;
        return;
    }
}

static void mixEditorNavigationTouchEvent(lv_event_t *e){
    if(!e) return;
    if(!isMixEditorMainVisible()) return;

    lv_obj_t *target=(lv_obj_t *)lv_event_get_current_target(e);
    if(!target) return;

    MixEditorEncoderItem items[MAX_MIX_EDITOR_ENCODER_ITEMS];
    uint8_t count=buildMixEditorEncoderItems(items,MAX_MIX_EDITOR_ENCODER_ITEMS);
    if(count==0) return;

    for(uint8_t i=0;i<count;i++){
        if(items[i].obj!=target) continue;

        // Touch and encoder share the same navigation cursor.
        // After touching any editor component, the next encoder step starts
        // from that exact component instead of from the old encoder position.
        mixEditorEncoderIndex=(int16_t)i;
        if(items[i].type!=MixEditorEncoderItemType::ACTION)
            focusedMixActionIndex=-1;
        return;
    }
}

static void bindMixEditorNavigationTouchTracking(
    MixEditorEncoderItem *items,
    uint8_t count
){
    if(!items||count==0) return;

    for(uint8_t i=0;i<count;i++){
        lv_obj_t *obj=items[i].obj;
        if(!obj) continue;

        // This can be called after rebuilding dynamic Action rows.
        // Remove the previous identical binding before adding it again.
        lv_obj_remove_event_cb(obj,mixEditorNavigationTouchEvent);
        lv_obj_add_event_cb(
            obj,
            mixEditorNavigationTouchEvent,
            LV_EVENT_PRESSED,
            nullptr
        );
    }
}

static void applyMixEditorEncoderFocus(){
    MixEditorEncoderItem items[MAX_MIX_EDITOR_ENCODER_ITEMS];
    uint8_t count=buildMixEditorEncoderItems(items,MAX_MIX_EDITOR_ENCODER_ITEMS);
    if(count==0) return;

    bindMixEditorNavigationTouchTracking(items,count);

    if(mixEditorEncoderIndex<0) mixEditorEncoderIndex=0;
    if(mixEditorEncoderIndex>=count) mixEditorEncoderIndex=count-1;

    for(uint8_t i=0;i<count;i++)
        if(items[i].obj)
            lv_obj_remove_state(items[i].obj,LV_STATE_FOCUSED);

    MixEditorEncoderItem &selected=items[mixEditorEncoderIndex];
    lv_obj_t *focused=selected.obj;
    if(!focused) return;

    lv_obj_add_state(focused,LV_STATE_FOCUSED);

    if(selected.type!=MixEditorEncoderItemType::ACTION)
        lv_obj_scroll_to_view(focused,LV_ANIM_OFF);
}

static void mixEditorEncoderMove(int8_t direction){
    if(!isMixEditorMainVisible()) return;

    MixEditorEncoderItem items[MAX_MIX_EDITOR_ENCODER_ITEMS];
    uint8_t count=buildMixEditorEncoderItems(items,MAX_MIX_EDITOR_ENCODER_ITEMS);
    if(count==0) return;

    syncMixEditorEncoderIndexFromCurrentFocus(items,count);

    if(mixEditorEncoderIndex<0) mixEditorEncoderIndex=0;
    if(mixEditorEncoderIndex>=count) mixEditorEncoderIndex=count-1;

    if(direction>0){
        if(mixEditorEncoderIndex<count-1) mixEditorEncoderIndex++;
    }else if(direction<0){
        if(mixEditorEncoderIndex>0) mixEditorEncoderIndex--;
    }

    applyMixEditorEncoderFocus();
}

static void mixEditorEncoderPressed(){
    if(!isMixEditorMainVisible()) return;

    MixEditorEncoderItem items[MAX_MIX_EDITOR_ENCODER_ITEMS];
    uint8_t count=buildMixEditorEncoderItems(
        items,
        MAX_MIX_EDITOR_ENCODER_ITEMS
    );

    if(count==0) return;

    syncMixEditorEncoderIndexFromCurrentFocus(items,count);

    if(mixEditorEncoderIndex<0)
        mixEditorEncoderIndex=0;

    if(mixEditorEncoderIndex>=count)
        mixEditorEncoderIndex=count-1;

    MixEditorEncoderItem &item=items[mixEditorEncoderIndex];
    if(!item.obj) return;

    switch(item.type){
        case MixEditorEncoderItemType::NAME:
            // First encoder item: start text editing with the shared keyboard.
            lv_obj_add_state(item.obj,LV_STATE_FOCUSED);
            lv_obj_send_event(item.obj,LV_EVENT_FOCUSED,nullptr);
            break;

        case MixEditorEncoderItemType::ACTIVE_CONDITION:
            // Condition selector backend is not connected yet.
            // Keep the object in navigation and forward the normal click event.
            lv_obj_send_event(item.obj,LV_EVENT_CLICKED,nullptr);
            break;

        case MixEditorEncoderItemType::SOURCE:
            lv_obj_send_event(item.obj,LV_EVENT_CLICKED,nullptr);
            break;

        case MixEditorEncoderItemType::OPERATION:
            lv_obj_send_event(item.obj,LV_EVENT_CLICKED,nullptr);
            break;

        case MixEditorEncoderItemType::ACTION:{
            uint8_t mixIndex=(uint8_t)editingMixIndex;
            uint8_t actionIndex=item.data;

            ModelConfig &model=modelManager.getActiveModel();
            const MixLine &mix=model.mixes[mixIndex];

            if(actionIndex>=mix.actionCount||actionIndex>=MAX_MIX_ACTIONS) break;

            pendingMixActionMixIndex=mixIndex;
            pendingMixActionIndex=actionIndex;

            if(!mixActionOpenPending){
                mixActionOpenPending=true;
                lv_async_call(openMixActionEditorAsync,nullptr);
            }
            break;
        }

        case MixEditorEncoderItemType::ADD_ACTION:
            lv_obj_send_event(item.obj,LV_EVENT_CLICKED,nullptr);
            break;

        case MixEditorEncoderItemType::OUTPUT_COUNT:
            lv_obj_send_event(item.obj,LV_EVENT_CLICKED,nullptr);
            break;

        case MixEditorEncoderItemType::OUTPUT_CHANNEL:
            lv_obj_send_event(item.obj,LV_EVENT_CLICKED,nullptr);
            break;

        case MixEditorEncoderItemType::OUTPUT_REVERSE:{
            bool checked=lv_obj_has_state(item.obj,LV_STATE_CHECKED);

            if(checked)
                lv_obj_remove_state(item.obj,LV_STATE_CHECKED);
            else
                lv_obj_add_state(item.obj,LV_STATE_CHECKED);

            // Existing callback stores the new state in MixOutput.
            lv_obj_send_event(item.obj,LV_EVENT_VALUE_CHANGED,nullptr);
            break;
        }
    }
}

static void resetMixEditorEncoderNavigation(){
    mixEditorEncoderIndex=0;

    if(objects.free_mixes_edit_panel)
        lv_obj_scroll_to_y(
            objects.free_mixes_edit_panel,
            0,
            LV_ANIM_OFF
        );

    updateMixOperationEditorUi();
    applyMixEditorEncoderFocus();
}

static bool isMixPopupVisible(){
    if(!objects.curve_popup) return false;
    if(lv_obj_get_parent(objects.curve_popup)!=objects.scr_mixes) return false;
    return !lv_obj_has_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN);
}

static bool isMixListVisible(){
    if(current_active_screen!=SCREEN_ID_SCR_MIXES) return false;
    if(!objects.mix_main_panel||lv_obj_has_flag(objects.mix_main_panel,LV_OBJ_FLAG_HIDDEN)) return false;
    if(objects.mix_addedit_panel&&!lv_obj_has_flag(objects.mix_addedit_panel,LV_OBJ_FLAG_HIDDEN)) return false;
    if(objects.all_mixes_panel&&!lv_obj_has_flag(objects.all_mixes_panel,LV_OBJ_FLAG_HIDDEN)) return false;
    return true;
}

static void applyMixPopupEncoderFocus(){
    if(objects.curve_popup__popup_item1)
        lv_obj_remove_state(objects.curve_popup__popup_item1,LV_STATE_FOCUSED);

    if(objects.curve_popup__popup_item3)
        lv_obj_remove_state(objects.curve_popup__popup_item3,LV_STATE_FOCUSED);

    lv_obj_t *selected=nullptr;

    if(mixPopupEncoderIndex==0)
        selected=objects.curve_popup__popup_item1;
    else
        selected=objects.curve_popup__popup_item3;

    if(selected)
        lv_obj_add_state(selected,LV_STATE_FOCUSED);
}

static void mixEncoderMoveList(int8_t direction){
    if(!isMixListVisible()) return;

    mixEncoderFocusIndex=findNextEnabledMixIndex(mixEncoderFocusIndex,direction);
    applyMixEncoderFocus();
}

static void mixEncoderMovePopup(int8_t direction){
    if(!isMixPopupVisible()) return;

    if(direction!=0)
        mixPopupEncoderIndex=(mixPopupEncoderIndex==0)?1:0;

    applyMixPopupEncoderFocus();
}

static void mixEncoderLeft(){
    if(isMixSourceSelectorVisible()&&moveMixSourceMemberFocus(-1))
        return;

    if(isMixPopupVisible()){
        mixEncoderMovePopup(-1);
        return;
    }

    if(isMixEditorMainVisible()){
        mixEditorEncoderMove(-1);
        return;
    }

    mixEncoderMoveList(-1);
}

static void mixEncoderRight(){
    if(isMixSourceSelectorVisible()&&moveMixSourceMemberFocus(1))
        return;

    if(isMixPopupVisible()){
        mixEncoderMovePopup(1);
        return;
    }

    if(isMixEditorMainVisible()){
        mixEditorEncoderMove(1);
        return;
    }

    mixEncoderMoveList(1);
}

static void mixEncoderPressed(){
    if(isMixSourceSelectorVisible()&&pressFocusedMixSourceMember())
        return;

    if(isMixPopupVisible()){
        lv_obj_t *selected=(mixPopupEncoderIndex==0)?
            objects.curve_popup__popup_item1:
            objects.curve_popup__popup_item3;

        if(selected)
            lv_obj_send_event(selected,LV_EVENT_CLICKED,nullptr);

        return;
    }

    if(isMixEditorMainVisible()){
        mixEditorEncoderPressed();
        return;
    }

    if(!isMixListVisible()) return;

    if(mixEncoderFocusIndex==MIX_ENCODER_FOCUS_ADD){
        if(objects.mix_add_btn)
            lv_obj_send_event(objects.mix_add_btn,LV_EVENT_CLICKED,nullptr);
        return;
    }

    if(mixEncoderFocusIndex==MIX_ENCODER_FOCUS_NONE){
        int8_t first=findFirstEnabledMixIndex();
        mixEncoderFocusIndex=first>=0?first:MIX_ENCODER_FOCUS_ADD;
        applyMixEncoderFocus();

        if(mixEncoderFocusIndex==MIX_ENCODER_FOCUS_ADD) return;
    }

    if(mixEncoderFocusIndex<0||mixEncoderFocusIndex>=MAX_MIX_LINES) return;

    lv_obj_t *row=getMixListRowForMixIndex((uint8_t)mixEncoderFocusIndex);
    if(!row) return;

    // Graph/info already follow encoder focus, so one press is enough:
    // make the focused Mix the active selection and open Edit/Delete immediately.
    setExclusiveChecked(objects.mix_list_panel,row);
    selectedMixIndex=mixEncoderFocusIndex;

    prepareSharedEditDeletePopupForMix((uint8_t)mixEncoderFocusIndex);
    mixPopupEncoderIndex=0;
    applyMixPopupEncoderFocus();
}

static void mixRowClicked(lv_event_t *e){
    if(!e) return;
    lv_obj_t *row=(lv_obj_t *)lv_event_get_current_target(e);
    if(!row) row=(lv_obj_t *)lv_event_get_target(e);
    if(!row) return;

    uint8_t mixIndex=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(mixIndex>=MAX_MIX_LINES) return;

    mixEncoderFocusIndex=(int8_t)mixIndex;
    applyMixEncoderFocus();

    // Первый tap выбирает Mix и показывает график.
    // Повторный tap по уже выбранному Mix открывает общий Edit/Delete popup.
    if(selectedMixIndex==(int8_t)mixIndex){
        setExclusiveChecked(objects.mix_list_panel,row);
        prepareSharedEditDeletePopupForMix(mixIndex);
        return;
    }

    setExclusiveChecked(objects.mix_list_panel,row);
    showMixInfo(mixIndex);
}

static void refreshMixList(){
    if(!objects.mix_list_panel) return;

    lv_obj_clean(objects.mix_list_panel);
    hideMixInfo();

    ModelConfig &model=modelManager.getActiveModel();
    uint8_t visibleRow=0;

    for(uint8_t i=0;i<MAX_MIX_LINES;i++){
        const MixLine &mix=model.mixes[i];
        if(!mix.enabled) continue;

        lv_obj_t *row=lv_obj_create(objects.mix_list_panel);
        lv_obj_set_size(row,LV_PCT(98),50);
        lv_obj_set_style_pad_left(row,0,LV_PART_MAIN);
        lv_obj_set_style_pad_top(row,0,LV_PART_MAIN);
        lv_obj_set_style_pad_right(row,0,LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(row,0,LV_PART_MAIN);
        lv_obj_set_style_border_width(row,0,LV_PART_MAIN);
        lv_obj_set_style_radius(row,0,LV_PART_MAIN);
        lv_obj_remove_flag(row,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(row,LV_OBJ_FLAG_CHECKABLE);

        uint32_t defaultColor=(visibleRow&1)?
            theme_colors[active_theme_index][COLOR_ID_FRONT_BG1]:
            theme_colors[active_theme_index][COLOR_ID_FRONT_BG];

        lv_obj_set_style_bg_color(row,lv_color_hex(defaultColor),LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row,255,LV_PART_MAIN);
        lv_obj_set_style_text_color(row,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_TEXT]),LV_PART_MAIN);
        lv_obj_set_style_text_font(row,&lv_font_montserrat_20,LV_PART_MAIN);

        lv_obj_set_style_bg_color(row,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_MS_TEXT]),(lv_style_selector_t)((uint32_t)LV_PART_MAIN | (uint32_t)LV_STATE_FOCUSED));
        lv_obj_set_style_bg_opa(row,255,(lv_style_selector_t)((uint32_t)LV_PART_MAIN | (uint32_t)LV_STATE_FOCUSED));
        lv_obj_set_style_text_color(row,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_BG]),(lv_style_selector_t)((uint32_t)LV_PART_MAIN | (uint32_t)LV_STATE_FOCUSED));
        lv_obj_set_style_text_font(row,&lv_font_montserrat_20,(lv_style_selector_t)((uint32_t)LV_PART_MAIN | (uint32_t)LV_STATE_FOCUSED));

        // CHECKED используется как постоянное выделение выбранной строки при touch.
        lv_obj_set_style_bg_color(row,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_MS_TEXT]),(lv_style_selector_t)((uint32_t)LV_PART_MAIN | (uint32_t)LV_STATE_CHECKED));
        lv_obj_set_style_bg_opa(row,255,(lv_style_selector_t)((uint32_t)LV_PART_MAIN | (uint32_t)LV_STATE_CHECKED));
        lv_obj_set_style_text_color(row,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_BG]),(lv_style_selector_t)((uint32_t)LV_PART_MAIN | (uint32_t)LV_STATE_CHECKED));
        lv_obj_set_style_text_font(row,&lv_font_montserrat_20,(lv_style_selector_t)((uint32_t)LV_PART_MAIN | (uint32_t)LV_STATE_CHECKED));

        // mixRowClicked manages exclusive row selection and first/second tap semantics.
        lv_obj_add_event_cb(row,exclusiveListRowPressed,LV_EVENT_PRESSED,nullptr);
        lv_obj_add_event_cb(row,mixRowClicked,LV_EVENT_CLICKED,(void *)(uintptr_t)i);

        lv_obj_t *nameLabel=lv_label_create(row);
        lv_obj_set_pos(nameLabel,10,14);
        lv_obj_set_size(nameLabel,175,LV_SIZE_CONTENT);
        lv_label_set_long_mode(nameLabel,LV_LABEL_LONG_CLIP);
        lv_label_set_text(nameLabel,mix.name[0]!='\0'?mix.name:((mix.flightModeMask&MIX_AILERON_TYPE_FLAG)!=0?"Ailerons":"Free mix"));

        lv_obj_t *channelsLabel=lv_label_create(row);
        lv_obj_set_pos(channelsLabel,200,14);
        lv_obj_set_size(channelsLabel,160,LV_SIZE_CONTENT);
        lv_label_set_long_mode(channelsLabel,LV_LABEL_LONG_CLIP);

        char outputChannels[64];
        outputChannels[0]='\0';
        uint8_t outputCount=mix.outputCount;
        if(outputCount>MAX_MIX_OUTPUTS) outputCount=MAX_MIX_OUTPUTS;

        for(uint8_t outputIndex=0;outputIndex<outputCount;outputIndex++){
            uint8_t channel=mix.mixOutputs[outputIndex].channel;
            if(channel>=MAX_MODEL_CHANNELS) continue;

            char part[8];
            snprintf(part,sizeof(part),"%s%u",outputChannels[0]? ",":"",(unsigned)(channel+1));
            strncat(outputChannels,part,sizeof(outputChannels)-strlen(outputChannels)-1);
        }

        lv_label_set_text(channelsLabel,outputChannels[0]?outputChannels:"None");

        lv_obj_t *sourceLabel=lv_label_create(row);
        lv_obj_set_pos(sourceLabel,370,14);
        lv_obj_set_size(sourceLabel,191,LV_SIZE_CONTENT);
        lv_label_set_long_mode(sourceLabel,LV_LABEL_LONG_CLIP);
        char sourceName[32];
        formatMixSourceName(mix.source,sourceName,sizeof(sourceName));

        if(mix.reversed&&strcmp(sourceName,"---")!=0){
            char displaySourceName[34];
            snprintf(displaySourceName,sizeof(displaySourceName),"-%s",sourceName);
            lv_label_set_text(sourceLabel,displaySourceName);
        }else{
            lv_label_set_text(sourceLabel,sourceName);
        }

        visibleRow++;
    }
}

void action_open_scr_mixes(lv_event_t *e){
    if(current_active_screen!=SCREEN_ID_SCR_MIXES)
        mixes_return_screen=current_active_screen;

    openScreen(SCREEN_ID_SCR_MIXES);
    ensureInputRuntimeStarted();

    // Mix list owns mix_diag through mix_diag_panel.
    setMixGraphActionPreview(false);

    if(objects.mix_addedit_panel!=NULL)
        lv_obj_add_flag(objects.mix_addedit_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.all_mixes_panel!=NULL)
        lv_obj_add_flag(objects.all_mixes_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.mix_main_panel!=NULL)
        lv_obj_remove_flag(objects.mix_main_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.global_status_bar__caption!=NULL)
        lv_label_set_text(objects.global_status_bar__caption,"Mixes");

    setupMixOutputUiCallbacks();
    refreshMixList();

    mixEncoderFocusIndex=MIX_ENCODER_FOCUS_ADD;
    applyMixEncoderFocus();

    if(mixGraphTimer==nullptr)
        mixGraphTimer=lv_timer_create(updateMixGraphLive,50,nullptr);
}


// EEZ autogenerated objNN identifiers change when widgets are added.
// Locate Mix Options rows by their label rather than a numeric objNN name.
static lv_obj_t *findMixOptionRow(const char *caption){
    if(!objects.mix_invert_panel||!caption) return nullptr;
    const uint32_t n=lv_obj_get_child_count(objects.mix_invert_panel);
    for(uint32_t i=0;i<n;i++){
        lv_obj_t *row=lv_obj_get_child(objects.mix_invert_panel,i);
        if(!row) continue;
        const uint32_t children=lv_obj_get_child_count(row);
        for(uint32_t j=0;j<children;j++){
            lv_obj_t *child=lv_obj_get_child(row,j);
            if(child&&lv_obj_check_type(child,&lv_label_class)&&strcmp(lv_label_get_text(child),caption)==0) return row;
        }
    }
    return nullptr;
}

static lv_obj_t *getMixInvertRow(){return findMixOptionRow("Invert");}

static lv_obj_t *getActiveConditionInvertCheckbox(){
    lv_obj_t *row=getMixInvertRow();
    if(!row) return nullptr;
    const uint32_t n=lv_obj_get_child_count(row);
    for(uint32_t i=0;i<n;i++){
        lv_obj_t *child=lv_obj_get_child(row,i);
        if(child&&lv_obj_check_type(child,&lv_checkbox_class)) return child;
    }
    return nullptr;
}

static void styleMixOptionRow(lv_obj_t *row){
    if(!row) return;
    lv_obj_add_flag(row,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(row,lv_color_hex(theme_colors[active_theme_index][3]),LV_PART_MAIN|LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(row,lv_color_hex(theme_colors[active_theme_index][9]),LV_PART_MAIN|LV_STATE_FOCUSED);
}

static void ensureSourceHalfRangeRow(){
    // Half range is already defined in EEZ Studio as objects.obj221.
    // Never create a second row programmatically.
    if(!objects.mix_invert_panel||!objects.obj221) return;
    sourceHalfRangeRow=objects.obj221;
    sourceHalfRangeCheckbox=nullptr;
    const uint32_t count=lv_obj_get_child_count(sourceHalfRangeRow);
    for(uint32_t i=0;i<count;i++){
        lv_obj_t *child=lv_obj_get_child(sourceHalfRangeRow,i);
        if(child&&lv_obj_check_type(child,&lv_checkbox_class)){
            sourceHalfRangeCheckbox=child;
            break;
        }
    }
    styleMixOptionRow(sourceHalfRangeRow);
}

static void setSourceHalfRangeRowVisible(bool visible){
    ensureSourceHalfRangeRow();
    if(!sourceHalfRangeRow||!objects.mix_invert_panel) return;

    // Base height already includes the Options title and Invert row.
    // The EEZ Half range row is initially hidden, so subtracting its height
    // when hiding it clipped the title (80 px became 40 px).
    if(visible) lv_obj_remove_flag(sourceHalfRangeRow,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(sourceHalfRangeRow,LV_OBJ_FLAG_HIDDEN);

    // The EEZ popup uses LV_SIZE_CONTENT with Flex Column. Leave its height
    // controlled by layout so the Half range row is not clipped.
    lv_obj_set_height(objects.mix_invert_panel,LV_SIZE_CONTENT);
    lv_obj_update_layout(objects.mix_invert_panel);
}

static void sourceHalfRangeChanged(lv_event_t *e){
    if(!e||!sourceHalfRangeCheckbox||!sourceHalfRangeRow) return;
    if(invertTargetMode!=InvertTargetMode::SOURCE) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
    if(lv_event_get_code(e)!=LV_EVENT_CLICKED) return;
    if((lv_obj_t *)lv_event_get_current_target(e)!=sourceHalfRangeRow) return;

    if(mixOptionSelectedRow!=sourceHalfRangeRow){
        if(mixOptionSelectedRow) lv_obj_remove_state(mixOptionSelectedRow,LV_STATE_FOCUSED);
        mixOptionSelectedRow=sourceHalfRangeRow;
        lv_obj_add_state(sourceHalfRangeRow,LV_STATE_FOCUSED);
        return;
    }

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];
    if(!mixSourceSupportsHalfRange(mix)) return;
    bool enabled=!mixSourceHalfRangeEnabled(mix);
    setMixSourceHalfRange(mix,enabled);
    if(enabled) lv_obj_add_state(sourceHalfRangeCheckbox,LV_STATE_CHECKED);
    else lv_obj_remove_state(sourceHalfRangeCheckbox,LV_STATE_CHECKED);
    syncMixDirectionConditionsToSource(mix);
    updateMixSourceEditorUi();
    mixer.resetRuntime();
    drawMixGraph((uint8_t)editingMixIndex);
}

void action_set_half_range_ignore_range(lv_event_t *e){
    sourceHalfRangeChanged(e);
}

void action_invert_panel_show(lv_event_t *e){
    if(!e||!objects.mix_invert_panel) return;

    lv_obj_t *target=(lv_obj_t *)lv_event_get_current_target(e);
    MixActionType variantType;
    uint8_t variantSlot=0;

    if(target==objects.mix_source_panel){
        invertTargetMode=InvertTargetMode::SOURCE;
        suppressMixSourceClick=true;
    }else if(target==objects.mix_action_ac_panel){
        invertTargetMode=InvertTargetMode::ACTION;
        suppressActionActiveConditionClick=true;
    }else if(getMixActionVariantConditionTarget(target,variantType,variantSlot)){
        invertTargetMode=InvertTargetMode::VARIANT;
        activeMixVariantConditionType=variantType;
        activeMixVariantConditionSlot=variantSlot;
        suppressVariantActiveConditionClick=true;
    }else{
        invertTargetMode=InvertTargetMode::MIX;
        suppressMixActiveConditionClick=true;
    }

    lv_obj_t *invertRow=getMixInvertRow();
    styleMixOptionRow(invertRow);
    ensureSourceHalfRangeRow();
    styleMixOptionRow(sourceHalfRangeRow);
    lv_obj_t *checkbox=getActiveConditionInvertCheckbox();
    if(mixOptionSelectedRow) lv_obj_remove_state(mixOptionSelectedRow,LV_STATE_FOCUSED);
    mixOptionSelectedRow=nullptr;
    if(getMixInvertRow()) lv_obj_remove_state(getMixInvertRow(),LV_STATE_FOCUSED);
    if(checkbox){
        lv_obj_remove_flag(checkbox,(lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_CHECKABLE));
    }
    if(sourceHalfRangeRow) lv_obj_remove_state(sourceHalfRangeRow,LV_STATE_FOCUSED);
    if(sourceHalfRangeCheckbox) lv_obj_remove_flag(sourceHalfRangeCheckbox,(lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_CHECKABLE));

    if(invertTargetMode==InvertTargetMode::SOURCE){
        if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

        ModelConfig &model=modelManager.getActiveModel();
        MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

        if(checkbox){
            if(mix.reversed) lv_obj_add_state(checkbox,LV_STATE_CHECKED);
            else lv_obj_remove_state(checkbox,LV_STATE_CHECKED);
        }

        bool showHalfRange=mixSourceSupportsHalfRange(mix);
        setSourceHalfRangeRowVisible(showHalfRange);

        if(sourceHalfRangeCheckbox){
            if(mixSourceHalfRangeEnabled(mix))
                lv_obj_add_state(sourceHalfRangeCheckbox,LV_STATE_CHECKED);
            else
                lv_obj_remove_state(sourceHalfRangeCheckbox,LV_STATE_CHECKED);
        }

        if(getMixInvertRow())
            lv_obj_add_flag(getMixInvertRow(),LV_OBJ_FLAG_CLICKABLE);

        lv_obj_remove_flag(objects.mix_invert_panel,LV_OBJ_FLAG_HIDDEN);
        showPopupDismissOverlay(objects.mix_invert_panel);
        return;
    }

    setSourceHalfRangeRowVisible(false);

    ConditionConfig *condition=nullptr;
    if(invertTargetMode==InvertTargetMode::ACTION)
        condition=&mixActionEditorCondition;
    else if(invertTargetMode==InvertTargetMode::VARIANT){
        if(activeMixVariantConditionSlot<MAX_MIX_ACTION_VARIANTS)
            condition=isAileronMixEditor()?(activeMixVariantConditionType==MixActionType::WEIGHT?&aileronEditorState.weightConditions[activeMixVariantConditionSlot]:&aileronEditorState.curveConditions[activeMixVariantConditionSlot]):&mixActionVariantConditions[activeMixVariantConditionSlot];
    }else if(editingMixIndex>=0&&editingMixIndex<MAX_MIX_LINES){
        ModelConfig &model=modelManager.getActiveModel();
        condition=&model.mixes[(uint8_t)editingMixIndex].condition;
    }

    if(!condition) return;
    if(condition->type==ConditionType::NEVER||condition->type==ConditionType::NONE) return;

    if(checkbox){
        if(condition->inverted) lv_obj_add_state(checkbox,LV_STATE_CHECKED);
        else lv_obj_remove_state(checkbox,LV_STATE_CHECKED);
    }

    if(getMixInvertRow())
        lv_obj_add_flag(getMixInvertRow(),LV_OBJ_FLAG_CLICKABLE);

    lv_obj_remove_flag(objects.mix_invert_panel,LV_OBJ_FLAG_HIDDEN);
    showPopupDismissOverlay(objects.mix_invert_panel);
}

void action_set_invert(lv_event_t *e){
    if(!e||!getMixInvertRow()) return;
    if(lv_event_get_code(e)!=LV_EVENT_CLICKED) return;
    if((lv_obj_t *)lv_event_get_current_target(e)!=getMixInvertRow()) return;
    lv_obj_t *checkbox=getActiveConditionInvertCheckbox();
    if(!checkbox) return;

    // First click selects the row. Only clicking it again changes Invert.
    // Checkbox is display-only; its LVGL VALUE_CHANGED event must not bypass selection.
    if(mixOptionSelectedRow!=getMixInvertRow()){
        if(mixOptionSelectedRow) lv_obj_remove_state(mixOptionSelectedRow,LV_STATE_FOCUSED);
        mixOptionSelectedRow=getMixInvertRow();
        lv_obj_add_state(getMixInvertRow(),LV_STATE_FOCUSED);
        return;
    }

    if(invertTargetMode==InvertTargetMode::SOURCE){
        if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
        ModelConfig &model=modelManager.getActiveModel();
        MixLine &mix=model.mixes[(uint8_t)editingMixIndex];
        mix.reversed=!mix.reversed;
        if(mix.reversed) lv_obj_add_state(checkbox,LV_STATE_CHECKED);
        else lv_obj_remove_state(checkbox,LV_STATE_CHECKED);
        updateMixSourceEditorUi();
        mixer.resetRuntime();
        drawMixGraph((uint8_t)editingMixIndex);
        return;
    }

    ConditionConfig *condition=nullptr;
    if(invertTargetMode==InvertTargetMode::ACTION)
        condition=&mixActionEditorCondition;
    else if(invertTargetMode==InvertTargetMode::VARIANT){
        if(activeMixVariantConditionSlot<MAX_MIX_ACTION_VARIANTS)
            condition=isAileronMixEditor()?(activeMixVariantConditionType==MixActionType::WEIGHT?&aileronEditorState.weightConditions[activeMixVariantConditionSlot]:&aileronEditorState.curveConditions[activeMixVariantConditionSlot]):&mixActionVariantConditions[activeMixVariantConditionSlot];
    }else if(editingMixIndex>=0&&editingMixIndex<MAX_MIX_LINES){
        ModelConfig &model=modelManager.getActiveModel();
        condition=&model.mixes[(uint8_t)editingMixIndex].condition;
    }

    if(!condition) return;
    if(condition->type==ConditionType::NEVER||condition->type==ConditionType::NONE) return;
    condition->inverted=!condition->inverted;
    if(condition->inverted) lv_obj_add_state(checkbox,LV_STATE_CHECKED);
    else lv_obj_remove_state(checkbox,LV_STATE_CHECKED);

    if(invertTargetMode==InvertTargetMode::ACTION)
        updateMixActionConditionEditorUi();
    else if(invertTargetMode==InvertTargetMode::VARIANT)
        updateMixActionVariantConditionEditorUi(activeMixVariantConditionType,activeMixVariantConditionSlot);
    else
        updateMixActiveConditionEditorUi();

    if(isAileronMixEditor()) saveAileronEditorState();
    mixer.resetRuntime();
    if(editingMixIndex>=0) drawMixGraph((uint8_t)editingMixIndex);
}

static void showMixOperationPopupAsync(void *param){
    (void)param;
    mixOperationPopupShowPending=false;

    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
    if(!objects.operation_panel) return;

    updateMixOperationEditorUi();
    updateMixOperationPopupFocus();

    lv_obj_remove_flag(objects.operation_panel,LV_OBJ_FLAG_HIDDEN);
    showPopupDismissOverlay(objects.operation_panel);
}

void action_operation_panel_show(lv_event_t *e){
    (void)e;

    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
    if(!objects.operation_panel) return;
    if(mixOperationPopupShowPending) return;

    mixOperationPopupShowPending=true;
    lv_async_call(showMixOperationPopupAsync,nullptr);
}

void action_operation_selected(lv_event_t *e){
    if(!e) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    uint8_t operationIndex=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(operationIndex>3) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    switch(operationIndex){
        case 0:
            mix.operation=MixOperation::ADD;
            break;

        case 1:
            mix.operation=MixOperation::MULTIPLY;
            break;

        case 2:
            mix.operation=MixOperation::REPLACE;
            break;

        case 3:
            mix.operation=MixOperation::LOCK;
            break;
    }

    updateMixOperationEditorUi();
    updateMixOperationPopupFocus();
    hideMixOperationPopup();

    mixer.resetRuntime();

    if(editingMixIndex>=0)
        drawMixGraph((uint8_t)editingMixIndex);
}

void action_add_mix(lv_event_t *e){
    if(objects.mix_main_panel) lv_obj_add_flag(objects.mix_main_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.mix_addedit_panel) lv_obj_add_flag(objects.mix_addedit_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.all_mixes_panel) lv_obj_remove_flag(objects.all_mixes_panel,LV_OBJ_FLAG_HIDDEN);
}

void action_mix_type_select(lv_event_t *e){
    if(!e) return;

    hideMixOperationPopup();
    uint8_t type=(uint8_t)(uintptr_t)lv_event_get_user_data(e);

    // Aileron UI may be edited independently; mixer-specific calculations
    // are connected in a separate implementation stage.
    if(type>1){
        Serial.printf("Mix type %u: UI ready, logic not connected yet\n",(unsigned)type);
        return;
    }

    int8_t freeMixIndex=findFreeMixIndex();
    if(freeMixIndex<0){
        Serial.println("Free Mix: no free MixLine");
        return;
    }

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)freeMixIndex];
    mix=MixLine{};
    mix.enabled=true;
    snprintf(mix.name,sizeof(mix.name),"%s",type==1?"Ailerons":"Free mix");
    mix.destination=0;
    mix.source=SourceId{};
    mix.operation=MixOperation::ADD;
    mix.condition.type=ConditionType::ALWAYS;
    mix.actionCount=0;
    mix.outputCount=type==1?2:1;
    for(uint8_t slot=0;slot<MAX_MIX_OUTPUTS;slot++){
        mix.mixOutputs[slot].channel=MIX_OUTPUT_NONE;
        mix.mixOutputs[slot].reversed=false;
    }
    if(type==1){
        // Default Aileron output assignment and shared global channel names.
        mix.mixOutputs[0].channel=0;
        mix.mixOutputs[1].channel=4;
        // Names are stored in global channel settings, not in the Aileron widget.
        snprintf(model.outputs[0].name,sizeof(model.outputs[0].name),"Aileron1");
        snprintf(model.outputs[4].name,sizeof(model.outputs[4].name),"Aileron2");
    }

    editingMixIndex=freeMixIndex;
    activeMixEditorKind=type==1?MixEditorKind::AILERON:MixEditorKind::FREE_MIX;
    if(isAileronMixEditor()) mix.flightModeMask|=MIX_AILERON_TYPE_FLAG;
    if(isAileronMixEditor()){
        aileronEditorState=AileronEditorState{};
        for(uint8_t i=0;i<10;i++) aileronEditorState.weightConditions[i].type=ConditionType::ALWAYS;
        for(uint8_t i=0;i<6;i++) aileronEditorState.curveConditions[i].type=ConditionType::ALWAYS;
        saveAileronEditorState();
    }else{
        char key[16];
        aileronPreferencesKey(key,sizeof(key),(uint8_t)freeMixIndex);
        Preferences prefs;
        if(prefs.begin("aileronMix",false)){prefs.remove(key);prefs.end();}
    }
    editingMixActionIndex=-1;
    mixActionEditorIsNew=false;
    selectedMixIndex=freeMixIndex;

    // Mix Editor and Action Editor share mix_editing_diag_panel.
    setMixGraphActionPreview(true);

    if(objects.all_mixes_panel) lv_obj_add_flag(objects.all_mixes_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.mix_main_panel) lv_obj_add_flag(objects.mix_main_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.mix_addedit_panel) lv_obj_remove_flag(objects.mix_addedit_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.free_mixes_edit_panel){
        if(isAileronMixEditor()) lv_obj_add_flag(objects.free_mixes_edit_panel,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(objects.free_mixes_edit_panel,LV_OBJ_FLAG_HIDDEN);
    }
    if(objects.aileron_mixes_edit_panel){
        if(isAileronMixEditor()) lv_obj_remove_flag(objects.aileron_mixes_edit_panel,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(objects.aileron_mixes_edit_panel,LV_OBJ_FLAG_HIDDEN);
    }
    if(isAileronMixEditor()) updateAileronEditorUi();
    if(objects.mix_edit_new_action_panel) lv_obj_add_flag(objects.mix_edit_new_action_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.action_type_selcet_panel) lv_obj_add_flag(objects.action_type_selcet_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.trim_type_select_panel) lv_obj_add_flag(objects.trim_type_select_panel,LV_OBJ_FLAG_HIDDEN);
    hideMixActionChanger();
    setupMixOutputUiCallbacks();
    bindAileronEditorCallbacks();
    updateMixOutputsUi();
    setupMixSourceSelectorCallbacks();
    updateMixSourceEditorUi();

    lv_obj_t *sourceSelectorRoot=getMixSourceSelectorRoot();
    if(sourceSelectorRoot)
        lv_obj_add_flag(sourceSelectorRoot,LV_OBJ_FLAG_HIDDEN);

    clearFreeMixActionList();
    rebuildFreeMixActionList();

    loadMixNameToEditor();
    updateMixActiveConditionEditorUi();
    updateMixSourceEditorUi();
    updateMixOperationEditorUi();
    resetMixEditorEncoderNavigation();
}


static lv_obj_t *getMixWeightRow(uint8_t index);
static lv_obj_t *getMixCurveRow(uint8_t index);
static void hideAllMixActionTypePanels();
static void resetMixActionEditorUi();

static int8_t findFreeMixIndex(){
    ModelConfig &model=modelManager.getActiveModel();

    for(uint8_t i=0;i<MAX_MIX_LINES;i++)
        if(!model.mixes[i].enabled)
            return (int8_t)i;

    return -1;
}

static const char *getMixActionTypeName(MixActionType type){
    switch(type){
        case MixActionType::CURVE:return "Curve";
        case MixActionType::WEIGHT:return "Weight";
        case MixActionType::DIFFERENTIAL:return "Differential";
        case MixActionType::OFFSET:return "Offset";
        case MixActionType::SLOW:return "Slow";
        case MixActionType::TRIM:return "Trim";
        default:return "?";
    }
}

static const char *getMixTrimName(int8_t trimIndex){
    static const char *names[6]={
        "Trim Rudder",
        "Trim Elevator",
        "Trim Throttle",
        "Trim Aileron",
        "T5",
        "T6"
    };

    if(trimIndex<0||trimIndex>=6) return "---";
    return names[trimIndex];
}

static bool mixActionVariantRowVisible(MixActionType type,uint8_t slot){
    if(slot==0) return true;

    lv_obj_t *row=nullptr;

    if(type==MixActionType::WEIGHT)
        row=getMixWeightRow(slot);
    else if(type==MixActionType::CURVE)
        row=getMixCurveRow(slot);

    return row&&!lv_obj_has_flag(row,LV_OBJ_FLAG_HIDDEN);
}

static uint8_t getVisibleMixActionVariantCount(MixActionType type){
    if(type==MixActionType::WEIGHT){
        uint8_t count=1;
        for(uint8_t slot=1;slot<10;slot++)
            if(mixActionVariantRowVisible(type,slot))
                count++;
        return count;
    }

    if(type==MixActionType::CURVE){
        uint8_t count=1;
        for(uint8_t slot=1;slot<6;slot++)
            if(mixActionVariantRowVisible(type,slot))
                count++;
        return count;
    }

    return 0;
}

static void formatMixActionValue(const MixAction &action,const ModelConfig &model,char *buffer,size_t size){
    if(!buffer||size==0) return;
    buffer[0]='\0';

    switch(action.type){
        case MixActionType::WEIGHT:{
            const MixActionVariant *variant=
                action.variantCount>0&&action.variants[0].enabled?&action.variants[0]:nullptr;

            if(!variant){
                snprintf(buffer,size,"---");
                return;
            }

            snprintf(buffer,size,"%d%%",(int)(variant->value/10));
            return;
        }

        case MixActionType::CURVE:{
            const MixActionVariant *variant=
                action.variantCount>0&&action.variants[0].enabled?&action.variants[0]:nullptr;

            if(!variant){
                snprintf(buffer,size,"---");
                return;
            }

            if(variant->curveIndex==OUTPUT_CURVE_EXPO){
                snprintf(buffer,size,"Expo(%d%%)",(int)variant->expo);
                return;
            }

            if(variant->curveIndex>=0&&variant->curveIndex<MAX_MODEL_CURVES){
                const CurveConfig &curve=model.curves[(uint8_t)variant->curveIndex];
                if(curve.enabled){
                    snprintf(buffer,size,"%s",curve.name[0]?curve.name:"Curve");
                    return;
                }
            }

            snprintf(buffer,size,"---");
            return;
        }

        case MixActionType::DIFFERENTIAL:
            snprintf(buffer,size,"%d%%",(int)(action.value1/10));
            return;

        case MixActionType::OFFSET:
            snprintf(buffer,size,"%d%% / %d%%",(int)(action.value1/10),(int)(action.value2/10));
            return;

        case MixActionType::SLOW:
            snprintf(buffer,size,"%.1fs / %.1fs",action.value1/1000.0f,action.value2/1000.0f);
            return;

        case MixActionType::TRIM:
            snprintf(buffer,size,"%s",getMixTrimName(action.trimIndex));
            return;

        default:
            snprintf(buffer,size,"---");
            return;
    }
}

static void clearFreeMixActionList(){
    if(objects.freemix_actions_list)
        lv_obj_clean(objects.freemix_actions_list);
}

static void appendMixActionListLine(char *buffer,size_t size,const char *condition,const char *value){
    if(!buffer||size==0||!condition||!value) return;

    size_t used=strlen(buffer);
    if(used>=size-1) return;

    snprintf(
        buffer+used,
        size-used,
        "%s%s  %s",
        used>0?"\n":"",
        condition,
        value
    );
}

static void formatMixCurveVariantValue(
    const MixActionVariant &variant,
    const ModelConfig &model,
    char *buffer,
    size_t size
){
    if(!buffer||size==0) return;

    if(variant.curveIndex==OUTPUT_CURVE_EXPO){
        snprintf(buffer,size,"Expo(%d%%)",(int)variant.expo);
        return;
    }

    if(variant.curveIndex>=0&&variant.curveIndex<MAX_MODEL_CURVES){
        const CurveConfig &curve=model.curves[(uint8_t)variant.curveIndex];

        if(curve.enabled){
            snprintf(buffer,size,"%s",curve.name[0]?curve.name:"Curve");
            return;
        }
    }

    snprintf(buffer,size,"---");
}

static void formatMixActionListDetails(
    const MixAction &action,
    const ModelConfig &model,
    char *buffer,
    size_t size
){
    if(!buffer||size==0) return;
    buffer[0]='\0';

    if(action.type==MixActionType::WEIGHT||action.type==MixActionType::CURVE){
        uint8_t count=action.variantCount;
        if(count>MAX_MIX_ACTION_VARIANTS) count=MAX_MIX_ACTION_VARIANTS;

        if(count==0){
            snprintf(buffer,size,"---");
            return;
        }

        // One variant is shown compactly, exactly like the ETHOS-style row:
        // Always on | Curve | CurveName
        if(count==1){
            const MixActionVariant &variant=action.variants[0];

            if(action.type==MixActionType::WEIGHT)
                snprintf(buffer,size,"%d%%",(int)(variant.value/10));
            else
                formatMixCurveVariantValue(variant,model,buffer,size);

            return;
        }

        // Conditional variants are listed first.
        // Slot 0 is the mandatory fallback and is displayed last as "Otherwise".
        for(uint8_t i=1;i<count;i++){
            const MixActionVariant &variant=action.variants[i];
            if(!variant.enabled) continue;

            char conditionText[64];
            char valueText[48];

            formatMixConditionName(variant.condition,conditionText,sizeof(conditionText));

            if(action.type==MixActionType::WEIGHT)
                snprintf(valueText,sizeof(valueText),"%d%%",(int)(variant.value/10));
            else
                formatMixCurveVariantValue(variant,model,valueText,sizeof(valueText));

            appendMixActionListLine(buffer,size,conditionText,valueText);
        }

        if(action.variants[0].enabled){
            char valueText[48];

            if(action.type==MixActionType::WEIGHT)
                snprintf(valueText,sizeof(valueText),"%d%%",(int)(action.variants[0].value/10));
            else
                formatMixCurveVariantValue(action.variants[0],model,valueText,sizeof(valueText));

            appendMixActionListLine(buffer,size,"Otherwise",valueText);
        }

        if(buffer[0]=='\0')
            snprintf(buffer,size,"---");

        return;
    }

    formatMixActionValue(action,model,buffer,size);
}

static void exitMixActionMoveMode(){
    mixActionMoveMode=false;
    selectedMixActionIndex=-1;

    if(objects.free_mixes_edit_panel)
        lv_obj_remove_event_cb(
            objects.free_mixes_edit_panel,
            mixActionMoveOutsideClicked
        );

    rebuildFreeMixActionList();
}

static void mixActionMoveOutsideClicked(lv_event_t *e){
    if(!e||!mixActionMoveMode) return;
    if(lv_event_get_code(e)!=LV_EVENT_CLICKED) return;

    exitMixActionMoveMode();
}

static void moveSelectedMixAction(int8_t direction){
    if(!mixActionMoveMode) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
    if(selectedMixActionIndex<0||selectedMixActionIndex>=MAX_MIX_ACTIONS) return;
    if(direction==0) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    uint8_t from=(uint8_t)selectedMixActionIndex;
    if(from>=mix.actionCount) return;

    int16_t to=(int16_t)from+(direction<0?-1:1);
    if(to<0||to>=mix.actionCount) return;

    lv_coord_t scrollY=0;
    if(objects.free_mixes_edit_panel)
        scrollY=lv_obj_get_scroll_y(objects.free_mixes_edit_panel);

    MixAction tmp=mix.actions[from];
    mix.actions[from]=mix.actions[(uint8_t)to];
    mix.actions[(uint8_t)to]=tmp;

    selectedMixActionIndex=(int8_t)to;

    rebuildFreeMixActionList();
    mixer.resetRuntime();

    if(selectedMixIndex>=0&&selectedMixIndex<MAX_MIX_LINES)
        drawMixGraph((uint8_t)selectedMixIndex);

    if(objects.free_mixes_edit_panel){
        lv_obj_update_layout(objects.free_mixes_edit_panel);
        lv_obj_scroll_to_y(
            objects.free_mixes_edit_panel,
            scrollY,
            LV_ANIM_OFF
        );
    }
}

static void mixActionMoveArrowClicked(lv_event_t *e){
    if(!e) return;

    int8_t direction=(int8_t)(intptr_t)lv_event_get_user_data(e);
    moveSelectedMixAction(direction);

    lv_event_stop_bubbling(e);
}

static void mixActionCloneMenuClicked(lv_event_t *e){
    (void)e;

    if(sharedEditDeletePopupTarget!=SharedEditDeletePopupTarget::ACTION) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
    if(selectedMixActionIndex<0||selectedMixActionIndex>=MAX_MIX_ACTIONS) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];
    uint8_t sourceIndex=(uint8_t)selectedMixActionIndex;

    if(sourceIndex>=mix.actionCount) return;
    if(!mix.actions[sourceIndex].enabled) return;
    if(mix.actionCount>=MAX_MIX_ACTIONS) return;

    uint8_t insertIndex=(uint8_t)(sourceIndex+1);

    for(uint8_t i=mix.actionCount;i>insertIndex;i--)
        mix.actions[i]=mix.actions[i-1];

    mix.actions[insertIndex]=mix.actions[sourceIndex];
    mix.actions[insertIndex].enabled=true;
    mix.actionCount++;

    selectedMixActionIndex=(int8_t)insertIndex;

    if(objects.curve_popup)
        lv_obj_add_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN);
    hidePopupDismissOverlay();

    rebuildFreeMixActionList();
    mixer.resetRuntime();

    if(selectedMixIndex>=0&&selectedMixIndex<MAX_MIX_LINES)
        drawMixGraph((uint8_t)selectedMixIndex);
}

static void mixActionAddMenuClicked(lv_event_t *e){
    (void)e;

    if(sharedEditDeletePopupTarget!=SharedEditDeletePopupTarget::ACTION) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    if(mix.actionCount>=MAX_MIX_ACTIONS) return;

    if(objects.curve_popup)
        lv_obj_add_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN);
    hidePopupDismissOverlay();

    if(objects.add_new_action_panel)
        lv_obj_send_event(
            objects.add_new_action_panel,
            LV_EVENT_CLICKED,
            nullptr
        );
}

static void mixActionMoveMenuClicked(lv_event_t *e){
    (void)e;

    if(sharedEditDeletePopupTarget!=SharedEditDeletePopupTarget::ACTION) return;
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
    if(selectedMixActionIndex<0||selectedMixActionIndex>=MAX_MIX_ACTIONS) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    if((uint8_t)selectedMixActionIndex>=mix.actionCount) return;

    mixActionMoveMode=true;

    if(objects.curve_popup)
        lv_obj_add_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN);
    hidePopupDismissOverlay();

    if(objects.free_mixes_edit_panel){
        lv_obj_remove_event_cb(
            objects.free_mixes_edit_panel,
            mixActionMoveOutsideClicked
        );
        lv_obj_add_event_cb(
            objects.free_mixes_edit_panel,
            mixActionMoveOutsideClicked,
            LV_EVENT_CLICKED,
            nullptr
        );
    }

    rebuildFreeMixActionList();
}

static lv_obj_t *createFreeMixActionRow(
    lv_obj_t *parent,
    uint8_t actionIndex,
    bool alternate
){
    if(!parent) return nullptr;

    lv_obj_t *row = lv_obj_create(parent);

    lv_obj_set_pos(row, 0, 0);
    lv_obj_set_size(row, 560, LV_SIZE_CONTENT); // Фиксированный размер спасает от рекурсии LV_SIZE_CONTENT
    
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(row, 0, LV_PART_MAIN);
    
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    
    // Обязательно разрешаем клики и фокус для энкодера
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    // Основной цвет фона
    lv_obj_set_style_bg_opa(row, 255, LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        row,
        lv_color_hex(theme_colors[active_theme_index][alternate ? 11 : 10]),
        LV_PART_MAIN
    );
    
    // КРИТИЧЕСКИ ВАЖНО: Убираем тяжелые пересчеты макета при смене состояния фокуса.
    // Меняем ТОЛЬКО цвет фона через LV_STATE_FOCUSED, без изменения размеров и отступов.
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, ((lv_style_selector_t)LV_PART_MAIN | (lv_style_selector_t)LV_STATE_FOCUSED));
    lv_obj_set_style_bg_color(
        row,
        lv_color_hex(theme_colors[active_theme_index][3]),
        ((lv_style_selector_t)LV_PART_MAIN | (lv_style_selector_t)LV_STATE_FOCUSED)
    );
    
    lv_obj_set_style_text_color(
        row,
        lv_color_hex(theme_colors[active_theme_index][16]),
        LV_PART_MAIN
    );
    lv_obj_set_style_text_color(
        row,
        lv_color_hex(theme_colors[active_theme_index][9]),
        ((lv_style_selector_t)LV_PART_MAIN | (lv_style_selector_t)LV_STATE_FOCUSED)
    );

    lv_obj_set_style_text_font(
        row,
        &lv_font_montserrat_22,
        LV_PART_MAIN
    );
    lv_obj_set_style_text_align(row, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    
    // ВНИМАНИЕ: Если внутри строки нет динамических элементов, меняющих размер,
    // лучше убрать LV_LAYOUT_FLEX или использовать абсолютное позиционирование дочерних элементов,
    // так как Flex-layout при получении фокуса может вызывать тяжелый перерасчет дерева.
    // Если флекс нужен, оставьте, но убедитесь, что у дочерних элементов нет % размеров.
    lv_obj_set_style_layout(row, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(row, LV_FLEX_FLOW_ROW, LV_PART_MAIN);
    
    lv_obj_set_style_pad_left(row, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_right(row, 5, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(row, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    
    lv_obj_set_style_pad_top(row, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(row, 6, LV_PART_MAIN);

    lv_obj_add_event_cb(
        row,
        mixActionRowClicked,
        LV_EVENT_CLICKED,
        (void *)(uintptr_t)actionIndex
    );

    return row;
}

static void rebuildFreeMixActionList(){
    if(!objects.freemix_actions_list) return;

    focusedMixActionIndex=-1;
    clearFreeMixActionList();

    // Keep the list itself exactly as configured in EEZ.
    lv_obj_set_pos(objects.freemix_actions_list,0,0);
    // Set a temporary minimal height; final explicit height is calculated below.
    lv_obj_set_size(objects.freemix_actions_list,560,LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(objects.freemix_actions_list,0,LV_PART_MAIN);
    lv_obj_set_style_pad_top(objects.freemix_actions_list,0,LV_PART_MAIN);
    lv_obj_set_style_pad_right(objects.freemix_actions_list,0,LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(objects.freemix_actions_list,0,LV_PART_MAIN);
    lv_obj_set_style_pad_row(objects.freemix_actions_list,-1,LV_PART_MAIN);
    lv_obj_set_style_bg_opa(objects.freemix_actions_list,0,LV_PART_MAIN);
    lv_obj_set_style_border_width(objects.freemix_actions_list,0,LV_PART_MAIN);
    lv_obj_set_style_radius(objects.freemix_actions_list,0,LV_PART_MAIN);
    lv_obj_remove_flag(objects.freemix_actions_list,LV_OBJ_FLAG_SCROLLABLE);
    // Dynamic Action rows use explicit absolute placement.
    // Do not let child state/style changes trigger a parent flex relayout.
    lv_obj_set_style_layout(objects.freemix_actions_list,LV_LAYOUT_FLEX,LV_PART_MAIN);
    lv_obj_set_style_flex_flow(objects.freemix_actions_list,LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);

    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    ModelConfig &model=modelManager.getActiveModel();
    const MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    uint8_t count=mix.actionCount;
    if(count>MAX_MIX_ACTIONS) count=MAX_MIX_ACTIONS;

    uint8_t visibleRow=0;

    for(uint8_t actionIndex=0;actionIndex<count;actionIndex++){
        const MixAction &action=mix.actions[actionIndex];
        if(!action.enabled) continue;

        lv_obj_t *row=createFreeMixActionRow(
            objects.freemix_actions_list,
            actionIndex,
            (visibleRow&1)!=0
        );
        if(!row) continue;

        visibleRow++;

        char conditionName[64];
        formatMixConditionName(action.condition,conditionName,sizeof(conditionName));

        // Column 1: 131 x 28.
        lv_obj_t *conditionLabel=lv_label_create(row);
        lv_obj_remove_flag(conditionLabel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(conditionLabel,0,0);
        lv_obj_set_size(conditionLabel,131,LV_SIZE_CONTENT);
        lv_label_set_long_mode(conditionLabel,LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(conditionLabel,LV_TEXT_ALIGN_LEFT,LV_PART_MAIN);
        //lv_obj_set_style_align(conditionLabel,LV_ALIGN_LEFT_MID,LV_PART_MAIN);
        setConditionText(conditionLabel,conditionName);

        // Column 2: 107 x 28.
        lv_obj_t *typeLabel=lv_label_create(row);
        lv_obj_remove_flag(typeLabel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(typeLabel,0,0);
        lv_obj_set_size(typeLabel,107,LV_SIZE_CONTENT);
        lv_label_set_long_mode(typeLabel,LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(typeLabel,LV_TEXT_ALIGN_LEFT,LV_PART_MAIN);
        //lv_obj_set_style_align(typeLabel,LV_ALIGN_CENTER,LV_PART_MAIN);
        lv_label_set_text(typeLabel,getMixActionTypeName(action.type));

        // Column 3. Use explicit geometry instead of nested LV_SIZE_CONTENT.
        // This avoids an LVGL flex/content layout loop when the Actions list is
        // present and another editor row (Operation, Action, etc.) is clicked.
        char details[512];
        formatMixActionListDetails(action,model,details,sizeof(details));

        uint8_t lineCount=1;
        for(const char *p=details;*p;p++)
            if(*p=='\n'&&lineCount<MAX_MIX_ACTION_VARIANTS)
                lineCount++;

        int32_t rowHeight=40;
        lv_obj_set_pos(row,0,0);
        lv_obj_set_size(row,560,LV_SIZE_CONTENT);

        bool isMoveRow=
            mixActionMoveMode&&selectedMixActionIndex==(int8_t)actionIndex;

        lv_obj_t *valueLabel=lv_label_create(row);
        lv_obj_remove_flag(valueLabel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(valueLabel,0,0);
        lv_obj_set_size(valueLabel,isMoveRow?190:280,LV_SIZE_CONTENT);
        lv_label_set_long_mode(valueLabel,LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(valueLabel,LV_TEXT_ALIGN_LEFT,LV_PART_MAIN);
        lv_obj_set_style_align(valueLabel,LV_ALIGN_RIGHT_MID,LV_PART_MAIN);
        setConditionText(valueLabel,details);

        if(isMoveRow){
            lv_obj_t *upButton=lv_button_create(row);
            lv_obj_set_size(upButton,38,32);
            lv_obj_remove_flag(upButton,LV_OBJ_FLAG_EVENT_BUBBLE);
            lv_obj_add_event_cb(
                upButton,
                mixActionMoveArrowClicked,
                LV_EVENT_CLICKED,
                (void *)(intptr_t)-1
            );

            lv_obj_t *upLabel=lv_label_create(upButton);
            lv_label_set_text(upLabel,LV_SYMBOL_UP);
            lv_obj_center(upLabel);

            lv_obj_t *downButton=lv_button_create(row);
            lv_obj_set_size(downButton,38,32);
            lv_obj_remove_flag(downButton,LV_OBJ_FLAG_EVENT_BUBBLE);
            lv_obj_add_event_cb(
                downButton,
                mixActionMoveArrowClicked,
                LV_EVENT_CLICKED,
                (void *)(intptr_t)1
            );

            lv_obj_t *downLabel=lv_label_create(downButton);
            lv_label_set_text(downLabel,LV_SYMBOL_DOWN);
            lv_obj_center(downLabel);

            if(actionIndex==0)
                lv_obj_add_state(upButton,LV_STATE_DISABLED);

            if(actionIndex+1>=count)
                lv_obj_add_state(downButton,LV_STATE_DISABLED);
        }
    }
}

static void setMixActionTypeUi(MixActionType type){
    activeMixActionType=type;

    hideMixActionChanger();

    if(objects.ch_edit_curve_list)
        lv_obj_add_flag(objects.ch_edit_curve_list,LV_OBJ_FLAG_HIDDEN);

    hideAllMixActionTypePanels();

    switch(type){
        case MixActionType::CURVE:
            if(objects.mix_action_type_lbl) lv_label_set_text(objects.mix_action_type_lbl,"Curve");
            if(objects.mix_action_curve_list_panel) lv_obj_remove_flag(objects.mix_action_curve_list_panel,LV_OBJ_FLAG_HIDDEN);
            break;

        case MixActionType::WEIGHT:
            if(objects.mix_action_type_lbl) lv_label_set_text(objects.mix_action_type_lbl,"Weight");
            if(objects.mix_action_weight_list_panel) lv_obj_remove_flag(objects.mix_action_weight_list_panel,LV_OBJ_FLAG_HIDDEN);
            break;

        case MixActionType::DIFFERENTIAL:
            if(objects.mix_action_type_lbl) lv_label_set_text(objects.mix_action_type_lbl,"Differential");
            if(objects.mix_action_differential_list_panel) lv_obj_remove_flag(objects.mix_action_differential_list_panel,LV_OBJ_FLAG_HIDDEN);
            break;

        case MixActionType::OFFSET:
            if(objects.mix_action_type_lbl) lv_label_set_text(objects.mix_action_type_lbl,"Offset");
            if(objects.mix_action_offset_list_panel) lv_obj_remove_flag(objects.mix_action_offset_list_panel,LV_OBJ_FLAG_HIDDEN);
            break;

        case MixActionType::SLOW:
            if(objects.mix_action_type_lbl) lv_label_set_text(objects.mix_action_type_lbl,"Slow");
            if(objects.mix_action_slow_list_panel) lv_obj_remove_flag(objects.mix_action_slow_list_panel,LV_OBJ_FLAG_HIDDEN);
            break;

        case MixActionType::TRIM:
            if(objects.mix_action_type_lbl) lv_label_set_text(objects.mix_action_type_lbl,"Trim");
            if(objects.mix_action_trim_list_panel) lv_obj_remove_flag(objects.mix_action_trim_list_panel,LV_OBJ_FLAG_HIDDEN);
            break;
    }
}

static void loadMixActionEditor(uint8_t mixIndex,uint8_t actionIndex){
    if(mixIndex>=MAX_MIX_LINES) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[mixIndex];

    if(actionIndex>=mix.actionCount||actionIndex>=MAX_MIX_ACTIONS) return;

    MixAction &action=mix.actions[actionIndex];
    if(!action.enabled) return;

    resetMixActionEditorUi();
    mixActionEditorCondition=action.condition;
    if(!mixActionEditorCondition.isValid()&&
       mixActionEditorCondition.type!=ConditionType::NEVER&&
       mixActionEditorCondition.type!=ConditionType::NONE&&
       mixActionEditorCondition.type!=ConditionType::ALWAYS)
        mixActionEditorCondition.type=ConditionType::ALWAYS;

    editingMixIndex=(int8_t)mixIndex;
    editingMixActionIndex=(int8_t)actionIndex;
    mixActionEditorIsNew=false;
    selectedMixIndex=(int8_t)mixIndex;

    setMixGraphActionPreview(true);
    setMixActionTypeUi(action.type);
    updateMixActionConditionEditorUi();

    if(action.type==MixActionType::WEIGHT){
        uint8_t count=action.variantCount;
        if(count<1) count=1;
        if(count>10) count=10;

        for(uint8_t slot=0;slot<count;slot++){
            if(slot>0){
                lv_obj_t *row=getMixWeightRow(slot);
                if(row) lv_obj_remove_flag(row,LV_OBJ_FLAG_HIDDEN);
            }

            if(slot<action.variantCount&&action.variants[slot].enabled){
                mixActionWeightValues[slot]=(int16_t)constrain((int32_t)action.variants[slot].value/10L,-500L,500L);
                if(slot>0){
                    mixActionVariantConditions[slot]=action.variants[slot].condition;
                    if(!mixActionVariantConditions[slot].isValid()&&
                       mixActionVariantConditions[slot].type!=ConditionType::NEVER&&
                       mixActionVariantConditions[slot].type!=ConditionType::NONE&&
                       mixActionVariantConditions[slot].type!=ConditionType::ALWAYS)
                        mixActionVariantConditions[slot].type=ConditionType::ALWAYS;
                    updateMixActionVariantConditionEditorUi(MixActionType::WEIGHT,slot);
                }
            }
        }

        if(objects.action_add_new_weight){
            if(count>=10) lv_obj_add_state(objects.action_add_new_weight,LV_STATE_DISABLED);
            else lv_obj_remove_state(objects.action_add_new_weight,LV_STATE_DISABLED);
        }

        for(uint8_t slot=0;slot<10;slot++)
            updateMixActionValueLabel(slot);
    }else if(action.type==MixActionType::CURVE){
        uint8_t count=action.variantCount;
        if(count<1) count=1;
        if(count>6) count=6;

        for(uint8_t slot=0;slot<count;slot++){
            if(slot>0){
                lv_obj_t *row=getMixCurveRow(slot);
                if(row) lv_obj_remove_flag(row,LV_OBJ_FLAG_HIDDEN);
            }

            if(slot<action.variantCount&&action.variants[slot].enabled){
                mixActionCurveSelection[slot]=action.variants[slot].curveIndex;
                mixActionCurveExpoValues[slot]=action.variants[slot].expo;
                if(slot>0){
                    mixActionVariantConditions[slot]=action.variants[slot].condition;
                    if(!mixActionVariantConditions[slot].isValid()&&
                       mixActionVariantConditions[slot].type!=ConditionType::NEVER&&
                       mixActionVariantConditions[slot].type!=ConditionType::NONE&&
                       mixActionVariantConditions[slot].type!=ConditionType::ALWAYS)
                        mixActionVariantConditions[slot].type=ConditionType::ALWAYS;
                    updateMixActionVariantConditionEditorUi(MixActionType::CURVE,slot);
                }
            }

            updateMixCurveSelectionUi(slot);
            updateMixActionValueLabel((uint8_t)(10+slot));
        }

        if(objects.action_add_new_curve){
            if(count>=6) lv_obj_add_state(objects.action_add_new_curve,LV_STATE_DISABLED);
            else lv_obj_remove_state(objects.action_add_new_curve,LV_STATE_DISABLED);
        }
    }else if(action.type==MixActionType::DIFFERENTIAL){
        mixActionDifferentialValue=(int16_t)constrain((int32_t)action.value1/10L,-100L,100L);
        updateMixActionValueLabel(20);
    }else if(action.type==MixActionType::OFFSET){
        mixActionOffsetActiveValue=(int16_t)constrain((int32_t)action.value1/10L,-200L,200L);
        mixActionOffsetInactiveValue=(int16_t)constrain((int32_t)action.value2/10L,-200L,200L);
        updateMixActionValueLabel(30);
        updateMixActionValueLabel(31);
    }else if(action.type==MixActionType::SLOW){
        mixActionSlowUpValue=(int16_t)constrain((int32_t)action.value1/100L,0L,250L);
        mixActionSlowDownValue=(int16_t)constrain((int32_t)action.value2/100L,0L,250L);
        updateMixActionValueLabel(40);
        updateMixActionValueLabel(41);
    }else if(action.type==MixActionType::TRIM){
        activeMixTrimIndex=action.trimIndex;
        if(activeMixTrimIndex<0||activeMixTrimIndex>=6) activeMixTrimIndex=0;
        if(objects.action_trim_trim)
            lv_label_set_text(objects.action_trim_trim,getMixTrimName(activeMixTrimIndex));
    }

    if(objects.free_mixes_edit_panel)
        lv_obj_add_flag(objects.free_mixes_edit_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.mix_edit_new_action_panel)
        lv_obj_remove_flag(objects.mix_edit_new_action_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.mix_edit_new_action_panel)
        lv_obj_scroll_to_y(objects.mix_edit_new_action_panel,0,LV_ANIM_OFF);
}

static bool buildMixActionFromEditor(MixAction &action){
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return false;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    MixAction oldAction{};
    if(!mixActionEditorIsNew&&editingMixActionIndex>=0&&editingMixActionIndex<mix.actionCount)
        oldAction=mix.actions[(uint8_t)editingMixActionIndex];

    action=MixAction{};
    action.enabled=true;
    action.type=activeMixActionType;
    action.condition=mixActionEditorCondition;

    switch(activeMixActionType){
        case MixActionType::WEIGHT:{
            uint8_t visibleCount=getVisibleMixActionVariantCount(activeMixActionType);
            action.variantCount=visibleCount;

            uint8_t dstSlot=0;
            for(uint8_t uiSlot=0;uiSlot<10&&dstSlot<visibleCount;uiSlot++){
                if(!mixActionVariantRowVisible(activeMixActionType,uiSlot)) continue;

                MixActionVariant &variant=action.variants[dstSlot++];
                variant.enabled=true;

                if(uiSlot==0)
                    variant.condition=ConditionConfig{};
                else
                    variant.condition=mixActionVariantConditions[uiSlot];

                variant.value=(int16_t)constrain(
                    (int32_t)mixActionWeightValues[uiSlot]*10L,
                    -5000L,
                    5000L
                );
            }
            break;
        }

        case MixActionType::CURVE:{
            uint8_t visibleCount=getVisibleMixActionVariantCount(activeMixActionType);
            action.variantCount=visibleCount;

            uint8_t dstSlot=0;
            for(uint8_t uiSlot=0;uiSlot<6&&dstSlot<visibleCount;uiSlot++){
                if(!mixActionVariantRowVisible(activeMixActionType,uiSlot)) continue;

                MixActionVariant &variant=action.variants[dstSlot++];
                variant.enabled=true;

                if(uiSlot==0)
                    variant.condition=ConditionConfig{};
                else
                    variant.condition=mixActionVariantConditions[uiSlot];

                variant.curveIndex=mixActionCurveSelection[uiSlot];
                variant.expo=(int8_t)constrain(
                    (int32_t)mixActionCurveExpoValues[uiSlot],
                    -100L,
                    100L
                );
            }
            break;
        }

        case MixActionType::DIFFERENTIAL:
            action.value1=(int16_t)constrain(
                (int32_t)mixActionDifferentialValue*10L,
                -1000L,
                1000L
            );
            break;

        case MixActionType::OFFSET:
            action.value1=(int16_t)constrain(
                (int32_t)mixActionOffsetActiveValue*10L,
                -2000L,
                2000L
            );
            action.value2=(int16_t)constrain(
                (int32_t)mixActionOffsetInactiveValue*10L,
                -2000L,
                2000L
            );
            break;

        case MixActionType::SLOW:
            action.value1=(int16_t)constrain(
                (int32_t)mixActionSlowUpValue*100L,
                0L,
                25000L
            );
            action.value2=(int16_t)constrain(
                (int32_t)mixActionSlowDownValue*100L,
                0L,
                25000L
            );
            break;

        case MixActionType::TRIM:
            action.trimIndex=(int8_t)constrain(
                (int32_t)activeMixTrimIndex,
                0L,
                5L
            );
            break;
    }

    if(action.condition.value2==ACTION_DIRECTION_MARKER&&
       (action.condition.type==ConditionType::SOURCE_POSITIVE||
        action.condition.type==ConditionType::SOURCE_NEGATIVE))
        action.condition.sourceA=getMixBaseSource(mix);

    uint8_t variantCount=action.variantCount;
    if(variantCount>MAX_MIX_ACTION_VARIANTS) variantCount=MAX_MIX_ACTION_VARIANTS;

    for(uint8_t i=1;i<variantCount;i++){
        MixActionVariant &variant=action.variants[i];
        if(!variant.enabled) continue;

        if(variant.condition.value2==ACTION_DIRECTION_MARKER&&
           (variant.condition.type==ConditionType::SOURCE_POSITIVE||
            variant.condition.type==ConditionType::SOURCE_NEGATIVE))
            variant.condition.sourceA=getMixBaseSource(mix);
    }

    return true;
}

static bool saveMixActionEditor(){
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return false;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    uint8_t actionIndex=0;

    if(mixActionEditorIsNew){
        if(mix.actionCount>=MAX_MIX_ACTIONS) return false;
        actionIndex=mix.actionCount;
    }else{
        if(editingMixActionIndex<0||editingMixActionIndex>=mix.actionCount) return false;
        actionIndex=(uint8_t)editingMixActionIndex;
    }

    MixAction action{};
    if(!buildMixActionFromEditor(action)) return false;



    mix.actions[actionIndex]=action;

    if(mixActionEditorIsNew){
        mix.actionCount++;
        editingMixActionIndex=(int8_t)actionIndex;
        mixActionEditorIsNew=false;
    }

    // Action Editor works on the active model in RAM.
    // Do NOT write the whole ModelConfig to SD here: v8 is large and a synchronous
    // SD save blocks LVGL for several seconds. The mix is persisted when leaving
    // the whole Free Mix editor.
    mixer.resetRuntime();

    return true;
}

static void openMixActionEditorAsync(void *param){
    (void)param;
    mixActionOpenPending=false;

    if(pendingMixActionMixIndex>=MAX_MIX_LINES) return;

    ModelConfig &model=modelManager.getActiveModel();
    const MixLine &mix=model.mixes[pendingMixActionMixIndex];

    if(pendingMixActionIndex>=mix.actionCount||
       pendingMixActionIndex>=MAX_MIX_ACTIONS)
        return;

    editingMixIndex=(int8_t)pendingMixActionMixIndex;
    loadMixActionEditor(pendingMixActionMixIndex,pendingMixActionIndex);
}

static void mixActionRowClicked(lv_event_t *e){
    if(!e) return;

    if(mixActionMoveMode){
        exitMixActionMoveMode();
        return;
    }
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    uint8_t actionIndex=(uint8_t)(uintptr_t)lv_event_get_user_data(e);

    ModelConfig &model=modelManager.getActiveModel();
    const MixLine &mix=model.mixes[(uint8_t)editingMixIndex];

    if(actionIndex>=mix.actionCount||actionIndex>=MAX_MIX_ACTIONS) return;
    if(!mix.actions[actionIndex].enabled) return;

    // ETHOS-like two-step touch behavior:
    // first tap only selects/focuses the Action row; tapping the same
    // already-selected row again opens its popup menu.
    if(focusedMixActionIndex!=(int8_t)actionIndex){
        focusedMixActionIndex=(int8_t)actionIndex;

        MixEditorEncoderItem items[MAX_MIX_EDITOR_ENCODER_ITEMS];
        uint8_t count=buildMixEditorEncoderItems(items,MAX_MIX_EDITOR_ENCODER_ITEMS);
        for(uint8_t i=0;i<count;i++){
            if(items[i].type!=MixEditorEncoderItemType::ACTION) continue;
            if(items[i].data!=actionIndex) continue;
            mixEditorEncoderIndex=(int16_t)i;
            break;
        }

        applyMixEditorEncoderFocus();
        return;
    }

    prepareSharedEditDeletePopupForAction(actionIndex);
}

static lv_obj_t *getMixWeightRow(uint8_t index){
    switch(index){
        case 1:return objects.weight_weight1_panel;
        case 2:return objects.weight_weight2_panel;
        case 3:return objects.weight_weight3_panel;
        case 4:return objects.weight_weight4_panel;
        case 5:return objects.weight_weight5_panel;
        case 6:return objects.weight_weight6_panel;
        case 7:return objects.weight_weight7_panel;
        case 8:return objects.weight_weight8_panel;
        case 9:return objects.weight_weight9_panel;
        default:return nullptr;
    }
}

static lv_obj_t *getMixCurveRow(uint8_t index){
    switch(index){
        case 1:return objects.curve_weight1_panel;
        case 2:return objects.curve_weight2_panel;
        case 3:return objects.curve_weight3_panel;
        case 4:return objects.curve_weight4_panel;
        case 5:return objects.curve_weight5_panel;
        default:return nullptr;
    }
}

static lv_obj_t *getMixActionVariantConditionPanel(MixActionType type,uint8_t slot){
    if(isAileronMixEditor()){
        if(type==MixActionType::WEIGHT){
            lv_obj_t *panels[]={nullptr,objects.aileron_mix_weight_ac_panel1,objects.aileron_mix_weight_ac_panel2,objects.aileron_mix_weight_ac_panel3,objects.aileron_mix_weight_ac_panel4,objects.aileron_mix_weight_ac_panel5,objects.aileron_mix_weight_ac_panel6,objects.aileron_mix_weight_ac_panel7,objects.aileron_mix_weight_ac_panel8,objects.aileron_mix_weight_ac_panel9};
            return slot<10?panels[slot]:nullptr;
        }
        if(type==MixActionType::CURVE){
            lv_obj_t *panels[]={nullptr,objects.aileron_mix_curve_ac_panel1,objects.aileron_mix_curve_ac_panel2,objects.aileron_mix_curve_ac_panel3,objects.aileron_mix_curve_ac_panel4,objects.aileron_mix_curve_ac_panel5};
            return slot<6?panels[slot]:nullptr;
        }
        return nullptr;
    }
    if(slot==0) return nullptr;

    lv_obj_t *row=nullptr;
    if(type==MixActionType::WEIGHT&&slot<10)
        row=getMixWeightRow(slot);
    else if(type==MixActionType::CURVE&&slot<6)
        row=getMixCurveRow(slot);

    if(!row||lv_obj_get_child_count(row)<2) return nullptr;
    return lv_obj_get_child(row,1);
}

static bool getMixActionVariantConditionTarget(lv_obj_t *target,MixActionType &type,uint8_t &slot){
    if(!target) return false;

    for(uint8_t i=1;i<10;i++){
        if(target==getMixActionVariantConditionPanel(MixActionType::WEIGHT,i)){
            type=MixActionType::WEIGHT;
            slot=i;
            return true;
        }
    }

    for(uint8_t i=1;i<6;i++){
        if(target==getMixActionVariantConditionPanel(MixActionType::CURVE,i)){
            type=MixActionType::CURVE;
            slot=i;
            return true;
        }
    }

    return false;
}

static void hideAllMixActionTypePanels(){
    if(objects.mix_action_weight_list_panel) lv_obj_add_flag(objects.mix_action_weight_list_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.mix_action_curve_list_panel) lv_obj_add_flag(objects.mix_action_curve_list_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.mix_action_differential_list_panel) lv_obj_add_flag(objects.mix_action_differential_list_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.mix_action_offset_list_panel) lv_obj_add_flag(objects.mix_action_offset_list_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.mix_action_slow_list_panel) lv_obj_add_flag(objects.mix_action_slow_list_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.mix_action_trim_list_panel) lv_obj_add_flag(objects.mix_action_trim_list_panel,LV_OBJ_FLAG_HIDDEN);
}

static void resetMixActionEditorUi(){
    activeMixActionType=MixActionType::WEIGHT;
    activeMixTrimIndex=0;

    if(objects.action_type_selcet_panel) lv_obj_add_flag(objects.action_type_selcet_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.trim_type_select_panel) lv_obj_add_flag(objects.trim_type_select_panel,LV_OBJ_FLAG_HIDDEN);

    resetMixActionValues();

    for(uint8_t i=1;i<=9;i++){
        lv_obj_t *row=getMixWeightRow(i);
        if(row) lv_obj_add_flag(row,LV_OBJ_FLAG_HIDDEN);
    }

    for(uint8_t i=1;i<=5;i++){
        lv_obj_t *row=getMixCurveRow(i);
        if(row) lv_obj_add_flag(row,LV_OBJ_FLAG_HIDDEN);
    }

    if(objects.action_add_new_weight) lv_obj_remove_state(objects.action_add_new_weight,LV_STATE_DISABLED);
    if(objects.action_add_new_curve) lv_obj_remove_state(objects.action_add_new_curve,LV_STATE_DISABLED);

    if(objects.action_trim_trim) lv_label_set_text(objects.action_trim_trim,"Trim Rudder");

    hideAllMixActionTypePanels();
    if(objects.mix_action_weight_list_panel) lv_obj_remove_flag(objects.mix_action_weight_list_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.mix_action_type_lbl) lv_label_set_text(objects.mix_action_type_lbl,"Weight");
}

void action_mix_add_new_action(lv_event_t *e){
    if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;

    ModelConfig &model=modelManager.getActiveModel();
    MixLine &mix=model.mixes[(uint8_t)editingMixIndex];
    if(mix.actionCount>=MAX_MIX_ACTIONS) return;

    resetMixActionEditorUi();
    mixActionEditorCondition=ConditionConfig{};
    mixActionEditorCondition.type=ConditionType::ALWAYS;
    updateMixActionConditionEditorUi();

    editingMixActionIndex=-1;
    mixActionEditorIsNew=true;
    selectedMixIndex=editingMixIndex;

    setMixGraphActionPreview(true);

    if(objects.free_mixes_edit_panel) lv_obj_add_flag(objects.free_mixes_edit_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.mix_edit_new_action_panel) lv_obj_remove_flag(objects.mix_edit_new_action_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.mix_edit_new_action_panel) lv_obj_scroll_to_y(objects.mix_edit_new_action_panel,0,LV_ANIM_OFF);
}

void action_mix_action_type_panel_show(lv_event_t *e){
    if(objects.action_type_selcet_panel)
        lv_obj_remove_flag(objects.action_type_selcet_panel,LV_OBJ_FLAG_HIDDEN);
}

void action_mix_action_type_selected(lv_event_t *e){
    if(!e) return;

    uint8_t type=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(type>5) return;

    setMixActionTypeUi((MixActionType)type);

    if(objects.action_type_selcet_panel)
        lv_obj_add_flag(objects.action_type_selcet_panel,LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *getAileronWeightRow(uint8_t index){
    lv_obj_t *rows[]={nullptr,objects.aileron_mix_weight_panel1,objects.aileron_mix_weight_panel2,objects.aileron_mix_weight_panel3,objects.aileron_mix_weight_panel4,objects.aileron_mix_weight_panel5,objects.aileron_mix_weight_panel6,objects.aileron_mix_weight_panel7,objects.aileron_mix_weight_panel8,objects.aileron_mix_weight_panel9};
    return index<10?rows[index]:nullptr;
}
static lv_obj_t *getAileronCurveRow(uint8_t index){
    lv_obj_t *rows[]={nullptr,objects.aileron_mix_curve_weight1_panel,objects.aileron_mix_curve_weight2_panel,objects.aileron_mix_curve_weight3_panel,objects.aileron_mix_curve_weight4_panel,objects.aileron_mix_curve_weight5_panel};
    return index>=1&&index<=5?rows[index]:nullptr;
}

static void updateAileronEditorUi(){
    if(!objects.aileron_mixes_edit_panel) return;
    for(uint8_t i=0;i<10;i++) updateMixActionValueLabel(i);
    for(uint8_t i=10;i<16;i++) updateMixActionValueLabel(i);
    updateMixActionValueLabel(20);
    for(uint8_t i=0;i<6;i++) updateMixCurveSelectionUi(i);
    for(uint8_t i=1;i<10;i++){
        lv_obj_t *row=getAileronWeightRow(i);
        if(row){
            if(aileronEditorState.weightEnabled&(1u<<i)) lv_obj_remove_flag(row,LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(row,LV_OBJ_FLAG_HIDDEN);
        }
        updateMixActionVariantConditionEditorUi(MixActionType::WEIGHT,i);
    }
    for(uint8_t i=1;i<6;i++){
        lv_obj_t *row=getAileronCurveRow(i);
        if(row){
            if(aileronEditorState.curveEnabled&(1u<<i)) lv_obj_remove_flag(row,LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(row,LV_OBJ_FLAG_HIDDEN);
        }
        updateMixActionVariantConditionEditorUi(MixActionType::CURVE,i);
    }
    if(objects.aileron_mix_add_new_weight_panel){
        if((aileronEditorState.weightEnabled&0x3FEu)==0x3FEu) lv_obj_add_state(objects.aileron_mix_add_new_weight_panel,LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.aileron_mix_add_new_weight_panel,LV_STATE_DISABLED);
    }
    if(objects.aileron_mix_action_add_new_curve){
        if((aileronEditorState.curveEnabled&0x3Eu)==0x3Eu) lv_obj_add_state(objects.aileron_mix_action_add_new_curve,LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.aileron_mix_action_add_new_curve,LV_STATE_DISABLED);
    }
    if(objects.aileron_mix_trim_switch){
        if(aileronEditorState.trim) lv_obj_add_state(objects.aileron_mix_trim_switch,LV_STATE_CHECKED);
        else lv_obj_remove_state(objects.aileron_mix_trim_switch,LV_STATE_CHECKED);
    }
}

static void aileronTrimChanged(lv_event_t *e){
    if(!e||!isAileronMixEditor()||aileronEditorLoading) return;
    aileronEditorState.trim=lv_obj_has_state(objects.aileron_mix_trim_switch,LV_STATE_CHECKED);
    saveAileronEditorState();
}

static void bindAileronEditorCallbacks(){
    if(aileronCallbacksBound||!objects.aileron_mixes_edit_panel) return;
    aileronCallbacksBound=true;
    // Trim is bound by the EEZ callback action_mixes_trim_on_off.
    bindChangerDismissRecursive(objects.aileron_mixes_edit_panel,objects.mix_changer_panel,dismissMixChangerOnPress);
}

void action_mix_add_new_weight(lv_event_t *e){
    if(isAileronMixEditor()){
        for(uint8_t i=1;i<=9;i++){
            if(aileronEditorState.weightEnabled&(1u<<i)) continue;
            aileronEditorState.weightEnabled|=(uint16_t)(1u<<i);
            aileronEditorState.weights[i]=100;
            aileronEditorState.weightConditions[i]=ConditionConfig{};
            aileronEditorState.weightConditions[i].type=ConditionType::ALWAYS;
            updateAileronEditorUi();
            saveAileronEditorState();
            return;
        }
        return;
    }
    for(uint8_t i=1;i<=9;i++){
        lv_obj_t *row=getMixWeightRow(i);
        if(row&&lv_obj_has_flag(row,LV_OBJ_FLAG_HIDDEN)){
            lv_obj_remove_flag(row,LV_OBJ_FLAG_HIDDEN);
            mixActionVariantConditions[i]=ConditionConfig{};
            mixActionVariantConditions[i].type=ConditionType::ALWAYS;
            updateMixActionVariantConditionEditorUi(MixActionType::WEIGHT,i);

            bool haveFree=false;
            for(uint8_t j=1;j<=9;j++){
                lv_obj_t *testRow=getMixWeightRow(j);
                if(testRow&&lv_obj_has_flag(testRow,LV_OBJ_FLAG_HIDDEN)){
                    haveFree=true;
                    break;
                }
            }

            if(!haveFree&&objects.action_add_new_weight)
                lv_obj_add_state(objects.action_add_new_weight,LV_STATE_DISABLED);

            return;
        }
    }
}

void action_mix_del_weight(lv_event_t *e){
    if(!e) return;
    if(isAileronMixEditor()){
        uint8_t slot=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
        if(slot<1||slot>9) return;
        aileronEditorState.weightEnabled&=(uint16_t)~(1u<<slot);
        if(activeMixActionValueId==slot) hideMixActionChanger();
        updateAileronEditorUi();
        saveAileronEditorState();
        return;
    }

    uint8_t index=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    lv_obj_t *row=getMixWeightRow(index);
    if(!row) return;

    lv_obj_add_flag(row,LV_OBJ_FLAG_HIDDEN);
    if(index<MAX_MIX_ACTION_VARIANTS){
        mixActionVariantConditions[index]=ConditionConfig{};
        mixActionVariantConditions[index].type=ConditionType::ALWAYS;
    }

    if(activeMixActionValueId==index) hideMixActionChanger();

    if(objects.action_add_new_weight)
        lv_obj_remove_state(objects.action_add_new_weight,LV_STATE_DISABLED);
}

void action_mix_add_new_curve(lv_event_t *e){
    if(isAileronMixEditor()){
        for(uint8_t i=1;i<=5;i++){
            if(aileronEditorState.curveEnabled&(1u<<i)) continue;
            aileronEditorState.curveEnabled|=(uint8_t)(1u<<i);
            aileronEditorState.curveSelection[i]=OUTPUT_CURVE_NONE;
            aileronEditorState.curveConditions[i]=ConditionConfig{};
            aileronEditorState.curveConditions[i].type=ConditionType::ALWAYS;
            updateAileronEditorUi();
            saveAileronEditorState();
            return;
        }
        return;
    }
    for(uint8_t i=1;i<=5;i++){
        lv_obj_t *row=getMixCurveRow(i);
        if(row&&lv_obj_has_flag(row,LV_OBJ_FLAG_HIDDEN)){
            lv_obj_remove_flag(row,LV_OBJ_FLAG_HIDDEN);
            mixActionVariantConditions[i]=ConditionConfig{};
            mixActionVariantConditions[i].type=ConditionType::ALWAYS;
            updateMixActionVariantConditionEditorUi(MixActionType::CURVE,i);

            bool haveFree=false;
            for(uint8_t j=1;j<=5;j++){
                lv_obj_t *testRow=getMixCurveRow(j);
                if(testRow&&lv_obj_has_flag(testRow,LV_OBJ_FLAG_HIDDEN)){
                    haveFree=true;
                    break;
                }
            }

            if(!haveFree&&objects.action_add_new_curve)
                lv_obj_add_state(objects.action_add_new_curve,LV_STATE_DISABLED);

            return;
        }
    }
}

void action_mix_del_curve(lv_event_t *e){
    if(!e) return;
    if(isAileronMixEditor()){
        uint8_t slot=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
        if(slot<1||slot>5) return;
        aileronEditorState.curveEnabled&=(uint8_t)~(1u<<slot);
        if(activeMixActionValueId==(uint8_t)(10+slot)) hideMixActionChanger();
        updateAileronEditorUi();
        saveAileronEditorState();
        return;
    }

    uint8_t index=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    lv_obj_t *row=getMixCurveRow(index);
    if(!row) return;

    lv_obj_add_flag(row,LV_OBJ_FLAG_HIDDEN);
    if(index<MAX_MIX_ACTION_VARIANTS){
        mixActionVariantConditions[index]=ConditionConfig{};
        mixActionVariantConditions[index].type=ConditionType::ALWAYS;
    }

    if(activeMixActionValueId==(uint8_t)(10+index)) hideMixActionChanger();

    if(objects.action_add_new_curve)
        lv_obj_remove_state(objects.action_add_new_curve,LV_STATE_DISABLED);
}

enum class TrimSelectContext:uint8_t{
    MIX_ACTION=0,
    VAR_REPURPOSE
};
static TrimSelectContext trimSelectContext=TrimSelectContext::MIX_ACTION;
static int8_t editingVarActionIndex=-1;
static VarConfig *getEditingVar();
static void updateVarActionsUi();

void action_mix_trim_type_panel_show(lv_event_t *e){
    trimSelectContext=TrimSelectContext::MIX_ACTION;
    if(objects.trim_type_select_panel)
        lv_obj_remove_flag(objects.trim_type_select_panel,LV_OBJ_FLAG_HIDDEN);
}

void action_trim_type_selected(lv_event_t *e){
    if(!e) return;

    static const char *trimNames[6]={
        "Trim Rudder",
        "Trim Elevator",
        "Trim Throttle",
        "Trim Aileron",
        "T5",
        "T6"
    };

    uint8_t index=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(index>=6) return;

    if(trimSelectContext==TrimSelectContext::VAR_REPURPOSE){
        VarConfig *var=getEditingVar();
        if(var&&editingVarActionIndex>=0&&editingVarActionIndex<var->actionCount){
            VarActionConfig &action=var->actions[(uint8_t)editingVarActionIndex];
            action.repurposeTrimIndex=(int8_t)index;
            updateVarActionsUi();
            modelManager.requestSaveActiveModel();
        }
        if(objects.vars_trim_type_select_panel) lv_obj_add_flag(objects.vars_trim_type_select_panel,LV_OBJ_FLAG_HIDDEN);
        hidePopupDismissOverlay();
        editingVarActionIndex=-1;
        trimSelectContext=TrimSelectContext::MIX_ACTION;
        return;
    }

    activeMixTrimIndex=(int8_t)index;
    if(objects.action_trim_trim) lv_label_set_text(objects.action_trim_trim,trimNames[index]);
    if(objects.trim_type_select_panel) lv_obj_add_flag(objects.trim_type_select_panel,LV_OBJ_FLAG_HIDDEN);
}

void action_open_scr_model_select(lv_event_t *e){
    openScreen(SCREEN_ID_SCR_MODEL_SELECT);
    refreshModelList();
}
void action_ch_curve_choice_panel_open(lv_event_t *e){
    if(!e) return;

    lv_obj_t *target=(lv_obj_t *)lv_event_get_target(e);

    if(target==objects.ch_curve_select){
        // Channel Editor: restore the shared picker to its native screen.
        curvePickerForMixAction=false;

        if(objects.ch_edit_curve_list&&objects.scr_ch_settings&&
           lv_obj_get_parent(objects.ch_edit_curve_list)!=objects.scr_ch_settings)
            lv_obj_set_parent(objects.ch_edit_curve_list,objects.scr_ch_settings);
    }else{
        // Free Mix Action Editor: userdata 0..5 selects the Curve row.
        uint8_t slot=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
        if(slot>=6) return;

        activeMixCurveSlot=slot;
        curvePickerForMixAction=true;

        // ch_edit_curve_list is the same picker used by Channel Editor.
        // Reparent it temporarily so it is visible on the Mixes screen.
        if(objects.ch_edit_curve_list&&objects.scr_mixes&&
           lv_obj_get_parent(objects.ch_edit_curve_list)!=objects.scr_mixes)
            lv_obj_set_parent(objects.ch_edit_curve_list,objects.scr_mixes);
    }

    rebuildOutputCurveList();

    if(objects.ch_edit_curve_list){
        lv_obj_set_align(objects.ch_edit_curve_list,LV_ALIGN_CENTER);
        lv_obj_set_pos(objects.ch_edit_curve_list,0,0);
        lv_obj_remove_flag(objects.ch_edit_curve_list,LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(objects.ch_edit_curve_list);
    }
}

void action_selected_curve(lv_event_t *e){
    if(!e) return;

    lv_obj_t *row=(lv_obj_t *)lv_event_get_target(e);
    setExclusiveChecked(lv_obj_get_parent(row),row);

    uintptr_t user_data=(uintptr_t)lv_event_get_user_data(e);
    int8_t selection=OUTPUT_CURVE_NONE;

    if(user_data==0) selection=OUTPUT_CURVE_NONE;
    else if(user_data==1) selection=OUTPUT_CURVE_EXPO;
    else return;

    if(curvePickerForMixAction){
        if(activeMixCurveSlot>=6) return;
        if(isAileronMixEditor()){
            aileronEditorState.curveSelection[activeMixCurveSlot]=selection;
            saveAileronEditorState();
        }else mixActionCurveSelection[activeMixCurveSlot]=selection;
        updateMixCurveSelectionUi(activeMixCurveSlot);

        if(selectedMixIndex>=0)
            drawMixGraph((uint8_t)selectedMixIndex);
    }else{
        if(editedChannel>=MAX_MODEL_CHANNELS) return;
        ModelConfig &model=modelManager.getActiveModel();
        model.outputs[editedChannel].curveIndex=selection;
        updateOutputCurveSelectionUi(editedChannel);
    }

    if(objects.ch_edit_curve_list)
        lv_obj_add_flag(objects.ch_edit_curve_list,LV_OBJ_FLAG_HIDDEN);
}

void action_open_scr_curve(lv_event_t *e){
    openScreen(SCREEN_ID_SCR_CURVE);
    if(objects.curve_main_panel!=NULL)
        lv_obj_remove_flag(objects.curve_main_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.curve_edit_panel!=NULL)
        lv_obj_add_flag(objects.curve_edit_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.curve_diag!=NULL&&objects.curve_diag_panel!=NULL)
        lv_obj_set_parent(objects.curve_diag,objects.curve_diag_panel);
    refreshCurveList();
    if(selectedCurveIndex>=0)
        drawCurveGraph((uint8_t)selectedCurveIndex);
}
void action_select_curve(lv_event_t *e){
    lv_obj_remove_flag(objects.curve_type_selector, LV_OBJ_FLAG_HIDDEN);
}
void action_set_curve_to(lv_event_t *e){
    if(selectedCurveIndex<0||selectedCurveIndex>=MAX_MODEL_CURVES) return;
    ModelConfig &model=modelManager.getActiveModel();
    CurveConfig &curve=model.curves[selectedCurveIndex];
    uintptr_t user_data=(uintptr_t)lv_event_get_user_data(e);
    lv_obj_add_flag(objects.curve_type_selector,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.expo_curv_panel,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.function_curv_panel,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.custom_curv_panel,LV_OBJ_FLAG_HIDDEN);
    switch(user_data){
        case 0:
            curve.type=CURVE_TYPE_NONE;
            lv_label_set_text(objects.seleted_curve_type_lbl,"---");
            break;
        case 1:
            curve.type=CurveType::EXPO;
            lv_label_set_text(objects.seleted_curve_type_lbl,"Expo");
            lv_obj_remove_flag(objects.expo_curv_panel,LV_OBJ_FLAG_HIDDEN);
            break;
        case 2:
            curve.type=CurveType::FUNCTION;
            curve.function=CurveFunction::NONE;
            lv_label_set_text(objects.seleted_curve_type_lbl,"Function");
            lv_label_set_text(objects.selected_function,"---");
            lv_obj_remove_flag(objects.function_curv_panel,LV_OBJ_FLAG_HIDDEN);
            break;
        case 3:
            curve.type=CurveType::CUSTOM;
            lv_label_set_text(objects.seleted_curve_type_lbl,"Custom");
            lv_obj_remove_flag(objects.custom_curv_panel,LV_OBJ_FLAG_HIDDEN);
            break;
        default:return;
    }
    drawCurveGraph((uint8_t)selectedCurveIndex);
}
void action_add_curve(lv_event_t *e){
    curveEditorReturnToChannel=false;
    ModelConfig &model=modelManager.getActiveModel();
    int8_t freeIndex=-1;
    for(uint8_t i=0;i<MAX_MODEL_CURVES;i++){
        if(!model.curves[i].enabled){
            freeIndex=(int8_t)i;
            break;
        }
    }
    if(freeIndex<0) return;
    CurveConfig &curve=model.curves[freeIndex];
    curve=CurveConfig{};
    curve.enabled=true;
    curve.type=CURVE_TYPE_NONE;
    curve.weight=1000;
    curve.offset=0;
    curve.expo=40;
    curve.function=CurveFunction::X_POSITIVE;
    curve.smooth=false;
    curve.easyMode=true;
    curve.pointCount=5;
    snprintf(curve.name,sizeof(curve.name),"Curve%d",freeIndex+1);
    for(uint8_t i=0;i<curve.pointCount;i++){
        int16_t value=(int16_t)(-1000L+(2000L*i)/(curve.pointCount-1));
        curve.x[i]=value;
        curve.y[i]=value;
    }
    curveEditorIsNew=true;
    editingCurveIndex=freeIndex;
    selectedCurveIndex=freeIndex;
    loadCurveToEditor((uint8_t)freeIndex);
    lv_obj_add_flag(objects.curve_main_panel,LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(objects.curve_edit_panel,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_parent(objects.curve_diag,objects.curve_diag_edit_panel);
    drawCurveGraph((uint8_t)freeIndex);
}
void action_open_points_picker(lv_event_t *e){
    uint8_t userdata=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(userdata==0){
        lv_obj_remove_flag(objects.curve_point_selector, LV_OBJ_FLAG_HIDDEN);
    }else if(userdata==1){
        lv_obj_remove_flag(objects.balance_pp, LV_OBJ_FLAG_HIDDEN);
    }
}
void action_open_function_selecter_panel(lv_event_t *e){
    lv_obj_remove_flag(objects.curve_function_selector, LV_OBJ_FLAG_HIDDEN);
}
void action_set_point_count(lv_event_t *e){
    uint8_t pointCount=(uint8_t)(uintptr_t)lv_event_get_user_data(e);

    if(objects.balance_pp!=NULL&&!lv_obj_has_flag(objects.balance_pp,LV_OBJ_FLAG_HIDDEN)){
        if(pointCount<2||pointCount>MAX_BALANCE_POINTS) return;
        balanceSettingsPointCount=pointCount;
        if(objects.balance_settings_panel__balance_settings_points_lbl!=NULL)
            lv_label_set_text_fmt(objects.balance_settings_panel__balance_settings_points_lbl,"%u points",pointCount);
        lv_obj_add_flag(objects.balance_pp,LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if(selectedCurveIndex<0||selectedCurveIndex>=MAX_MODEL_CURVES) return;
    if(pointCount<2||pointCount>MAX_CURVE_POINTS) return;
    ModelConfig &model=modelManager.getActiveModel();
    CurveConfig &curve=model.curves[selectedCurveIndex];
    currentCurvePointCount=pointCount;
    curve.pointCount=pointCount;
    lv_label_set_text_fmt(objects.curv_point_editor__point_count_lbl,"%u points",pointCount);
    lv_obj_add_flag(objects.curve_point_selector,LV_OBJ_FLAG_HIDDEN);
    updateCurvePointVisibility();
    distributeCurvePoints(pointCount);
    for(uint8_t i=0;i<pointCount;i++){
        curve.x[i]=curveEditX[i];
    }
    updateCurveEasyMode();
    drawCurveGraph((uint8_t)selectedCurveIndex);
}
void action_set_function_to(lv_event_t *e){
    if(selectedCurveIndex<0||selectedCurveIndex>=MAX_MODEL_CURVES) return;
    ModelConfig &model=modelManager.getActiveModel();
    CurveConfig &curve=model.curves[selectedCurveIndex];
    uint8_t func=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    switch(func){
        case 0:
            curve.function=CurveFunction::NONE;
            lv_label_set_text(objects.selected_function,"---");
            break;
        case 1:
            curve.function=CurveFunction::X_POSITIVE;
            lv_label_set_text(objects.selected_function,"x > 0");
            break;
        case 2:
            curve.function=CurveFunction::X_NEGATIVE;
            lv_label_set_text(objects.selected_function,"x < 0");
            break;
        case 3:
            curve.function=CurveFunction::ABS_X;
            lv_label_set_text(objects.selected_function,"| x |");
            break;
        case 4:
            curve.function=CurveFunction::F_POSITIVE;
            lv_label_set_text(objects.selected_function,"f > 0");
            break;
        case 5:
            curve.function=CurveFunction::F_NEGATIVE;
            lv_label_set_text(objects.selected_function,"f < 0");
            break;
        case 6:
            curve.function=CurveFunction::ABS_F;
            lv_label_set_text(objects.selected_function,"| f |");
            break;
        default:
            return;
    }
    lv_obj_add_flag(objects.curve_function_selector,LV_OBJ_FLAG_HIDDEN);
    drawCurveGraph((uint8_t)selectedCurveIndex);
}
void action_edit_menu_clicked(lv_event_t *e){
    if(sharedEditDeletePopupTarget==SharedEditDeletePopupTarget::ACTION){
        if(editingMixIndex<0||editingMixIndex>=MAX_MIX_LINES) return;
        if(selectedMixActionIndex<0||selectedMixActionIndex>=MAX_MIX_ACTIONS) return;

        uint8_t mixIndex=(uint8_t)editingMixIndex;
        uint8_t actionIndex=(uint8_t)selectedMixActionIndex;

        ModelConfig &model=modelManager.getActiveModel();
        const MixLine &mix=model.mixes[mixIndex];

        if(actionIndex>=mix.actionCount) return;
        if(!mix.actions[actionIndex].enabled) return;

        if(objects.curve_popup)
            lv_obj_add_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN);
        hidePopupDismissOverlay();

        pendingMixActionMixIndex=mixIndex;
        pendingMixActionIndex=actionIndex;

        if(mixActionOpenPending) return;

        mixActionOpenPending=true;
        lv_async_call(openMixActionEditorAsync,nullptr);
        return;
    }

    if(sharedEditDeletePopupTarget==SharedEditDeletePopupTarget::VAR){
        if(selectedVarIndex<0||selectedVarIndex>=MAX_MODEL_VARS) return;
        uint8_t index=(uint8_t)selectedVarIndex;
        hideVarPopup();
        openVarEditor(index);
        return;
    }

    if(sharedEditDeletePopupTarget==SharedEditDeletePopupTarget::MIX){
        if(selectedMixIndex<0||selectedMixIndex>=MAX_MIX_LINES) return;
        openExistingMixEditor((uint8_t)selectedMixIndex);
        return;
    }

    if(selectedCurveIndex<0) return;
    curveEditorReturnToChannel=false;
    curveEditorIsNew=false;
    editingCurveIndex=selectedCurveIndex;
    loadCurveToEditor((uint8_t)editingCurveIndex);
    lv_obj_add_flag(objects.curve_main_panel,LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(objects.curve_edit_panel,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_parent(objects.curve_diag,objects.curve_diag_edit_panel);
    drawCurveGraph((uint8_t)editingCurveIndex);
}
void action_set_easymode(lv_event_t *e){
    if(selectedCurveIndex<0||selectedCurveIndex>=MAX_MODEL_CURVES) return;
    ModelConfig &model=modelManager.getActiveModel();
    CurveConfig &curve=model.curves[selectedCurveIndex];
    curve.easyMode=lv_obj_has_state(objects.custom_curv_easy_mode_sw,LV_STATE_CHECKED);
    updateCurveEasyMode();
    if(curve.easyMode){
        for(uint8_t i=0;i<currentCurvePointCount;i++){
            curve.x[i]=curveEditX[i];
        }
    }
    drawCurveGraph((uint8_t)selectedCurveIndex);
}
void update_BB_widget_visibility(ScreensEnum screen_id)
{
    // 1. Обновляем текстовый заголовок в статус-баре
    if (objects.global_status_bar__caption != NULL) {
        lv_label_set_text(objects.global_status_bar__caption, getScreenTitle(screen_id));
    }
    // 2. Управление кнопкой "Назад" в верхнем статус-баре
    if (objects.global_status_bar__goback) {
        if (screen_id == SCREEN_ID_SCR_MAIN) {
            lv_obj_add_flag(objects.global_status_bar__goback, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(objects.global_status_bar__goback, LV_OBJ_FLAG_HIDDEN);
        }
    }
    // 3. Управление нижней панелью навигации (Bottom Bar)
    if (!objects.global_bottom_bar) return;
    // Проверяем, входит ли экран в список 4 основных вкладок
    bool is_tab_screen = (screen_id == SCREEN_ID_SCR_MAIN || screen_id == SCREEN_ID_SCR_MODEL || screen_id == SCREEN_ID_SCR_TELEMETRY || screen_id == SCREEN_ID_SCR_SETTINGS);
    if (!is_tab_screen) {
        lv_obj_add_flag(objects.global_bottom_bar, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    // 4. Показываем нижний бар
    lv_obj_remove_flag(objects.global_bottom_bar, LV_OBJ_FLAG_HIDDEN);
    // Вспомогательная лямбда для переключения состояния иконки (CHECKED)
    auto update_state = [screen_id](lv_obj_t *lbl, ScreensEnum target_id) {
        if (!lbl) return;
        if (screen_id == target_id) lv_obj_add_state(lbl, LV_STATE_CHECKED);
        else lv_obj_remove_state(lbl, LV_STATE_CHECKED);
    };
    update_state(objects.global_bottom_bar__home_lbl,      SCREEN_ID_SCR_MAIN);
    update_state(objects.global_bottom_bar__model_lbl,     SCREEN_ID_SCR_MODEL);
    update_state(objects.global_bottom_bar__telemetry_lbl, SCREEN_ID_SCR_TELEMETRY);
    update_state(objects.global_bottom_bar__settings_lbl,  SCREEN_ID_SCR_SETTINGS);
}
void update_active_field_ui() {
    float min_b, max_b;
    get_current_field_bounds(min_b, max_b);
    float* val_ptr = get_active_field_ptr();
    if (!val_ptr) return;
    update_dropdown_step_options();
    // 1. Ограничиваем значение диапазона (Clamp)
    if (*val_ptr < min_b) *val_ptr = min_b;
    if (*val_ptr > max_b) *val_ptr = max_b;
    // 2. Синхронизируем положение слайдера
    if(objects.ch_edit_changer_panel__mcmsusd_slider!=NULL){
        if(active_field_is_pwm||active_field==FIELD_EXPO_CURVE){
            lv_slider_set_range(objects.ch_edit_changer_panel__mcmsusd_slider,(int32_t)min_b,(int32_t)max_b);
            lv_slider_set_value(objects.ch_edit_changer_panel__mcmsusd_slider,(int32_t)(*val_ptr),LV_ANIM_OFF);
        }else{
            lv_slider_set_range(objects.ch_edit_changer_panel__mcmsusd_slider,(int32_t)(min_b*10.0f),(int32_t)(max_b*10.0f));
            lv_slider_set_value(objects.ch_edit_changer_panel__mcmsusd_slider,(int32_t)((*val_ptr)*10.0f),LV_ANIM_OFF);
        }
    }
    // 3. Форматируем значение в строку в зависимости от типа поля
    char buf[16];
    lv_obj_t* target_ta=NULL;
    if(active_field_is_pwm){
        target_ta=objects.ch_pwm;
        snprintf(buf,sizeof(buf),"%dus",(int)lroundf(*val_ptr));
    }else switch(active_field){
        case FIELD_MIN:        
            target_ta = objects.ch_min; 
            snprintf(buf, sizeof(buf), "%.1f%%", *val_ptr);
            break;
        case FIELD_CENTER:     
            target_ta = objects.ch_center; 
            snprintf(buf, sizeof(buf), "%.1f%%", *val_ptr);
            break;
        case FIELD_MAX:        
            target_ta = objects.ch_max; 
            snprintf(buf, sizeof(buf), "%.1f%%", *val_ptr);
            break;
        case FIELD_EXPO_CURVE: 
            target_ta = objects.ch_expo_percent; 
            snprintf(buf, sizeof(buf), "%.0f%%", *val_ptr); // Без десятичной части (целые %)
            break;
        case FIELD_SLOW_UP:    
            target_ta = objects.ch_slow_up; 
            snprintf(buf, sizeof(buf), "%.1fс", *val_ptr);
            break;
        case FIELD_SLOW_DOWN:  
            target_ta = objects.ch_slow_down; 
            snprintf(buf, sizeof(buf), "%.1fс", *val_ptr);
            break;
    }
    // Записываем результат в объект LVGL
    if (target_ta != NULL) lv_label_set_text(target_ta, buf);
}
void set_disabled_recursive(lv_obj_t *obj, bool disabled){
    if (!obj) return;
    if (disabled) lv_obj_add_state(obj, LV_STATE_DISABLED);
    else lv_obj_remove_state(obj, LV_STATE_DISABLED);
    uint32_t count = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < count; i++){
        lv_obj_t *child = lv_obj_get_child(obj, i);
        set_disabled_recursive(child, disabled);
    }
}
void loadThemeSettings(){
    Preferences prefs;
    prefs.begin("gen_settings", true);
    uint32_t theme = prefs.getUInt("theme", THEME_ID_UI_THEME_DARK);
    uint32_t msColor = prefs.getUInt("ms_color", theme_colors[THEME_ID_UI_THEME_DARK][COLOR_ID_MS_TEXT]);
    uint8_t brightness = prefs.getUChar("brightness", 100);
    prefs.end();
    display_set_brightness(brightness);
    lv_slider_set_value(objects.brightness_changer, brightness, LV_ANIM_OFF);
    if(theme > THEME_ID_UI_THEME_LIGHT) theme = THEME_ID_UI_THEME_DARK;
    // Один MS_TEXT цвет для обеих тем
    theme_colors[THEME_ID_UI_THEME_DARK][COLOR_ID_MS_TEXT] = msColor;
    theme_colors[THEME_ID_UI_THEME_LIGHT][COLOR_ID_MS_TEXT] = msColor;
    
    // Применяем тему
    change_color_theme(theme);
    // Положение theme_switch
    if(theme == THEME_ID_UI_THEME_DARK)
        lv_obj_add_state(objects.theme_switch, LV_STATE_CHECKED);
    else
        lv_obj_remove_state(objects.theme_switch, LV_STATE_CHECKED);
    // Получаем RGB
    uint8_t r = (msColor >> 16) & 0xFF;
    uint8_t g = (msColor >> 8) & 0xFF;
    uint8_t b = msColor & 0xFF;
    lv_color_t color = lv_color_make(r, g, b);
    // HEX в color_pick_lbl
    char hex[8];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", r, g, b);
    lv_label_set_text(objects.color_pick_lbl, hex);
    lv_obj_set_style_text_color(objects.color_pick_lbl, color, LV_PART_MAIN);
    // Цвет панели-превью
    lv_obj_set_style_bg_color(objects.color_pick_panel, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(objects.color_pick_panel, LV_OPA_COVER, LV_PART_MAIN);
}
void saveGeneralSettings(){
    uint32_t theme = active_theme_index;
    uint32_t msColor = theme_colors[active_theme_index][COLOR_ID_MS_TEXT];
    uint8_t brightness = (uint8_t)lv_slider_get_value(objects.brightness_changer);
    Preferences prefs;
    prefs.begin("gen_settings", false);
    prefs.putUInt("theme", theme);
    prefs.putUInt("ms_color", msColor);
    prefs.putUChar("brightness", brightness);
    prefs.end();
}
//---для работы в окном Выбор модели
static void modelCardClicked(lv_event_t *e){
    uint16_t modelId=(uint16_t)(uintptr_t)lv_event_get_user_data(e);
    if(modelId==modelManager.getActiveModelId()) return;
    if(!modelManager.selectModel(modelId)) return;
    refreshModelList();
}
static void refreshModelList(){
    lv_obj_t *parent=objects.model_list_container;
    if(!parent) return;
    lv_obj_clean(parent);
    lv_obj_set_flex_flow(parent,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(parent,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(parent,10,0);
    lv_obj_set_style_pad_row(parent,10,0);
    lv_obj_set_scroll_dir(parent,LV_DIR_HOR);
    uint16_t activeId=modelManager.getActiveModelId();
    uint8_t count=modelManager.getModelCount();
    for(uint8_t i=0;i<count;i++){
        const ModelInfo *info=modelManager.getModelInfo(i);
        if(!info) continue;
        lv_obj_t *card=lv_obj_create(parent);
        lv_obj_set_size(card,180,180);
        lv_obj_set_flex_flow(card,LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(card,0,0);
        lv_obj_set_style_radius(card,6,0);
        lv_obj_clear_flag(card,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(card,LV_OBJ_FLAG_CLICKABLE);
        if(info->modelId==activeId){
            lv_obj_set_style_bg_color(card,lv_color_hex(0xFF8500),0);
            lv_obj_set_style_border_color(card,lv_color_hex(0xFF8500),0);
        }else{
            lv_obj_set_style_bg_color(card,lv_color_hex(0xD7D7D7),0);
            lv_obj_set_style_border_color(card,lv_color_hex(0x808080),0);
        }
        lv_obj_set_style_border_width(card,1,0);
        lv_obj_add_event_cb(card,modelCardClicked,LV_EVENT_CLICKED,(void *)(uintptr_t)info->modelId);
        lv_obj_t *title=lv_label_create(card);
        lv_label_set_text(title,info->name);
        lv_obj_set_width(title,160);
        lv_label_set_long_mode(title,LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(title,info->modelId==activeId?lv_color_white():lv_color_hex(0x505050),0);
        lv_obj_set_style_margin_top(title,8,0);
        lv_obj_t *separator=lv_obj_create(card);
        lv_obj_set_size(separator,180,1);
        lv_obj_set_style_bg_color(separator,lv_color_hex(0x808080),0);
        lv_obj_set_style_border_width(separator,0,0);
        lv_obj_set_style_pad_all(separator,0,0);
        lv_obj_clear_flag(separator,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *modelIcon=lv_label_create(card);
        lv_label_set_text(modelIcon,LV_SYMBOL_IMAGE);
        lv_obj_set_style_text_font(modelIcon,&lv_font_montserrat_32,0);
        lv_obj_set_style_text_color(modelIcon,lv_color_hex(0x505050),0);
        lv_obj_set_style_margin_top(modelIcon,25,0);
        if(info->modelId==activeId){
            lv_obj_t *check=lv_label_create(card);
            lv_label_set_text(check,LV_SYMBOL_OK);
            lv_obj_set_style_text_color(check,lv_color_white(),0);
            lv_obj_align(check,LV_ALIGN_TOP_RIGHT,-8,8);
        }
    }
    lv_obj_t *addCard=lv_obj_create(parent);
    lv_obj_set_size(addCard,180,180);
    lv_obj_clear_flag(addCard,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(addCard,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(addCard,addModelCardClicked,LV_EVENT_CLICKED,nullptr);
    lv_obj_set_style_bg_color(addCard,lv_color_hex(0xD7D7D7),0);
    lv_obj_set_style_border_color(addCard,lv_color_hex(0x808080),0);
    lv_obj_set_style_border_width(addCard,1,0);
    lv_obj_set_style_radius(addCard,6,0);
    lv_obj_t *plus=lv_label_create(addCard);
    lv_label_set_text(plus,LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(plus,&lv_font_montserrat_32,0);
    lv_obj_set_style_text_color(plus,lv_color_hex(0x505050),0);
    lv_obj_center(plus);
}
static void addModelCardClicked(lv_event_t *e){
    showCreateModelDialog();
}
static void createModelDialogCancel(lv_event_t *e){
    closeSystemConfirmation();
    createModelDialog=nullptr;
    createModelName=nullptr;
}
static void createModelDialogCreate(lv_event_t *e){
    if(!createModelName) return;
    const char *name=lv_textarea_get_text(createModelName);
    if(!name||name[0]=='\0') return;
    if(!modelManager.createModel(name)) return;
    closeSystemConfirmation();
    createModelDialog=nullptr;
    createModelName=nullptr;
    refreshModelList();
}
static void showCreateModelDialog(){
    if(createModelDialog) return;
    lv_obj_t *panel=createMixChannelQuestion("Model name:",nullptr);
    createModelDialog=mixChannelQuestionOverlay;
    // Reuse the same panel, header, theme palette and footer buttons.
    lv_obj_t *header=lv_obj_get_child(panel,0);
    if(header&&lv_obj_get_child_count(header)) lv_label_set_text(lv_obj_get_child(header,0),"Create model");
    createModelName=lv_textarea_create(panel);
    lv_obj_set_size(createModelName,520,48);
    lv_obj_align(createModelName,LV_ALIGN_TOP_MID,0,108);
    lv_textarea_set_one_line(createModelName,true);
    lv_textarea_set_placeholder_text(createModelName,"Model name");
    lv_obj_set_style_bg_color(createModelName,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_FRONT_BG1]),LV_PART_MAIN);
    lv_obj_set_style_text_color(createModelName,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_TEXT]),LV_PART_MAIN);
    lv_obj_set_style_border_color(createModelName,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_MS_TEXT]),LV_PART_MAIN|LV_STATE_FOCUSED);
    addMixChannelQuestionButton(panel,"Create",0,2,createModelDialogCreate,0);
    addMixChannelQuestionButton(panel,"Cancel",1,2,createModelDialogCancel,0);
}

static lv_obj_t *getMixCurveNameLabel(uint8_t slot){
    if(isAileronMixEditor()){
        lv_obj_t *labels[]={objects.aileron_mix_curve_name,objects.aileron_mix_curve_name1,objects.aileron_mix_curve_name2,objects.aileron_mix_curve_name3,objects.aileron_mix_curve_name4,objects.aileron_mix_curve_name5};
        return slot<6?labels[slot]:nullptr;
    }
    switch(slot){
        case 0:return objects.mix_curve_name;
        case 1:return objects.mix_curve_name1;
        case 2:return objects.mix_curve_name2;
        case 3:return objects.mix_curve_name3;
        case 4:return objects.mix_curve_name4;
        case 5:return objects.mix_curve_name5;
        default:return nullptr;
    }
}

static lv_obj_t *getMixCurveExpoPanel(uint8_t slot){
    if(isAileronMixEditor()){
        lv_obj_t *panels[]={objects.aileron_mix_curve_otherwise_weight_panel,objects.aileron_mix_curve_weight_panel1,objects.aileron_mix_curve_weight_panel2,objects.aileron_mix_curve_weight_panel3,objects.aileron_mix_curve_weight_panel4,objects.aileron_mix_curve_weight_panel5};
        return slot<6?panels[slot]:nullptr;
    }
    switch(slot){
        case 0:return objects.curve_weight_panel0;
        case 1:return objects.curve_weight_panel1;
        case 2:return objects.curve_weight_panel2;
        case 3:return objects.curve_weight_panel3;
        case 4:return objects.curve_weight_panel4;
        case 5:return objects.curve_weight_panel5;
        default:return nullptr;
    }
}

static lv_obj_t *getMixCurveEditPanel(uint8_t slot){
    if(isAileronMixEditor()){
        lv_obj_t *panels[]={objects.aileron_mix_curve_edit_panel,objects.aileron_mix_curve_edit_panel1,objects.aileron_mix_curve_edit_panel2,objects.aileron_mix_curve_edit_panel3,objects.aileron_mix_curve_edit_panel4,objects.aileron_mix_curve_edit_panel5};
        return slot<6?panels[slot]:nullptr;
    }
    switch(slot){
        case 0:return objects.mix_curve_edit0;
        case 1:return objects.mix_curve_edit1;
        case 2:return objects.mix_curve_edit2;
        case 3:return objects.mix_curve_edit3;
        case 4:return objects.mix_curve_edit4;
        case 5:return objects.mix_curve_edit5;
        default:return nullptr;
    }
}

static int8_t getActiveCurvePickerSelection(){
    if(curvePickerForMixAction){
        if(activeMixCurveSlot>=6) return OUTPUT_CURVE_NONE;
        return isAileronMixEditor()?aileronEditorState.curveSelection[activeMixCurveSlot]:mixActionCurveSelection[activeMixCurveSlot];
    }

    if(editedChannel>=MAX_MODEL_CHANNELS) return OUTPUT_CURVE_NONE;
    ModelConfig &model=modelManager.getActiveModel();
    return getOutputCurveSelection(model.outputs[editedChannel]);
}

static void updateMixCurveSelectionUi(uint8_t slot){
    if(slot>=6) return;

    ModelConfig &model=modelManager.getActiveModel();
    int8_t selection=isAileronMixEditor()?aileronEditorState.curveSelection[slot]:mixActionCurveSelection[slot];

    const char *name="---";
    bool showExpo=false;
    bool showEdit=false;

    if(selection==OUTPUT_CURVE_EXPO){
        name="Expo";
        showExpo=true;
    }else if(selection>=0&&selection<MAX_MODEL_CURVES){
        const CurveConfig &curve=model.curves[selection];
        if(curve.enabled){
            name=curve.name[0]!='\0'?curve.name:"---";
            showEdit=true;
        }else{
            if(isAileronMixEditor()) aileronEditorState.curveSelection[slot]=OUTPUT_CURVE_NONE;
            else mixActionCurveSelection[slot]=OUTPUT_CURVE_NONE;
        }
    }

    lv_obj_t *nameLabel=getMixCurveNameLabel(slot);
    lv_obj_t *expoPanel=getMixCurveExpoPanel(slot);
    lv_obj_t *editPanel=getMixCurveEditPanel(slot);

    if(nameLabel) lv_label_set_text(nameLabel,name);

    if(expoPanel){
        if(showExpo) lv_obj_remove_flag(expoPanel,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(expoPanel,LV_OBJ_FLAG_HIDDEN);
    }

    if(editPanel){
        if(showEdit) lv_obj_remove_flag(editPanel,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(editPanel,LV_OBJ_FLAG_HIDDEN);
    }
}

static int8_t getOutputCurveSelection(const OutputConfig &output){
    if(output.curveIndex==OUTPUT_CURVE_LEGACY)
        return output.expo!=0?OUTPUT_CURVE_EXPO:OUTPUT_CURVE_NONE;
    if(output.curveIndex==OUTPUT_CURVE_NONE||output.curveIndex==OUTPUT_CURVE_EXPO)
        return output.curveIndex;
    if(output.curveIndex>=0&&output.curveIndex<MAX_MODEL_CURVES)
        return output.curveIndex;
    return OUTPUT_CURVE_NONE;
}
static void updateOutputCurveSelectionUi(uint8_t ch){
    if(ch>=MAX_MODEL_CHANNELS) return;
    ModelConfig &model=modelManager.getActiveModel();
    OutputConfig &output=model.outputs[ch];
    int8_t selection=getOutputCurveSelection(output);
    const char *name="---";
    bool showExpo=false;
    bool showEdit=false;
    if(selection==OUTPUT_CURVE_EXPO){
        name="Expo";
        showExpo=true;
    }else if(selection>=0&&selection<MAX_MODEL_CURVES){
        const CurveConfig &curve=model.curves[selection];
        if(curve.enabled){
            name=curve.name[0]!='\0'?curve.name:"---";
            showEdit=true;
        }else{
            selection=OUTPUT_CURVE_NONE;
        }
    }
    if(objects.ch_editing_curve_name!=NULL)
        lv_label_set_text(objects.ch_editing_curve_name,name);
    if(objects.ch_expo_percent_panel!=NULL){
        if(showExpo)
            lv_obj_remove_flag(objects.ch_expo_percent_panel,LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(objects.ch_expo_percent_panel,LV_OBJ_FLAG_HIDDEN);
    }
    if(objects.ch_expo_curve_edit_panel!=NULL){
        if(showEdit)
            lv_obj_remove_flag(objects.ch_expo_curve_edit_panel,LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(objects.ch_expo_curve_edit_panel,LV_OBJ_FLAG_HIDDEN);
    }
}
static void clearDynamicCurveRows(){
    for(uint8_t i=0;i<dynamicCurveRowCount;i++){
        if(dynamicCurveRows[i]!=nullptr){
            lv_obj_delete(dynamicCurveRows[i]);
            dynamicCurveRows[i]=nullptr;
        }
    }
    dynamicCurveRowCount=0;
    if(dynamicAddCurveRow!=nullptr){
        lv_obj_delete(dynamicAddCurveRow);
        dynamicAddCurveRow=nullptr;
    }
}
static void copyCurveRowAppearance(lv_obj_t *row,lv_obj_t *templateRow){
    if(!row||!templateRow) return;
    lv_obj_update_layout(templateRow);
    lv_obj_set_width(row,487);
    lv_obj_set_height(row,65);
    add_style_container1_1(row);
    lv_obj_set_style_radius(row,0,LV_PART_MAIN);
    lv_obj_add_flag(row,LV_OBJ_FLAG_CHECKABLE);
}
static void rebuildOutputCurveList(){
    if(objects.ch_edit_curve_list__ch_editing_curve_list==NULL) return;
    clearDynamicCurveRows();
    ModelConfig &model=modelManager.getActiveModel();
    lv_obj_t *templateRow=objects.ch_edit_curve_list__ch_editing_curve_list_expo;
    lv_obj_t *templateLabel=templateRow?lv_obj_get_child(templateRow,0):nullptr;
    int8_t activeSelection=getActiveCurvePickerSelection();

    if(objects.ch_edit_curve_list__ch_editing_curve_list_none){
        if(activeSelection==OUTPUT_CURVE_NONE)
            lv_obj_add_state(objects.ch_edit_curve_list__ch_editing_curve_list_none,LV_STATE_CHECKED);
        else
            lv_obj_remove_state(objects.ch_edit_curve_list__ch_editing_curve_list_none,LV_STATE_CHECKED);
    }

    if(objects.ch_edit_curve_list__ch_editing_curve_list_expo){
        if(activeSelection==OUTPUT_CURVE_EXPO)
            lv_obj_add_state(objects.ch_edit_curve_list__ch_editing_curve_list_expo,LV_STATE_CHECKED);
        else
            lv_obj_remove_state(objects.ch_edit_curve_list__ch_editing_curve_list_expo,LV_STATE_CHECKED);
    }

    bool hasFreeSlot=false;
    for(uint8_t i=0;i<MAX_MODEL_CURVES;i++){
        const CurveConfig &curve=model.curves[i];
        if(!curve.enabled){
            hasFreeSlot=true;
            continue;
        }
        if(curve.name[0]=='\0') continue;
        if(dynamicCurveRowCount>=MAX_MODEL_CURVES) break;
        lv_obj_t *row=lv_obj_create(objects.ch_edit_curve_list__ch_editing_curve_list);
        dynamicCurveRows[dynamicCurveRowCount++]=row;
        copyCurveRowAppearance(row,templateRow);
        lv_obj_clear_flag(row,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(row,LV_OBJ_FLAG_CHECKABLE);
        lv_obj_add_event_cb(row,outputCurveRowClicked,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
        if(activeSelection==(int8_t)i)
            lv_obj_add_state(row,LV_STATE_CHECKED);
        else
            lv_obj_remove_state(row,LV_STATE_CHECKED);
        lv_obj_t *label=lv_label_create(row);
        lv_label_set_text(label,curve.name);
        if(templateLabel){
            lv_obj_set_style_text_font(label,lv_obj_get_style_text_font(templateLabel,LV_PART_MAIN),LV_PART_MAIN);
            lv_obj_set_style_text_color(label,lv_obj_get_style_text_color(templateLabel,LV_PART_MAIN),LV_PART_MAIN);
        }
        lv_obj_align(label,LV_ALIGN_LEFT_MID,0,0);
    }
    if(hasFreeSlot){
        dynamicAddCurveRow=lv_obj_create(objects.ch_edit_curve_list__ch_editing_curve_list);
        copyCurveRowAppearance(dynamicAddCurveRow,templateRow);
        lv_obj_clear_flag(dynamicAddCurveRow,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(dynamicAddCurveRow,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(dynamicAddCurveRow,addOutputCurveClicked,LV_EVENT_CLICKED,nullptr);
        lv_obj_t *label=lv_label_create(dynamicAddCurveRow);
        lv_label_set_text(label,"+ Add curve");
        if(templateLabel){
            lv_obj_set_style_text_font(label,lv_obj_get_style_text_font(templateLabel,LV_PART_MAIN),LV_PART_MAIN);
            lv_obj_set_style_text_color(label,lv_obj_get_style_text_color(templateLabel,LV_PART_MAIN),LV_PART_MAIN);
        }
        lv_obj_align(label,LV_ALIGN_LEFT_MID,0,0);
    }
}
static void setExclusiveChecked(lv_obj_t *parent,lv_obj_t *selected){
    if(!parent||!selected) return;
    uint32_t count=lv_obj_get_child_count(parent);
    for(uint32_t i=0;i<count;i++){
        lv_obj_t *child=lv_obj_get_child(parent,i);
        if(child) lv_obj_remove_state(child,LV_STATE_CHECKED);
    }
    lv_obj_add_state(selected,LV_STATE_CHECKED);
}
static void outputCurveRowClicked(lv_event_t *e){
    if(!e) return;

    lv_obj_t *row=(lv_obj_t *)lv_event_get_target(e);
    setExclusiveChecked(lv_obj_get_parent(row),row);

    uint8_t curveIndex=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(curveIndex>=MAX_MODEL_CURVES) return;

    ModelConfig &model=modelManager.getActiveModel();
    if(!model.curves[curveIndex].enabled) return;

    if(curvePickerForMixAction){
        if(activeMixCurveSlot>=6) return;
        mixActionCurveSelection[activeMixCurveSlot]=(int8_t)curveIndex;
        updateMixCurveSelectionUi(activeMixCurveSlot);

        if(selectedMixIndex>=0)
            drawMixGraph((uint8_t)selectedMixIndex);
    }else{
        if(editedChannel>=MAX_MODEL_CHANNELS) return;
        model.outputs[editedChannel].curveIndex=(int8_t)curveIndex;
        updateOutputCurveSelectionUi(editedChannel);
    }

    if(row) lv_obj_add_state(row,LV_STATE_CHECKED);

    if(objects.ch_edit_curve_list)
        lv_obj_add_flag(objects.ch_edit_curve_list,LV_OBJ_FLAG_HIDDEN);
}

static void addOutputCurveClicked(lv_event_t *e){
    if(!curvePickerForMixAction&&editedChannel>=MAX_MODEL_CHANNELS) return;

    ModelConfig &model=modelManager.getActiveModel();
    int8_t freeIndex=-1;
    for(uint8_t i=0;i<MAX_MODEL_CURVES;i++){
        if(!model.curves[i].enabled){
            freeIndex=(int8_t)i;
            break;
        }
    }
    if(freeIndex<0) return;
    CurveConfig &curve=model.curves[freeIndex];
    curve=CurveConfig{};
    curve.enabled=true;
    curve.type=CURVE_TYPE_NONE;
    curve.weight=1000;
    curve.offset=0;
    curve.expo=40;
    curve.function=CurveFunction::X_POSITIVE;
    curve.smooth=false;
    curve.easyMode=true;
    curve.pointCount=5;
    snprintf(curve.name,sizeof(curve.name),"Curve%d",freeIndex+1);
    for(uint8_t i=0;i<curve.pointCount;i++){
        int16_t value=(int16_t)(-1000L+(2000L*i)/(curve.pointCount-1));
        curve.x[i]=value;
        curve.y[i]=value;
    }
    curveEditorReturnToAileron=curvePickerForMixAction&&isAileronMixEditor();
    curveEditorReturnToMixAction=curvePickerForMixAction&&!curveEditorReturnToAileron;
    curveEditorReturnToChannel=!curvePickerForMixAction;
    curveEditorIsNew=true;
    editingCurveIndex=freeIndex;
    selectedCurveIndex=freeIndex;
    if(objects.ch_edit_curve_list!=NULL)
        lv_obj_add_flag(objects.ch_edit_curve_list,LV_OBJ_FLAG_HIDDEN);
    openScreen(SCREEN_ID_SCR_CURVE);
    loadCurveToEditor((uint8_t)freeIndex);
    lv_obj_add_flag(objects.curve_main_panel,LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(objects.curve_edit_panel,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_parent(objects.curve_diag,objects.curve_diag_edit_panel);
    drawCurveGraph((uint8_t)freeIndex);
}
static const char *getCurveTypeName(CurveType type){
    if(type==CURVE_TYPE_NONE) return "---";
    switch(type){
        case CurveType::EXPO:return "Expo";
        case CurveType::FUNCTION:return "Function";
        case CurveType::CUSTOM:return "Custom";
        default:return "---";
    }
}
static void curveListRowClicked(lv_event_t *e){
    if(!e) return;
    lv_obj_t *row=(lv_obj_t *)lv_event_get_current_target(e);
    if(!row) return;
    uint8_t curveIndex=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(curveIndex>=MAX_MODEL_CURVES) return;

    bool alreadyFocused=selectedCurveIndex==(int8_t)curveIndex&&
        (lv_obj_has_state(row,LV_STATE_FOCUSED)||lv_obj_has_state(row,LV_STATE_CHECKED));

    setExclusiveChecked(lv_obj_get_parent(row),row);
    selectedCurveIndex=(int8_t)curveIndex;
    drawCurveGraph(curveIndex);

    if(alreadyFocused){
        prepareSharedEditDeletePopupForCurve();
        lv_obj_remove_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN);
        showPopupDismissOverlay(objects.curve_popup);
    }
}
static void refreshCurveList(){
    if(objects.curve_list_panel==nullptr) return;
    lv_obj_clean(objects.curve_list_panel);
    lv_obj_set_flex_flow(objects.curve_list_panel,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(objects.curve_list_panel,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(objects.curve_list_panel,0,0);
    lv_obj_set_style_pad_all(objects.curve_list_panel,0,0);
    lv_obj_set_scroll_dir(objects.curve_list_panel,LV_DIR_VER);
    ModelConfig &model=modelManager.getActiveModel();
    uint8_t rowIndex=0;
    for(uint8_t i=0;i<MAX_MODEL_CURVES;i++){
        CurveConfig &curve=model.curves[i];
        if(!curve.enabled) continue;
        lv_obj_t *row=lv_obj_create(objects.curve_list_panel);
        lv_obj_set_width(row,475);
        lv_obj_set_height(row,50);
        lv_obj_set_style_pad_all(row,0,0);
        lv_obj_set_style_border_width(row,0,LV_PART_MAIN);
        lv_obj_clear_flag(row,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row,LV_OBJ_FLAG_CLICKABLE);
        if((rowIndex&1)==0)
            add_style_container1(row);
        else
            add_style_container1_1(row);
        lv_obj_set_style_radius(row,0,LV_PART_MAIN);
        // curveListRowClicked manages exclusive row selection and first/second tap semantics.
        lv_obj_add_event_cb(row,exclusiveListRowPressed,LV_EVENT_PRESSED,nullptr);
        lv_obj_add_event_cb(row,curveListRowClicked,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
        lv_obj_t *num=lv_label_create(row);
        lv_label_set_text_fmt(num,"Curve%u",i+1);
        lv_obj_set_pos(num,5,10);
        lv_obj_t *type=lv_label_create(row);
        lv_label_set_text(type,getCurveTypeName(curve.type));
        lv_obj_set_pos(type,115,10);
        lv_obj_t *name=lv_label_create(row);
        lv_label_set_text(name,curve.name[0]!='\0'?curve.name:"");
        lv_obj_set_width(name,220);
        lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);
        lv_obj_set_pos(name,239,10);
        rowIndex++;
        Serial.printf("Curve %u: type=%s, name=%s\n",i,getCurveTypeName(curve.type),curve.name);
    }
}
//рисование графиков кривых в окне просмотра кривых
static void initCurveGraph(){
    if(objects.curve_diag==nullptr) return;
    if(curveGraphLine==nullptr){
        curveGraphLine=lv_line_create(objects.curve_diag);
        lv_obj_set_style_line_color(curveGraphLine,lv_color_hex(0xFF0000),0);
        lv_obj_set_style_line_width(curveGraphLine,2,0);
        lv_obj_remove_flag(curveGraphLine,LV_OBJ_FLAG_CLICKABLE);
    }
    for(uint8_t i=0;i<MAX_CURVE_POINTS;i++){
        if(curveGraphPointMarkers[i]!=nullptr) continue;
        curveGraphPointMarkers[i]=lv_obj_create(objects.curve_diag);
        lv_obj_set_size(curveGraphPointMarkers[i],7,7);
        lv_obj_set_style_radius(curveGraphPointMarkers[i],LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_color(curveGraphPointMarkers[i],lv_color_hex(0xFFFFFF),0);
        lv_obj_set_style_bg_opa(curveGraphPointMarkers[i],LV_OPA_COVER,0);
        lv_obj_set_style_border_width(curveGraphPointMarkers[i],0,0);
        lv_obj_set_style_pad_all(curveGraphPointMarkers[i],0,0);
        lv_obj_remove_flag(curveGraphPointMarkers[i],LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(curveGraphPointMarkers[i],LV_OBJ_FLAG_HIDDEN);
    }
}
static void drawCurveGraph(uint8_t curveIndex){
    if(objects.curve_diag==nullptr) return;
    if(curveIndex>=MAX_MODEL_CURVES) return;
    ModelConfig &model=modelManager.getActiveModel();
    CurveConfig &curve=model.curves[curveIndex];
    if(!curve.enabled) return;
    initCurveGraph();
    if(curve.type==CURVE_TYPE_NONE){
        if(curveGraphLine) lv_obj_add_flag(curveGraphLine,LV_OBJ_FLAG_HIDDEN);
        for(uint8_t i=0;i<MAX_CURVE_POINTS;i++){
            if(curveGraphPointMarkers[i]) lv_obj_add_flag(curveGraphPointMarkers[i],LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    if(curveGraphLine) lv_obj_remove_flag(curveGraphLine,LV_OBJ_FLAG_HIDDEN);
    constexpr int32_t W=300;
    constexpr int32_t H=300;
    for(int32_t px=0;px<W;px++){
        int16_t input=(int16_t)(-1000+(px*2000)/(W-1));
        int16_t output=curveProcessor.process(input,curve);
        curveGraphPoints[px].x=px;
        curveGraphPoints[px].y=((1000-(int32_t)output)*(H-1))/2000;
    }
    lv_line_set_points(curveGraphLine,curveGraphPoints,W);
    lv_obj_set_pos(curveGraphLine,0,0);
    for(uint8_t i=0;i<MAX_CURVE_POINTS;i++)
        lv_obj_add_flag(curveGraphPointMarkers[i],LV_OBJ_FLAG_HIDDEN);
    if(curve.type==CurveType::CUSTOM){
        uint8_t count=curve.pointCount;
        if(count>MAX_CURVE_POINTS) count=MAX_CURVE_POINTS;
        for(uint8_t i=0;i<count;i++){
            int16_t xValue;
            if(curve.easyMode)
                xValue=(int16_t)(-1000+(2000L*i)/(count-1));
            else
                xValue=curve.x[i];
            int16_t yValue=curveProcessor.process(xValue,curve);
            int32_t x=((int32_t)xValue+1000)*(W-1)/2000;
            int32_t y=(1000-(int32_t)yValue)*(H-1)/2000;
            lv_obj_set_pos(curveGraphPointMarkers[i],x-3,y-3);
            lv_obj_remove_flag(curveGraphPointMarkers[i],LV_OBJ_FLAG_HIDDEN);
        }
    }
}
//открытие данных в окне редактирования кривой
static void loadCurveToEditor(uint8_t curveIndex){
    if(curveIndex>=MAX_MODEL_CURVES) return;
    ModelConfig &model=modelManager.getActiveModel();
    CurveConfig &curve=model.curves[curveIndex];
    if(!curve.enabled) return;
    selectedCurveIndex=(int8_t)curveIndex;
    editingCurveIndex=(int8_t)curveIndex;
    lv_textarea_set_text(objects.curve_name,curve.name);
    lv_obj_add_flag(objects.expo_curv_panel,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.function_curv_panel,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.custom_curv_panel,LV_OBJ_FLAG_HIDDEN);
    char buf[16];
    if(curve.type==CURVE_TYPE_NONE){
        lv_label_set_text(objects.seleted_curve_type_lbl,"---");
        return;
    }
    switch(curve.type){
        case CurveType::EXPO:
            lv_label_set_text(objects.seleted_curve_type_lbl,"Expo");
            snprintf(buf,sizeof(buf),"%d%%",(int)lroundf(curve.weight/10.0f));
            lv_label_set_text(objects.curve_expo_weight,buf);
            snprintf(buf,sizeof(buf),"%.1f%%",curve.offset/10.0f);
            lv_label_set_text(objects.curve_expo_offset,buf);
            snprintf(buf,sizeof(buf),"%d%%",(int)curve.expo);
            lv_label_set_text(objects.curve_expo_expo,buf);
            lv_obj_remove_flag(objects.expo_curv_panel,LV_OBJ_FLAG_HIDDEN);
            break;
        case CurveType::FUNCTION:{
            lv_label_set_text(objects.seleted_curve_type_lbl,"Function");
            const char *functionName="---";
            switch(curve.function){
                case CurveFunction::X_POSITIVE:functionName="x > 0";break;
                case CurveFunction::X_NEGATIVE:functionName="x < 0";break;
                case CurveFunction::ABS_X:functionName="| x |";break;
                case CurveFunction::F_POSITIVE:functionName="f > 0";break;
                case CurveFunction::F_NEGATIVE:functionName="f < 0";break;
                case CurveFunction::ABS_F:functionName="| f |";break;
                case CurveFunction::NONE:functionName="---";break;
            }
            lv_label_set_text(objects.selected_function,functionName);
            snprintf(buf,sizeof(buf),"%.1f%%",curve.offset/10.0f);
            lv_label_set_text(objects.curve_function_offset,buf);
            lv_obj_remove_flag(objects.function_curv_panel,LV_OBJ_FLAG_HIDDEN);
            break;
        }
        case CurveType::CUSTOM:
            lv_label_set_text(objects.seleted_curve_type_lbl,"Custom");
            snprintf(buf,sizeof(buf),"%.1f%%",curve.offset/10.0f);
            lv_label_set_text(objects.custom_curv_offset_lbl,buf);
            lv_obj_remove_flag(objects.custom_curv_panel,LV_OBJ_FLAG_HIDDEN);
            currentCurvePointCount=constrain((int)curve.pointCount,2,(int)MAX_CURVE_POINTS);
            lv_label_set_text_fmt(objects.curv_point_editor__point_count_lbl,"%u points",currentCurvePointCount);
            if(curve.easyMode) lv_obj_add_state(objects.custom_curv_easy_mode_sw,LV_STATE_CHECKED);
            else lv_obj_remove_state(objects.custom_curv_easy_mode_sw,LV_STATE_CHECKED);
            if(curve.smooth) lv_obj_add_state(objects.custom_curv_smooth_sw,LV_STATE_CHECKED);
            else lv_obj_remove_state(objects.custom_curv_smooth_sw,LV_STATE_CHECKED);
            updateCurvePointVisibility();
            if(curve.easyMode){
                distributeCurvePoints(currentCurvePointCount);
                for(uint8_t i=0;i<currentCurvePointCount;i++) curve.x[i]=curveEditX[i];
            }else{
                for(uint8_t i=0;i<currentCurvePointCount;i++){
                    curveEditX[i]=curve.x[i];
                    updateCurvePointXLabel(i);
                }
                curveEditX[0]=-1000;
                curveEditX[currentCurvePointCount-1]=1000;
                curve.x[0]=-1000;
                curve.x[currentCurvePointCount-1]=1000;
                updateCurvePointXLabel(0);
                updateCurvePointXLabel(currentCurvePointCount-1);
            }
            for(uint8_t i=0;i<currentCurvePointCount;i++){
                lv_obj_t *yLabel=getCurvePointY(i);
                if(!yLabel) continue;
                snprintf(buf,sizeof(buf),"%.1f%%",curve.y[i]/10.0f);
                lv_label_set_text(yLabel,buf);
            }
            updateCurveEasyMode();
            break;
    }
}
static void updateCurvePointVisibility(){
    for(uint8_t i=0;i<MAX_CURVE_POINTS;i++){
        lv_obj_t *point=getCurvePointContainer(i);
        if(!point) continue;
        if(i<currentCurvePointCount)
            lv_obj_remove_flag(point,LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(point,LV_OBJ_FLAG_HIDDEN);
    }
}
static void updateCurveEasyMode(){
    bool easyMode=lv_obj_has_state(objects.custom_curv_easy_mode_sw,LV_STATE_CHECKED);
    if(easyMode)
        distributeCurvePoints(currentCurvePointCount);
    for(uint8_t i=0;i<currentCurvePointCount;i++){
        lv_obj_t *x=getCurvePointX(i);
        if(!x) continue;
        bool endpoint=(i==0||i==currentCurvePointCount-1);
        if(endpoint||easyMode)
            lv_obj_add_state(x,LV_STATE_DISABLED);
        else
            lv_obj_remove_state(x,LV_STATE_DISABLED);
    }
}
static void saveCurveEditor(){
    if(editingCurveIndex<0||editingCurveIndex>=MAX_MODEL_CURVES) return;
    ModelConfig &model=modelManager.getActiveModel();
    CurveConfig &curve=model.curves[editingCurveIndex];
    if(objects.curve_name){
        const char *name=lv_textarea_get_text(objects.curve_name);
        if(name&&name[0]!='\0'){
            strncpy(curve.name,name,sizeof(curve.name)-1);
            curve.name[sizeof(curve.name)-1]='\0';
        }
    }
    curve.pointCount=currentCurvePointCount;
    curve.easyMode=lv_obj_has_state(objects.custom_curv_easy_mode_sw,LV_STATE_CHECKED);
    curve.smooth=lv_obj_has_state(objects.custom_curv_smooth_sw,LV_STATE_CHECKED);
    if(curve.type==CurveType::CUSTOM){
        for(uint8_t i=0;i<currentCurvePointCount;i++){
            curve.x[i]=curveEditX[i];
        }
    }
    if(!modelManager.requestSaveActiveModel())
        Serial.println("ERROR: Curve save failed");
    else
        Serial.printf("Curve %d saved\n",editingCurveIndex+1);
    curveEditorIsNew=false;
}
// Curve references must be resolved in both legacy fields and ordered Mix Actions.
// Aileron uses the same persisted MixAction::CURVE variants as FreeMix.
static uint16_t getCurveUsageCount(uint8_t curveIndex){
    const ModelConfig &model=modelManager.getActiveModel();
    uint16_t count=0;
    for(uint8_t i=0;i<MAX_MODEL_CHANNELS;i++){
        if(model.outputs[i].curveIndex==(int8_t)curveIndex) count++;
    }
    for(uint8_t i=0;i<MAX_MODEL_INPUTS;i++){
        if(model.inputs[i].curveIndex==(int8_t)curveIndex) count++;
    }
    for(uint8_t i=0;i<MAX_MIX_LINES;i++){
        const MixLine &mix=model.mixes[i];
        if(!mix.enabled) continue;
        if(mix.curveIndex==(int8_t)curveIndex) count++;
        for(uint8_t a=0;a<mix.actionCount&&a<MAX_MIX_ACTIONS;a++){
            const MixAction &action=mix.actions[a];
            if(!action.enabled||action.type!=MixActionType::CURVE) continue;
            for(uint8_t v=0;v<action.variantCount&&v<MAX_MIX_ACTION_VARIANTS;v++){
                if(action.variants[v].curveIndex==(int8_t)curveIndex) count++;
            }
        }
    }
    return count;
}

// Append a human-readable owner once (a mix can reference the curve repeatedly).
static void appendCurveUsageOwner(char *text,size_t capacity,const char *owner,uint16_t &shown,uint16_t &remaining){
    if(!owner||!owner[0]) return;
    if(shown>=4){remaining++;return;}
    size_t length=strlen(text);
    if(length>=capacity-1) return;
    snprintf(text+length,capacity-length,"%s%s",shown?", ":"",owner);
    shown++;
}

static void formatCurveUsageOwners(uint8_t curveIndex,char *buffer,size_t size){
    if(!buffer||size==0) return;
    buffer[0]='\0';
    const ModelConfig &model=modelManager.getActiveModel();
    uint16_t shown=0,remaining=0;
    char name[48];
    for(uint8_t i=0;i<MAX_MODEL_CHANNELS;i++){
        if(model.outputs[i].curveIndex!=(int8_t)curveIndex) continue;
        snprintf(name,sizeof(name),"CH%u%s%s",(unsigned)(i+1),model.outputs[i].name[0]?" (":"",model.outputs[i].name);
        if(model.outputs[i].name[0]) strncat(name,")",sizeof(name)-strlen(name)-1);
        appendCurveUsageOwner(buffer,size,name,shown,remaining);
    }
    for(uint8_t i=0;i<MAX_MODEL_INPUTS;i++){
        if(model.inputs[i].curveIndex!=(int8_t)curveIndex) continue;
        snprintf(name,sizeof(name),"Input%u%s%s",(unsigned)(i+1),model.inputs[i].name[0]?" (":"",model.inputs[i].name);
        if(model.inputs[i].name[0]) strncat(name,")",sizeof(name)-strlen(name)-1);
        appendCurveUsageOwner(buffer,size,name,shown,remaining);
    }
    for(uint8_t i=0;i<MAX_MIX_LINES;i++){
        const MixLine &mix=model.mixes[i];
        if(!mix.enabled) continue;
        bool used=mix.curveIndex==(int8_t)curveIndex;
        for(uint8_t a=0;a<mix.actionCount&&a<MAX_MIX_ACTIONS&&!used;a++){
            const MixAction &action=mix.actions[a];
            if(!action.enabled||action.type!=MixActionType::CURVE) continue;
            for(uint8_t v=0;v<action.variantCount&&v<MAX_MIX_ACTION_VARIANTS;v++){
                if(action.variants[v].curveIndex==(int8_t)curveIndex){used=true;break;}
            }
        }
        if(!used) continue;
        snprintf(name,sizeof(name),"%s",mix.name[0]?mix.name:"Unnamed mix");
        appendCurveUsageOwner(buffer,size,name,shown,remaining);
    }
    if(remaining){
        size_t length=strlen(buffer);
        if(length<size-1) snprintf(buffer+length,size-length," + %u more",(unsigned)remaining);
    }
}

static void removeCurveReferences(uint8_t curveIndex){
    ModelConfig &model=modelManager.getActiveModel();
    for(uint8_t i=0;i<MAX_MODEL_CHANNELS;i++){
        if(model.outputs[i].curveIndex==(int8_t)curveIndex) model.outputs[i].curveIndex=OUTPUT_CURVE_NONE;
    }
    for(uint8_t i=0;i<MAX_MODEL_INPUTS;i++){
        if(model.inputs[i].curveIndex==(int8_t)curveIndex) model.inputs[i].curveIndex=-1;
    }
    for(uint8_t i=0;i<MAX_MIX_LINES;i++){
        MixLine &mix=model.mixes[i];
        if(mix.curveIndex==(int8_t)curveIndex) mix.curveIndex=OUTPUT_CURVE_NONE;
        for(uint8_t a=0;a<mix.actionCount&&a<MAX_MIX_ACTIONS;a++){
            MixAction &action=mix.actions[a];
            if(action.type!=MixActionType::CURVE) continue;
            for(uint8_t v=0;v<action.variantCount&&v<MAX_MIX_ACTION_VARIANTS;v++){
                if(action.variants[v].curveIndex==(int8_t)curveIndex)
                    action.variants[v].curveIndex=OUTPUT_CURVE_NONE;
            }
        }
    }
    // Keep the currently open editor's transient copy consistent with the model.
    for(uint8_t slot=0;slot<6;slot++){
        if(mixActionCurveSelection[slot]==(int8_t)curveIndex) mixActionCurveSelection[slot]=OUTPUT_CURVE_NONE;
        if(aileronEditorState.curveSelection[slot]==(int8_t)curveIndex) aileronEditorState.curveSelection[slot]=OUTPUT_CURVE_NONE;
    }
}
static void deleteSelectedCurve(){
    if(selectedCurveIndex<0||selectedCurveIndex>=MAX_MODEL_CURVES) return;
    ModelConfig &model=modelManager.getActiveModel();
    uint8_t curveIndex=(uint8_t)selectedCurveIndex;
    removeCurveReferences(curveIndex);
    model.curves[curveIndex]=CurveConfig{};
    if(!modelManager.requestSaveActiveModel()){
        Serial.println("ERROR: Curve delete failed");
        return;
    }
    selectedCurveIndex=-1;
    editingCurveIndex=-1;
    if(objects.curve_popup!=NULL)
        lv_obj_add_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN);
    refreshCurveList();
}
static void curveDeleteConfirmCb(lv_event_t *e){
    closeSystemConfirmation();
    deleteSelectedCurve();
}
static void curveDeleteCancelCb(lv_event_t *e){
    closeSystemConfirmation();
}
static void updateBalanceSettingsSmoothColors(){
    if(objects.balance_settings_panel__balance_settings_smooth_sw==NULL) return;

    bool focused=lv_obj_has_state(objects.balance_settings_panel__balance_settings_smooth_sw,LV_STATE_FOCUSED);
    bool checked=lv_obj_has_state(objects.balance_settings_panel__balance_settings_smooth_sw,LV_STATE_CHECKED);

    lv_color_t textColor=lv_color_hex(theme_colors[active_theme_index][COLOR_ID_TEXT]);
    lv_color_t selectedColor=lv_color_hex(theme_colors[active_theme_index][COLOR_ID_MS_TEXT]);

    if(objects.balance_settings_panel__balance_settings_smooth_off_lbl!=NULL)
        lv_obj_set_style_text_color(objects.balance_settings_panel__balance_settings_smooth_off_lbl,focused&&!checked?selectedColor:textColor,LV_PART_MAIN);

    if(objects.balance_settings_panel__balance_settings_smooth_on_lbl!=NULL)
        lv_obj_set_style_text_color(objects.balance_settings_panel__balance_settings_smooth_on_lbl,focused&&checked?selectedColor:textColor,LV_PART_MAIN);
}

static void balanceSettingsSmoothEvent(lv_event_t *e){
    if(e==NULL||objects.balance_settings_panel__balance_settings_smooth_sw==NULL) return;

    lv_event_code_t code=lv_event_get_code(e);
    if(code==LV_EVENT_VALUE_CHANGED)
        balanceSettingsSmooth=lv_obj_has_state(objects.balance_settings_panel__balance_settings_smooth_sw,LV_STATE_CHECKED);

    if(code==LV_EVENT_VALUE_CHANGED||code==LV_EVENT_FOCUSED||code==LV_EVENT_DEFOCUSED||code==LV_EVENT_CLICKED)
        updateBalanceSettingsSmoothColors();
}

static void loadBalanceSettingsUi(){
    if(editedChannel>=MAX_MODEL_CHANNELS) return;

    ModelConfig &model=modelManager.getActiveModel();
    OutputConfig &output=model.outputs[editedChannel];
    BalanceConfig &balance=output.balance;

    balanceSettingsPointCount=(uint8_t)constrain((int)balance.pointCount,2,(int)MAX_BALANCE_POINTS);
    balanceSettingsSmooth=balance.smooth;

    char channelText[48];
    if(output.name[0]!='\0')
        snprintf(channelText,sizeof(channelText),"CH%u %s",(unsigned)(editedChannel+1),output.name);
    else
        snprintf(channelText,sizeof(channelText),"CH%u",(unsigned)(editedChannel+1));

    if(objects.balance_settings_panel__balance_settings_channel_lbl!=NULL)
        lv_label_set_text(objects.balance_settings_panel__balance_settings_channel_lbl,channelText);

    if(objects.balance_settings_panel__balance_settings_points_lbl!=NULL)
        lv_label_set_text_fmt(objects.balance_settings_panel__balance_settings_points_lbl,"%u points",balanceSettingsPointCount);


    if(objects.balance_settings_panel__balance_settings_smooth_sw!=NULL){
        if(balanceSettingsSmooth)
            lv_obj_add_state(objects.balance_settings_panel__balance_settings_smooth_sw,LV_STATE_CHECKED);
        else
            lv_obj_remove_state(objects.balance_settings_panel__balance_settings_smooth_sw,LV_STATE_CHECKED);
    }

    if(!balanceSettingsUiBound){
        if(objects.balance_settings_panel__balance_settings_smooth_sw!=NULL)
            lv_obj_add_event_cb(objects.balance_settings_panel__balance_settings_smooth_sw,balanceSettingsSmoothEvent,LV_EVENT_ALL,nullptr);

        if(objects.balance_settings_panel__balance_settings_ok!=NULL)
            lv_obj_add_event_cb(objects.balance_settings_panel__balance_settings_ok,balanceSettingsApply,LV_EVENT_CLICKED,nullptr);

        if(objects.balance_settings_panel__balance_settings_cancel!=NULL)
            lv_obj_add_event_cb(objects.balance_settings_panel__balance_settings_cancel,balanceSettingsCancel,LV_EVENT_CLICKED,nullptr);

        balanceSettingsUiBound=true;
    }

    updateBalanceSettingsSmoothColors();
}

static void resampleBalancePoints(BalanceConfig &balance,uint8_t newPointCount){
    if(newPointCount<2||newPointCount>MAX_BALANCE_POINTS) return;
    if(balance.pointCount<2||balance.pointCount>MAX_BALANCE_POINTS){
        balance.pointCount=newPointCount;
        for(uint8_t i=0;i<MAX_BALANCE_POINTS;i++)
            balance.correction[i]=0;
        return;
    }

    if(balance.pointCount==newPointCount) return;

    BalanceConfig oldBalance=balance;
    int16_t newCorrection[MAX_BALANCE_POINTS]={};

    for(uint8_t i=0;i<newPointCount;i++){
        int16_t x=(int16_t)(-1000L+(2000L*i)/(newPointCount-1));
        newCorrection[i]=(int16_t)constrain((int32_t)balanceProcessor.getCorrection(x,oldBalance),-100L,100L);
    }

    for(uint8_t i=0;i<MAX_BALANCE_POINTS;i++)
        balance.correction[i]=i<newPointCount?newCorrection[i]:0;

    balance.pointCount=newPointCount;
}

static void balanceSettingsApply(lv_event_t *e){
    if(editedChannel>=MAX_MODEL_CHANNELS) return;

    ModelConfig &model=modelManager.getActiveModel();
    BalanceConfig &balance=model.outputs[editedChannel].balance;

    uint8_t newPointCount=(uint8_t)constrain((int)balanceSettingsPointCount,2,(int)MAX_BALANCE_POINTS);
    resampleBalancePoints(balance,newPointCount);
    balance.smooth=balanceSettingsSmooth;

    balancePointEditActive=false;
    activeBalancePoint=0;
    balancePointEditValue=0.0f;
    balanceEncoderLastStepMs=0;
    balanceEncoderLastDirection=0;

    if(objects.ch_edit_changer_panel!=NULL)
        lv_obj_add_flag(objects.ch_edit_changer_panel,LV_OBJ_FLAG_HIDDEN);

    if(objects.balance_pp!=NULL)
        lv_obj_add_flag(objects.balance_pp,LV_OBJ_FLAG_HIDDEN);

    if(objects.channels_list!=NULL)
        lv_obj_add_flag(objects.channels_list,LV_OBJ_FLAG_HIDDEN);

    if(objects.balance_settings_panel!=NULL)
        lv_obj_add_flag(objects.balance_settings_panel,LV_OBJ_FLAG_HIDDEN);

    drawChannelBalanceGraph(editedChannel);
    drawChannelBalanceMiniGraph(editedChannel);
    updateBalanceStickPosition();
    updateChannelBalanceUi(editedChannel);
    modelManager.requestSaveActiveModel();
}

static void balanceSettingsCancel(lv_event_t *e){
    if(objects.balance_pp!=NULL)
        lv_obj_add_flag(objects.balance_pp,LV_OBJ_FLAG_HIDDEN);

    if(objects.channels_list!=NULL)
        lv_obj_add_flag(objects.channels_list,LV_OBJ_FLAG_HIDDEN);

    if(objects.balance_settings_panel!=NULL)
        lv_obj_add_flag(objects.balance_settings_panel,LV_OBJ_FLAG_HIDDEN);
}

//для реализации баланса кривых в окне редактирования Output 
static void updateChannelBalanceUi(uint8_t ch){
    if(ch>=MAX_MODEL_CHANNELS) return;
    ModelConfig &model=modelManager.getActiveModel();
    BalanceConfig &balance=model.outputs[ch].balance;
    if(objects.ch_curve_balanse_add!=NULL)
        lv_label_set_text(objects.ch_curve_balanse_add,balance.enabled?"Edit":"Add");
    if(objects.ch_curve_balanse_clear_panel!=NULL){
        if(balance.enabled)
            lv_obj_remove_flag(objects.ch_curve_balanse_clear_panel,LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(objects.ch_curve_balanse_clear_panel,LV_OBJ_FLAG_HIDDEN);
    }
    if(objects.curve_balance_canvas_mini!=NULL){
        if(balance.enabled){
            lv_obj_remove_flag(objects.curve_balance_canvas_mini_panel,LV_OBJ_FLAG_HIDDEN);
            drawChannelBalanceMiniGraph(ch);
        }else{
            lv_obj_add_flag(objects.curve_balance_canvas_mini_panel,LV_OBJ_FLAG_HIDDEN);
        }
    }
    if(objects.channel_editing_widget__ch_edit_balance!=NULL){
    if(balance.enabled)
        lv_obj_remove_flag(objects.channel_editing_widget__ch_edit_balance,LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(objects.channel_editing_widget__ch_edit_balance,LV_OBJ_FLAG_HIDDEN);
    }
}
static void openChannelBalanceEditor(uint8_t ch){
    if(ch>=MAX_MODEL_CHANNELS) return;
    balancePointEditActive=false;
    if(objects.ch_edit_changer_panel!=NULL)
        lv_obj_add_flag(objects.ch_edit_changer_panel,LV_OBJ_FLAG_HIDDEN);
    ModelConfig &model=modelManager.getActiveModel();
    OutputConfig &output=model.outputs[ch];
    BalanceConfig &balance=output.balance;
    if(!balance.enabled){
        balance=BalanceConfig{};
        balance.enabled=true;
        balance.pointCount=5;
        balance.smooth=false;
    }
    if(objects.curve_ch_num!=NULL){
        char buf[12];
        snprintf(buf,sizeof(buf),"CH%u",(unsigned)(ch+1));
        lv_label_set_text(objects.curve_ch_num,buf);
    }
    if(objects.curve_ch_name!=NULL)
        lv_label_set_text(objects.curve_ch_name,output.name);
    if(objects.ch_editing_panel!=NULL)
        lv_obj_add_flag(objects.ch_editing_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.curve_balance_panel!=NULL)
        lv_obj_remove_flag(objects.curve_balance_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.stick_position_panel!=NULL&&!balanceStickPositionUiBound){
        lv_obj_add_flag(objects.stick_position_panel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(objects.stick_position_panel,LV_OBJ_FLAG_CHECKABLE);
        lv_obj_add_event_cb(objects.stick_position_panel,balanceStickPositionClicked,LV_EVENT_CLICKED,nullptr);
        balanceStickPositionUiBound=true;
    }
    if(objects.magnet_panel!=NULL&&!balanceMagnetUiBound){
        lv_obj_add_flag(objects.magnet_panel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(objects.magnet_panel,LV_OBJ_FLAG_CHECKABLE);
        lv_obj_add_event_cb(objects.magnet_panel,balanceMagnetClicked,LV_EVENT_CLICKED,nullptr);
        balanceMagnetUiBound=true;
    }
    if(objects.balance_editor_lock_panel!=NULL&&!balanceLockUiBound){
        lv_obj_add_flag(objects.balance_editor_lock_panel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(objects.balance_editor_lock_panel,LV_OBJ_FLAG_CHECKABLE);
        lv_obj_add_event_cb(objects.balance_editor_lock_panel,balanceLockClicked,LV_EVENT_CLICKED,nullptr);
        balanceLockUiBound=true;
    }
    outputProcessor.setBalancePositionOverride(ch,false,0);
    mixer.setInputLock(false);
    balanceLockedXValue=0;
    balanceAutoAnalogEnabled=false;
    balanceAutoAnalogCaptured=false;
    balanceMagnetEnabled=false;
    balanceCanvasEditMode=false;
    balanceEncoderNavIndex=0;
    if(objects.curve_balance_canvas!=NULL)
        lv_obj_remove_state(objects.curve_balance_canvas,LV_STATE_CHECKED);
    if(objects.stick_position_panel!=NULL)
        lv_obj_remove_state(objects.stick_position_panel,LV_STATE_CHECKED);
    if(objects.magnet_panel!=NULL)
        lv_obj_remove_state(objects.magnet_panel,LV_STATE_CHECKED);
    if(objects.balance_editor_lock_panel!=NULL)
        lv_obj_remove_state(objects.balance_editor_lock_panel,LV_STATE_CHECKED);
    drawChannelBalanceGraph(ch);
    applyBalanceEncoderSelection();
    updateBalanceStickPosition();
}
static void clearChannelBalance(uint8_t ch){
    if(ch>=MAX_MODEL_CHANNELS) return;
    balancePointEditActive=false;
    if(objects.ch_edit_changer_panel!=NULL)
        lv_obj_add_flag(objects.ch_edit_changer_panel,LV_OBJ_FLAG_HIDDEN);
    ModelConfig &model=modelManager.getActiveModel();
    model.outputs[ch].balance=BalanceConfig{};
    modelManager.requestSaveActiveModel();
    updateChannelBalanceUi(ch);
}
static void balancePointClicked(lv_event_t *e){
    if(e==NULL||editedChannel>=MAX_MODEL_CHANNELS) return;
    if(objects.curve_balance_canvas==NULL||!lv_obj_has_state(objects.curve_balance_canvas,LV_STATE_CHECKED)) return;
    uint8_t point=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    ModelConfig &model=modelManager.getActiveModel();
    BalanceConfig &balance=model.outputs[editedChannel].balance;
    if(!balance.enabled||point>=balance.pointCount||point>=MAX_BALANCE_POINTS) return;
    activeBalancePoint=point;
    balancePointEditActive=true;
    balancePointEditValue=balance.correction[point]/10.0f;
    balanceEncoderLastStepMs=0;
    balanceEncoderLastDirection=0;
    drawChannelBalanceGraph(editedChannel);
}
static int16_t getBalanceEncoderStep(int8_t direction){
    uint32_t now=millis();
    uint32_t dt=now-balanceEncoderLastStepMs;
    if(direction!=balanceEncoderLastDirection){
        balanceEncoderLastDirection=direction;
        balanceEncoderLastStepMs=now;
        return 1;
    }
    balanceEncoderLastStepMs=now;
    if(dt<=135) return 10;
    if(dt<=240) return 5;
    if(dt<=420) return 2;
    return 1;
}

static void redrawBalanceGraphsNow(){
    if(editedChannel>=MAX_MODEL_CHANNELS) return;

    drawChannelBalanceGraph(editedChannel);
    drawChannelBalanceMiniGraph(editedChannel);

    balanceGraphLastRedrawMs=millis();
    balanceGraphRedrawPending=false;
}

static void requestBalanceGraphRedraw(){
    uint32_t now=millis();

    if((uint32_t)(now-balanceGraphLastRedrawMs)>=BALANCE_GRAPH_REDRAW_INTERVAL_MS){
        redrawBalanceGraphsNow();
        return;
    }

    balanceGraphRedrawPending=true;
}

static void serviceBalanceGraphRedraw(){
    if(!balanceGraphRedrawPending) return;

    uint32_t now=millis();
    if((uint32_t)(now-balanceGraphLastRedrawMs)<BALANCE_GRAPH_REDRAW_INTERVAL_MS) return;

    redrawBalanceGraphsNow();
}

static lv_obj_t *getBalanceEncoderNavObject(uint8_t index){
    switch(index){
        case 0: return objects.curve_balance_canvas;
        case 1: return objects.stick_position_panel;
        case 2: return objects.magnet_panel;
        case 3: return objects.balance_editor_lock_panel;
        case 4: return objects.balance_editor_settings_panel;
        case 5: return objects.balance_editor_about_panel;
        default: return nullptr;
    }
}

static void applyBalanceEncoderSelection(){
    for(uint8_t i=0;i<BALANCE_ENCODER_NAV_COUNT;i++){
        lv_obj_t *obj=getBalanceEncoderNavObject(i);
        if(obj!=nullptr)
            lv_obj_remove_state(obj,LV_STATE_FOCUSED);
    }

    lv_obj_t *selected=getBalanceEncoderNavObject(balanceEncoderNavIndex);
    if(selected!=nullptr)
        lv_obj_add_state(selected,LV_STATE_FOCUSED);
}

static void moveBalanceEncoderSelection(int8_t direction){
    if(direction>0){
        if(balanceEncoderNavIndex<BALANCE_ENCODER_NAV_COUNT-1)
            balanceEncoderNavIndex++;
    }else if(direction<0){
        if(balanceEncoderNavIndex>0)
            balanceEncoderNavIndex--;
    }

    applyBalanceEncoderSelection();
}

static int16_t getBalanceCurrentXValue(){
    if(mixer.isInputLocked()) return balanceLockedXValue;

    int16_t xValue=(int16_t)constrain((int32_t)mixer.getChannel(editedChannel),-1000L,1000L);

    if(balanceAutoAnalogEnabled){
        int16_t bestDelta=0;
        uint8_t bestIndex=0;

        for(uint8_t i=0;i<static_cast<uint8_t>(AnalogInput::COUNT);i++){
            AnalogInput input=static_cast<AnalogInput>(i);
            int16_t current=inputManager.getAnalog(input);
            int16_t delta=(int16_t)abs((int32_t)current-(int32_t)balanceAutoAnalogPrevious[i]);

            if(delta>bestDelta){
                bestDelta=delta;
                bestIndex=i;
            }

            balanceAutoAnalogPrevious[i]=current;
        }

        if(bestDelta>=BALANCE_AUTO_ANALOG_THRESHOLD){
            balanceAutoAnalogInput=static_cast<AnalogInput>(bestIndex);
            balanceAutoAnalogCaptured=true;
        }

        if(balanceAutoAnalogCaptured)
            xValue=(int16_t)constrain((int32_t)inputManager.getAnalog(balanceAutoAnalogInput),-1000L,1000L);
    }

    return xValue;
}

static void selectNearestBalancePointToCurrentX(){
    if(editedChannel>=MAX_MODEL_CHANNELS) return;

    ModelConfig &model=modelManager.getActiveModel();
    BalanceConfig &balance=model.outputs[editedChannel].balance;
    if(!balance.enabled||balance.pointCount<2) return;

    int16_t xValue=getBalanceCurrentXValue();
    int32_t displayX=((int32_t)xValue+1000L)*779L/2000L;
    displayX=constrain(displayX,0L,779L);

    uint8_t nearestPoint=0;
    int32_t nearestDistance=INT32_MAX;

    for(uint8_t i=0;i<balance.pointCount;i++){
        int32_t pointX=(int32_t)i*779L/(balance.pointCount-1);
        int32_t distance=abs(displayX-pointX);

        if(distance<nearestDistance){
            nearestDistance=distance;
            nearestPoint=i;
        }
    }

    activeBalancePoint=nearestPoint;
    balancePointEditActive=true;
    balancePointEditValue=balance.correction[nearestPoint]/10.0f;
    balanceEncoderLastStepMs=0;
    balanceEncoderLastDirection=0;
    drawChannelBalanceGraph(editedChannel);
}

void balanceEncoderLeft(){
    if(current_active_screen==SCREEN_ID_SCR_MIXES){
        mixEncoderLeft();
        return;
    }

    if(objects.curve_balance_panel==NULL||lv_obj_has_flag(objects.curve_balance_panel,LV_OBJ_FLAG_HIDDEN)) return;

    if(balanceEncoderNavIndex==0&&objects.curve_balance_canvas!=NULL&&lv_obj_has_state(objects.curve_balance_canvas,LV_STATE_FOCUSED)&&lv_obj_has_state(objects.curve_balance_canvas,LV_STATE_CHECKED)&&balancePointEditActive){
        if(editedChannel>=MAX_MODEL_CHANNELS) return;
        ModelConfig &model=modelManager.getActiveModel();
        BalanceConfig &balance=model.outputs[editedChannel].balance;
        if(!balance.enabled||activeBalancePoint>=balance.pointCount||activeBalancePoint>=MAX_BALANCE_POINTS) return;

        int16_t step=getBalanceEncoderStep(-1);
        int32_t value=(int32_t)balance.correction[activeBalancePoint]-step;
        balance.correction[activeBalancePoint]=(int16_t)constrain(value,-100L,100L);
        balancePointEditValue=balance.correction[activeBalancePoint]/10.0f;

        // Update data immediately; redraw is frame-limited so fast rotation
        // does not build up a backlog of expensive canvas redraws.
        requestBalanceGraphRedraw();
        return;
    }

    moveBalanceEncoderSelection(-1);
}

void balanceEncoderRight(){
    if(current_active_screen==SCREEN_ID_SCR_MIXES){
        mixEncoderRight();
        return;
    }

    if(objects.curve_balance_panel==NULL||lv_obj_has_flag(objects.curve_balance_panel,LV_OBJ_FLAG_HIDDEN)) return;

    if(balanceEncoderNavIndex==0&&objects.curve_balance_canvas!=NULL&&lv_obj_has_state(objects.curve_balance_canvas,LV_STATE_FOCUSED)&&lv_obj_has_state(objects.curve_balance_canvas,LV_STATE_CHECKED)&&balancePointEditActive){
        if(editedChannel>=MAX_MODEL_CHANNELS) return;
        ModelConfig &model=modelManager.getActiveModel();
        BalanceConfig &balance=model.outputs[editedChannel].balance;
        if(!balance.enabled||activeBalancePoint>=balance.pointCount||activeBalancePoint>=MAX_BALANCE_POINTS) return;

        int16_t step=getBalanceEncoderStep(1);
        int32_t value=(int32_t)balance.correction[activeBalancePoint]+step;
        balance.correction[activeBalancePoint]=(int16_t)constrain(value,-100L,100L);
        balancePointEditValue=balance.correction[activeBalancePoint]/10.0f;

        // Update data immediately; redraw is frame-limited so fast rotation
        // does not build up a backlog of expensive canvas redraws.
        requestBalanceGraphRedraw();
        return;
    }

    moveBalanceEncoderSelection(1);
}

void balanceEncoderPressed(){
    if(current_active_screen==SCREEN_ID_SCR_MIXES){
        mixEncoderPressed();
        return;
    }

    if(objects.curve_balance_panel==NULL||lv_obj_has_flag(objects.curve_balance_panel,LV_OBJ_FLAG_HIDDEN)) return;

    lv_obj_t *selected=getBalanceEncoderNavObject(balanceEncoderNavIndex);
    if(selected==nullptr) return;

    // Canvas, Position, Magnet и Lock работают как переключатели.
    if(balanceEncoderNavIndex<=3){
        bool checked=lv_obj_has_state(selected,LV_STATE_CHECKED);

        if(checked)
            lv_obj_remove_state(selected,LV_STATE_CHECKED);
        else
            lv_obj_add_state(selected,LV_STATE_CHECKED);
    }

    if(balanceEncoderNavIndex==0){
        balanceCanvasEditMode=lv_obj_has_state(objects.curve_balance_canvas,LV_STATE_CHECKED);

        // First encoder step after entering the graph must be immediate.
        balanceEncoderLastStepMs=0;
        balanceEncoderLastDirection=0;
        balanceGraphLastRedrawMs=0;
        balanceGraphRedrawPending=false;

        if(balanceCanvasEditMode){
            if(!balancePointEditActive)
                selectNearestBalancePointToCurrentX();
        }else{
            balancePointEditActive=false;
        }

        drawChannelBalanceGraph(editedChannel);
        updateBalanceStickPosition();
        return;
    }

    // Settings и About — обычные кнопки действия.
    // Они не получают LV_STATE_CHECKED от энкодера.
    lv_obj_send_event(selected,LV_EVENT_CLICKED,nullptr);

    if(balanceEncoderNavIndex>=4)
        lv_obj_remove_state(selected,LV_STATE_CHECKED);

    applyBalanceEncoderSelection();
}

static void updateBalancePointChangerUi(){
    if(!balancePointEditActive||objects.ch_edit_changer_panel==NULL) return;
    const StepConfig *cfg=&STEPS_LIMIT;
    if(objects.ch_edit_changer_panel__dropdown_step!=NULL){
        const char *current=lv_dropdown_get_options(objects.ch_edit_changer_panel__dropdown_step);
        if(current==NULL||strcmp(current,cfg->options)!=0)
            lv_dropdown_set_options(objects.ch_edit_changer_panel__dropdown_step,cfg->options);
        uint32_t selected=last_selected_indices[STEP_GROUP_LIMIT];
        if(selected>cfg->max_idx) selected=cfg->max_idx;
        lv_dropdown_set_selected(objects.ch_edit_changer_panel__dropdown_step,selected);
        if(objects.ch_edit_changer_panel__reduce_step!=NULL){
            if(selected==0) lv_obj_add_state(objects.ch_edit_changer_panel__reduce_step,LV_STATE_DISABLED);
            else lv_obj_remove_state(objects.ch_edit_changer_panel__reduce_step,LV_STATE_DISABLED);
        }
        if(objects.ch_edit_changer_panel__increase_step!=NULL){
            if(selected>=cfg->max_idx) lv_obj_add_state(objects.ch_edit_changer_panel__increase_step,LV_STATE_DISABLED);
            else lv_obj_remove_state(objects.ch_edit_changer_panel__increase_step,LV_STATE_DISABLED);
        }
    }
    if(objects.ch_edit_changer_panel__mcmsusd_slider!=NULL){
        lv_slider_set_range(objects.ch_edit_changer_panel__mcmsusd_slider,-100,100);
        lv_slider_set_value(objects.ch_edit_changer_panel__mcmsusd_slider,(int32_t)lroundf(balancePointEditValue*10.0f),LV_ANIM_OFF);
    }
}
static void balancePointChangeStep(lv_event_t *e){
    if(e==NULL||objects.ch_edit_changer_panel__dropdown_step==NULL) return;
    const StepConfig *cfg=&STEPS_LIMIT;
    uintptr_t direction=(uintptr_t)lv_event_get_user_data(e);
    uint32_t selected=lv_dropdown_get_selected(objects.ch_edit_changer_panel__dropdown_step);
    if(direction==0){
        if(selected>0) selected--;
    }else if(direction==1){
        if(selected<cfg->max_idx) selected++;
    }else return;
    lv_dropdown_set_selected(objects.ch_edit_changer_panel__dropdown_step,selected);
    last_selected_indices[STEP_GROUP_LIMIT]=selected;
    updateBalancePointChangerUi();
}
static void balancePointDoStep(lv_event_t *e){
    if(e==NULL||editedChannel>=MAX_MODEL_CHANNELS) return;
    ModelConfig &model=modelManager.getActiveModel();
    BalanceConfig &balance=model.outputs[editedChannel].balance;
    if(!balance.enabled||activeBalancePoint>=balance.pointCount||activeBalancePoint>=MAX_BALANCE_POINTS) return;
    const StepConfig *cfg=&STEPS_LIMIT;
    uintptr_t command=(uintptr_t)lv_event_get_user_data(e);
    float value=balance.correction[activeBalancePoint]/10.0f;
    if(command==2){
        if(objects.ch_edit_changer_panel__mcmsusd_slider==NULL) return;
        value=lv_slider_get_value(objects.ch_edit_changer_panel__mcmsusd_slider)/10.0f;
    }else{
        if(objects.ch_edit_changer_panel__dropdown_step==NULL) return;
        uint32_t selected=lv_dropdown_get_selected(objects.ch_edit_changer_panel__dropdown_step);
        if(selected>cfg->max_idx) selected=cfg->max_idx;
        float step=cfg->steps[selected];
        if(command==0) value-=step;
        else if(command==1) value+=step;
        else return;
    }
    if(value<-10.0f) value=-10.0f;
    if(value>10.0f) value=10.0f;
    balancePointEditValue=value;
    balance.correction[activeBalancePoint]=(int16_t)lroundf(value*10.0f);
    if(objects.ch_edit_changer_panel__mcmsusd_slider!=NULL&&command!=2)
        lv_slider_set_value(objects.ch_edit_changer_panel__mcmsusd_slider,(int32_t)lroundf(value*10.0f),LV_ANIM_OFF);
    drawChannelBalanceGraph(editedChannel);
    drawChannelBalanceMiniGraph(editedChannel);
}
static void balanceStickPositionClicked(lv_event_t *e){
    if(e==NULL||objects.stick_position_panel==NULL) return;

    balanceEncoderNavIndex=1;
    balanceAutoAnalogEnabled=lv_obj_has_state(objects.stick_position_panel,LV_STATE_CHECKED);
    balanceAutoAnalogCaptured=false;

    if(balanceAutoAnalogEnabled){
        for(uint8_t i=0;i<static_cast<uint8_t>(AnalogInput::COUNT);i++)
            balanceAutoAnalogPrevious[i]=inputManager.getAnalog(static_cast<AnalogInput>(i));
    }else{
        outputProcessor.setBalancePositionOverride(editedChannel,false,0);
    }

    applyBalanceEncoderSelection();
    updateBalanceStickPosition();
}
static void balanceMagnetClicked(lv_event_t *e){
    if(e==NULL||objects.magnet_panel==NULL) return;

    balanceEncoderNavIndex=2;
    balanceMagnetEnabled=lv_obj_has_state(objects.magnet_panel,LV_STATE_CHECKED);
    applyBalanceEncoderSelection();

    if(balanceMagnetEnabled)
        updateBalanceStickPosition();
}
static void balanceLockClicked(lv_event_t *e){
    if(e==NULL||objects.balance_editor_lock_panel==NULL) return;

    balanceEncoderNavIndex=3;
    bool lockEnabled=lv_obj_has_state(objects.balance_editor_lock_panel,LV_STATE_CHECKED);

    if(lockEnabled&&!mixer.isInputLocked()){
        balanceLockedXValue=getBalanceCurrentXValue();
        mixer.setInputLock(true);
    }else if(!lockEnabled&&mixer.isInputLocked()){
        mixer.setInputLock(false);
    }

    applyBalanceEncoderSelection();
    updateBalanceStickPosition();
}
static void updateBalanceStickPosition(){
    // Finish the last deferred encoder redraw even after the knob has stopped.
    serviceBalanceGraphRedraw();

    if(editedChannel>=MAX_MODEL_CHANNELS) return;
    if(objects.curve_balance_panel==NULL||objects.curve_balance_canvas==NULL) return;
    if(lv_obj_has_flag(objects.curve_balance_panel,LV_OBJ_FLAG_HIDDEN)) return;
    constexpr int32_t W=780;
    constexpr int32_t H=292;
    ModelConfig &model=modelManager.getActiveModel();
    const BalanceConfig &balance=model.outputs[editedChannel].balance;
    if(!balance.enabled) return;
    if(balanceStickCursorLine==NULL){
        balanceStickCursorLine=lv_obj_create(objects.curve_balance_canvas);
        lv_obj_set_size(balanceStickCursorLine,1,H);
        lv_obj_set_style_bg_color(balanceStickCursorLine,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_TEXT]),LV_PART_MAIN);
        lv_obj_set_style_bg_opa(balanceStickCursorLine,LV_OPA_COVER,LV_PART_MAIN);
        lv_obj_set_style_border_width(balanceStickCursorLine,0,LV_PART_MAIN);
        lv_obj_set_style_pad_all(balanceStickCursorLine,0,LV_PART_MAIN);
        lv_obj_remove_flag(balanceStickCursorLine,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(balanceStickCursorLine,LV_OBJ_FLAG_SCROLLABLE);
    }
    if(balanceStickMarker==NULL){
        balanceStickMarker=lv_obj_create(objects.curve_balance_canvas);
        lv_obj_set_size(balanceStickMarker,10,10);
        lv_obj_set_style_bg_color(balanceStickMarker,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_TEXT]),LV_PART_MAIN);
        lv_obj_set_style_bg_opa(balanceStickMarker,LV_OPA_COVER,LV_PART_MAIN);
        lv_obj_set_style_border_width(balanceStickMarker,0,LV_PART_MAIN);
        lv_obj_set_style_radius(balanceStickMarker,LV_RADIUS_CIRCLE,LV_PART_MAIN);
        lv_obj_remove_flag(balanceStickMarker,LV_OBJ_FLAG_CLICKABLE);
    }
    if(balanceStickInfoLabel==NULL){
        balanceStickInfoLabel=lv_label_create(objects.curve_balance_canvas);
        lv_obj_set_style_bg_color(balanceStickInfoLabel,lv_color_hex(0xD7DCE1),LV_PART_MAIN);
        lv_obj_set_style_bg_opa(balanceStickInfoLabel,LV_OPA_COVER,LV_PART_MAIN);
        lv_obj_set_style_text_color(balanceStickInfoLabel,lv_color_hex(0x4D5963),LV_PART_MAIN);
        lv_obj_set_style_pad_left(balanceStickInfoLabel,3,LV_PART_MAIN);
        lv_obj_set_style_pad_right(balanceStickInfoLabel,3,LV_PART_MAIN);
        lv_obj_set_style_pad_top(balanceStickInfoLabel,2,LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(balanceStickInfoLabel,2,LV_PART_MAIN);
        lv_obj_remove_flag(balanceStickInfoLabel,LV_OBJ_FLAG_CLICKABLE);
    }
    int16_t xValue=getBalanceCurrentXValue();

    outputProcessor.setBalancePositionOverride(
        editedChannel,
        balanceAutoAnalogEnabled&&balanceAutoAnalogCaptured,
        xValue
    );

    int16_t displayValue=xValue;
    int16_t displayCorrection=balanceProcessor.getCorrection(displayValue,balance);
    displayCorrection=(int16_t)constrain((int32_t)displayCorrection,-100L,100L);

    int32_t displayX=((int32_t)displayValue+1000L)*(W-1)/2000L;
    if(displayX<0) displayX=0;
    if(displayX>=W) displayX=W-1;

    if(balanceMagnetEnabled&&objects.curve_balance_canvas!=NULL&&lv_obj_has_state(objects.curve_balance_canvas,LV_STATE_CHECKED)&&balance.pointCount>=2){
        uint8_t nearestPoint=0;
        int32_t nearestDistance=INT32_MAX;

        for(uint8_t i=0;i<balance.pointCount;i++){
            int32_t pointX=(int32_t)i*(W-1)/(balance.pointCount-1);
            int32_t distance=abs(displayX-pointX);

            if(distance<nearestDistance){
                nearestDistance=distance;
                nearestPoint=i;
            }
        }

        if(!balancePointEditActive||activeBalancePoint!=nearestPoint){
            activeBalancePoint=nearestPoint;
            balancePointEditActive=true;
            drawChannelBalanceGraph(editedChannel);
        }
    }

    constexpr int32_t ZERO_Y=(H/2)+1;
    int32_t displayY;
    if(displayCorrection>=0)
        displayY=ZERO_Y-((int32_t)displayCorrection*ZERO_Y)/100L;
    else
        displayY=ZERO_Y+((int32_t)(-displayCorrection)*(H-1-ZERO_Y))/100L;

    if(displayY<0) displayY=0;
    if(displayY>=H) displayY=H-1;

    lv_obj_set_style_bg_color(balanceStickCursorLine,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_TEXT]),LV_PART_MAIN);
    lv_obj_set_pos(balanceStickCursorLine,displayX,0);
    lv_obj_remove_flag(balanceStickCursorLine,LV_OBJ_FLAG_HIDDEN);

    char info[32];
    snprintf(info,sizeof(info),"X: %.1f%%\nΔ: %.1f%%",displayValue/10.0f,displayCorrection/10.0f);
    lv_label_set_text(balanceStickInfoLabel,info);

    int32_t labelX=displayX+8;
    if(labelX>W-90) labelX=displayX-82;
    labelX=constrain(labelX,0,W-82);

    int32_t labelY=displayY-34;
    if(labelY<0) labelY=displayY+8;
    labelY=constrain(labelY,0,H-42);

    lv_obj_set_pos(balanceStickInfoLabel,labelX,labelY);
    lv_obj_remove_flag(balanceStickInfoLabel,LV_OBJ_FLAG_HIDDEN);

    // Линия и точка всегда работают как единый курсор.
    // OFF: X берётся из обычного источника/микса редактируемого канала.
    // CHECKED: управляет последний реально сдвинутый analog input.
    lv_obj_set_pos(balanceStickMarker,displayX-5,displayY-5);
    lv_obj_remove_flag(balanceStickMarker,LV_OBJ_FLAG_HIDDEN);

    lv_obj_move_foreground(balanceStickCursorLine);
    lv_obj_move_foreground(balanceStickMarker);
    lv_obj_move_foreground(balanceStickInfoLabel);
}
static void drawChannelBalanceGraph(uint8_t ch){
    if(ch>=MAX_MODEL_CHANNELS||objects.curve_balance_canvas==NULL) return;
    ModelConfig &model=modelManager.getActiveModel();
    const BalanceConfig &balance=model.outputs[ch].balance;
    if(!balance.enabled) return;
    if(balanceGraphLine==NULL){
        balanceGraphLine=lv_line_create(objects.curve_balance_canvas);
        lv_obj_set_style_line_color(balanceGraphLine,lv_color_hex(0xFF0000),LV_PART_MAIN);
        lv_obj_set_style_line_width(balanceGraphLine,2,LV_PART_MAIN);
        lv_obj_remove_flag(balanceGraphLine,LV_OBJ_FLAG_CLICKABLE);
    }
    for(uint8_t i=0;i<MAX_BALANCE_POINTS;i++){
        if(balanceGraphPointMarkers[i]==NULL){
            balanceGraphPointMarkers[i]=lv_obj_create(objects.curve_balance_canvas);
            lv_obj_set_size(balanceGraphPointMarkers[i],10,10);
            lv_obj_set_style_radius(balanceGraphPointMarkers[i],LV_RADIUS_CIRCLE,LV_PART_MAIN);
            lv_obj_set_style_bg_color(balanceGraphPointMarkers[i],lv_color_hex(theme_colors[active_theme_index][COLOR_ID_TEXT]),LV_PART_MAIN);
            lv_obj_set_style_bg_opa(balanceGraphPointMarkers[i],LV_OPA_COVER,LV_PART_MAIN);
            lv_obj_set_style_border_width(balanceGraphPointMarkers[i],0,LV_PART_MAIN);
            lv_obj_set_style_pad_all(balanceGraphPointMarkers[i],0,LV_PART_MAIN);
            lv_obj_remove_flag(balanceGraphPointMarkers[i],LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(balanceGraphPointMarkers[i],LV_OBJ_FLAG_HIDDEN);
        }
        if(balanceGraphPointHitAreas[i]==NULL){
            balanceGraphPointHitAreas[i]=lv_obj_create(objects.curve_balance_canvas);
            lv_obj_set_size(balanceGraphPointHitAreas[i],30,30);
            lv_obj_set_style_bg_opa(balanceGraphPointHitAreas[i],LV_OPA_TRANSP,LV_PART_MAIN);
            lv_obj_set_style_border_width(balanceGraphPointHitAreas[i],0,LV_PART_MAIN);
            lv_obj_set_style_pad_all(balanceGraphPointHitAreas[i],0,LV_PART_MAIN);
            lv_obj_add_flag(balanceGraphPointHitAreas[i],LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(balanceGraphPointHitAreas[i],LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(balanceGraphPointHitAreas[i],balancePointClicked,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
            lv_obj_add_flag(balanceGraphPointHitAreas[i],LV_OBJ_FLAG_HIDDEN);
        }
    }
    constexpr int32_t W=780;
    constexpr int32_t H=292;
    constexpr int32_t ZERO_Y=(H/2)+1;

    // EEZ 0.0% axis is shifted down by 1 px.
    // First cover the old axis row with the canvas background, then draw the new one.
    if(balanceZeroLineErase==NULL){
        balanceZeroLineErase=lv_obj_create(objects.curve_balance_canvas);
        lv_obj_set_size(balanceZeroLineErase,W,1);
        lv_obj_set_style_border_width(balanceZeroLineErase,0,LV_PART_MAIN);
        lv_obj_set_style_pad_all(balanceZeroLineErase,0,LV_PART_MAIN);
        lv_obj_remove_flag(balanceZeroLineErase,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(balanceZeroLineErase,LV_OBJ_FLAG_SCROLLABLE);
    }
    if(balanceZeroLine==NULL){
        balanceZeroLine=lv_obj_create(objects.curve_balance_canvas);
        lv_obj_set_size(balanceZeroLine,W,1);
        lv_obj_set_style_border_width(balanceZeroLine,0,LV_PART_MAIN);
        lv_obj_set_style_pad_all(balanceZeroLine,0,LV_PART_MAIN);
        lv_obj_remove_flag(balanceZeroLine,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(balanceZeroLine,LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_color_t canvasBg=lv_obj_get_style_bg_color(objects.curve_balance_canvas,LV_PART_MAIN);
    lv_obj_set_style_bg_color(balanceZeroLineErase,canvasBg,LV_PART_MAIN);
    lv_obj_set_style_bg_opa(balanceZeroLineErase,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_pos(balanceZeroLineErase,0,H/2);
    lv_obj_set_style_bg_color(balanceZeroLine,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_TEXT]),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(balanceZeroLine,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_pos(balanceZeroLine,0,ZERO_Y);

    for(uint16_t i=0;i<BALANCE_GRAPH_DRAW_POINTS;i++){
        int16_t input=(int16_t)(-1000L+((int32_t)i*2000L)/(BALANCE_GRAPH_DRAW_POINTS-1));
        int16_t correction=balanceProcessor.getCorrection(input,balance);
        correction=(int16_t)constrain((int32_t)correction,-100L,100L);
        balanceGraphPoints[i].x=(int32_t)i*(W-1)/(BALANCE_GRAPH_DRAW_POINTS-1);
        if(correction>=0)
            balanceGraphPoints[i].y=ZERO_Y-((int32_t)correction*ZERO_Y)/100L;
        else
            balanceGraphPoints[i].y=ZERO_Y+((int32_t)(-correction)*(H-1-ZERO_Y))/100L;
    }
    lv_line_set_points(balanceGraphLine,balanceGraphPoints,BALANCE_GRAPH_DRAW_POINTS);
    lv_obj_set_pos(balanceGraphLine,0,0);
    uint8_t count=balance.pointCount;
    if(count>MAX_BALANCE_POINTS) count=MAX_BALANCE_POINTS;
    for(uint8_t i=0;i<MAX_BALANCE_POINTS;i++){
        lv_obj_add_flag(balanceGraphPointMarkers[i],LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(balanceGraphPointHitAreas[i],LV_OBJ_FLAG_HIDDEN);
    }
    if(count>=2){
        for(uint8_t i=0;i<count;i++){
            int16_t correction=(int16_t)constrain((int32_t)balance.correction[i],-100L,100L);
            int32_t x=(int32_t)i*(W-1)/(count-1);
            int32_t y;
            if(correction>=0)
                y=ZERO_Y-((int32_t)correction*ZERO_Y)/100L;
            else
                y=ZERO_Y+((int32_t)(-correction)*(H-1-ZERO_Y))/100L;

            bool active=balancePointEditActive&&i==activeBalancePoint;
            int32_t pointSize=active?12:10;
            int32_t pointHalf=pointSize/2;
            lv_obj_set_size(balanceGraphPointMarkers[i],pointSize,pointSize);
            lv_obj_set_pos(balanceGraphPointMarkers[i],x-pointHalf,y-pointHalf);

            int32_t hitX=constrain(x-15,0,W-30);
            int32_t hitY=constrain(y-15,0,H-30);
            lv_obj_set_pos(balanceGraphPointHitAreas[i],hitX,hitY);

            if(active)
                lv_obj_set_style_bg_color(balanceGraphPointMarkers[i],lv_color_hex(theme_colors[active_theme_index][COLOR_ID_MS_TEXT]),LV_PART_MAIN);
            else
                lv_obj_set_style_bg_color(balanceGraphPointMarkers[i],lv_color_hex(theme_colors[active_theme_index][COLOR_ID_TEXT]),LV_PART_MAIN);
            lv_obj_remove_flag(balanceGraphPointHitAreas[i],LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(balanceGraphPointMarkers[i],LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_obj_move_foreground(balanceGraphLine);
    for(uint8_t i=0;i<count;i++){
        lv_obj_move_foreground(balanceGraphPointHitAreas[i]);
        lv_obj_move_foreground(balanceGraphPointMarkers[i]);
    }
    if(balanceStickCursorLine!=NULL) lv_obj_move_foreground(balanceStickCursorLine);
    if(balanceStickMarker!=NULL) lv_obj_move_foreground(balanceStickMarker);
    if(balanceStickInfoLabel!=NULL) lv_obj_move_foreground(balanceStickInfoLabel);
}
static void drawChannelBalanceMiniGraph(uint8_t ch){
    if(ch>=MAX_MODEL_CHANNELS||objects.curve_balance_canvas_mini==NULL) return;
    ModelConfig &model=modelManager.getActiveModel();
    const BalanceConfig &balance=model.outputs[ch].balance;
    if(!balance.enabled) return;
    if(balanceMiniGraphLine==NULL){
        balanceMiniGraphLine=lv_line_create(objects.curve_balance_canvas_mini);
        lv_obj_set_style_line_color(balanceMiniGraphLine,lv_color_hex(0xFF0000),LV_PART_MAIN);
        lv_obj_set_style_line_width(balanceMiniGraphLine,1,LV_PART_MAIN);
        lv_obj_remove_flag(balanceMiniGraphLine,LV_OBJ_FLAG_CLICKABLE);
    }
    for(uint8_t i=0;i<MAX_BALANCE_POINTS;i++){
        if(balanceMiniGraphPointMarkers[i]==NULL){
            balanceMiniGraphPointMarkers[i]=lv_obj_create(objects.curve_balance_canvas_mini);
            lv_obj_set_size(balanceMiniGraphPointMarkers[i],4,4);
            lv_obj_set_style_radius(balanceMiniGraphPointMarkers[i],LV_RADIUS_CIRCLE,LV_PART_MAIN);
            lv_obj_set_style_bg_color(balanceMiniGraphPointMarkers[i],lv_color_hex(theme_colors[active_theme_index][COLOR_ID_TEXT]),LV_PART_MAIN);
            lv_obj_set_style_bg_opa(balanceMiniGraphPointMarkers[i],LV_OPA_COVER,LV_PART_MAIN);
            lv_obj_set_style_border_width(balanceMiniGraphPointMarkers[i],0,LV_PART_MAIN);
            lv_obj_set_style_pad_all(balanceMiniGraphPointMarkers[i],0,LV_PART_MAIN);
            lv_obj_remove_flag(balanceMiniGraphPointMarkers[i],LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(balanceMiniGraphPointMarkers[i],LV_OBJ_FLAG_HIDDEN);
        }
    }
    constexpr int32_t W=280;
    constexpr int32_t H=40;
    for(uint16_t i=0;i<BALANCE_MINI_DRAW_POINTS;i++){
        int16_t input=(int16_t)(-1000L+((int32_t)i*2000L)/(BALANCE_MINI_DRAW_POINTS-1));
        int16_t correction=balanceProcessor.getCorrection(input,balance);
        correction=(int16_t)constrain((int32_t)correction,-100L,100L);
        balanceMiniGraphPoints[i].x=(int32_t)i*(W-1)/(BALANCE_MINI_DRAW_POINTS-1);
        int32_t zeroY=(H/2)+1;
        if(correction>=0)
            balanceMiniGraphPoints[i].y=zeroY-((int32_t)correction*zeroY)/100L;
        else
            balanceMiniGraphPoints[i].y=zeroY+((int32_t)(-correction)*(H-1-zeroY))/100L;
    }
    lv_line_set_points(balanceMiniGraphLine,balanceMiniGraphPoints,BALANCE_MINI_DRAW_POINTS);
    lv_obj_set_pos(balanceMiniGraphLine,0,0);

    uint8_t count=balance.pointCount;
    if(count>MAX_BALANCE_POINTS) count=MAX_BALANCE_POINTS;

    for(uint8_t i=0;i<MAX_BALANCE_POINTS;i++)
        lv_obj_add_flag(balanceMiniGraphPointMarkers[i],LV_OBJ_FLAG_HIDDEN);

    if(count>=2){
        for(uint8_t i=0;i<count;i++){
            int16_t correction=(int16_t)constrain((int32_t)balance.correction[i],-100L,100L);
            int32_t x=(int32_t)i*(W-1)/(count-1);
            int32_t zeroY=(H/2)+1;
            int32_t y;
            if(correction>=0)
                y=zeroY-((int32_t)correction*zeroY)/100L;
            else
                y=zeroY+((int32_t)(-correction)*(H-1-zeroY))/100L;
            lv_obj_set_size(balanceMiniGraphPointMarkers[i],4,4);
            lv_obj_set_pos(balanceMiniGraphPointMarkers[i],x-2,y-2);
            lv_obj_set_style_bg_color(balanceMiniGraphPointMarkers[i],lv_color_hex(theme_colors[active_theme_index][COLOR_ID_TEXT]),LV_PART_MAIN);
            lv_obj_remove_flag(balanceMiniGraphPointMarkers[i],LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_obj_move_foreground(balanceMiniGraphLine);
    for(uint8_t i=0;i<count;i++)
        lv_obj_move_foreground(balanceMiniGraphPointMarkers[i]);

    // На mini graph намеренно НЕ рисуются:
    // balanceStickCursorLine, balanceStickMarker, balanceStickInfoLabel.
}

void action_channels_list_show(lv_event_t *e){
    uintptr_t userdata=(uintptr_t)lv_event_get_user_data(e);

    if(userdata==0){
        if(editedChannel>=MAX_MODEL_CHANNELS) return;

        ModelConfig &model=modelManager.getActiveModel();
        OutputConfig &output=model.outputs[editedChannel];

        char channelText[48];
        if(output.name[0]!='\0')
            snprintf(channelText,sizeof(channelText),"CH%u %s",(unsigned)(editedChannel+1),output.name);
        else
            snprintf(channelText,sizeof(channelText),"CH%u",(unsigned)(editedChannel+1));

        lv_obj_remove_flag(objects.channels_list,LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(objects.channels_list__channel_list_lbl,"Channels");

        lv_obj_t *list=objects.channels_list__channel_list_panel;
        if(list){
            if(dynamicBalanceChannelRow){lv_obj_delete(dynamicBalanceChannelRow);dynamicBalanceChannelRow=nullptr;}
            lv_obj_set_height(list,50);
            lv_obj_set_flex_flow(list,LV_FLEX_FLOW_COLUMN);
            lv_obj_set_style_pad_row(list,0,LV_PART_MAIN);
            dynamicBalanceChannelRow=createDynamicChannelRow(list,channelText,editedChannel,[](lv_event_t *ev){
                (void)ev;
                if(objects.channels_list) lv_obj_add_flag(objects.channels_list,LV_OBJ_FLAG_HIDDEN);
            });
        }
        hideChannelOkButton(objects.channels_list__ch_list_ok);
        return;
    }
    if(userdata==1){
        lv_obj_add_flag(objects.channels_list,LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if(userdata==2){
        loadBalanceSettingsUi();
        lv_obj_remove_flag(objects.balance_settings_panel,LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if(userdata==3){
        balanceSettingsCancel(e);
        return;
    }
}



// -----------------------------------------------------------------------------
// Vars - first UI/storage increment
// -----------------------------------------------------------------------------

enum class VarNumericField:uint8_t{
    NONE=0,
    RANGE_MIN,
    RANGE_MAX,
    VALUE_DEFAULT,
    VALUE_CONDITIONAL,
    ACTION_PARAMETER
};

static int8_t editingVarIndex=-1;
static VarNumericField activeVarNumericField=VarNumericField::NONE;
static uint8_t activeVarValueSlot=0;
static uint8_t activeVarActionSlot=0;
static int8_t editingVarConditionSlot=-1;
static bool editingVarConditionIsAction=false;
static int8_t editingVarFunctionActionIndex=-1;
static bool varConditionAutoDetectActive=false;
static int16_t varConditionSwitchBaseline[(uint8_t)SwitchInput::COUNT]={};
static bool varConditionFunctionBaseline[FUNCTION_SWITCH_COUNT]={};
static bool varConditionLogicBaseline[MAX_LOGICAL_SWITCHES]={};
static bool varConditionTrimBaseline[(uint8_t)TrimDirection::COUNT]={};
static uint8_t varNumericStepSelected=0;
static lv_obj_t *dynamicVarRows[MAX_MODEL_VARS]={};
static lv_obj_t *dynamicVarValueLabels[MAX_MODEL_VARS]={};
static int32_t lastRenderedVarValues[MAX_MODEL_VARS]={};
static int8_t lastRenderedEditorVar=-1;
static int32_t lastRenderedEditorValue=INT32_MIN;
static lv_obj_t *varsCallbacksBoundScreen=nullptr;
static int8_t varOptionsSlot=-1;
static lv_obj_t *varOptionSelectedPanel=nullptr;
static bool varOptionsIsAction=false;
static lv_obj_t *varOptionsLongPressTarget=nullptr;
static bool suppressVarConditionClick=false;
static uint32_t varOptionsLongPressTick=0;

static VarConfig *getEditingVar(){
    if(editingVarIndex<0||editingVarIndex>=MAX_MODEL_VARS) return nullptr;
    ModelConfig &model=modelManager.getActiveModel();
    VarConfig &var=model.vars[(uint8_t)editingVarIndex];
    if(!var.enabled) return nullptr;
    return &var;
}

static void ensureVarDefaultValue(VarConfig &var){
    if(var.valueCount>MAX_VAR_VALUES) var.valueCount=MAX_VAR_VALUES;
    if(var.valueCount==0){
        var.valueCount=1;
        var.values[0]=VarValueConfig{};
        var.values[0].enabled=true;
        var.values[0].condition.type=ConditionType::ALWAYS;
        var.values[0].value=0;
    }
    var.values[0].enabled=true;
    if(var.values[0].condition.type==ConditionType::NONE)
        var.values[0].condition.type=ConditionType::ALWAYS;
}

static int16_t getVarCurrentDisplayValue(const VarConfig &var){
    // The runtime accumulator, not the base Value, is displayed in Vars.
    ModelConfig &model=modelManager.getActiveModel();
    for(uint8_t i=0;i<MAX_MODEL_VARS;i++)
        if(&model.vars[i]==&var) return varProcessor.getValue(i);
    return varProcessor.evaluateBase(var);
}

static void formatVarPercent(lv_obj_t *label,int16_t value){
    if(!label) return;
    int32_t absValue=abs((int)value);
    lv_label_set_text_fmt(label,"%s%ld.%ld%%",value<0?"-":"",(long)(absValue/10),(long)(absValue%10));
}

static void formatVarRangeLabel(const VarConfig &var){
    if(!objects.vars_range_lbl) return;
    char minBuf[20],maxBuf[20],rangeBuf[48];
    int32_t a=abs((int)var.rangeMin),b=abs((int)var.rangeMax);
    snprintf(minBuf,sizeof(minBuf),"%s%ld.%ld%%",var.rangeMin<0?"-":"",(long)(a/10),(long)(a%10));
    snprintf(maxBuf,sizeof(maxBuf),"%s%ld.%ld%%",var.rangeMax<0?"-":"",(long)(b/10),(long)(b%10));
    snprintf(rangeBuf,sizeof(rangeBuf),"%s - %s",minBuf,maxBuf);
    lv_label_set_text(objects.vars_range_lbl,rangeBuf);
}

static void updateVarsListInfo(int8_t varIndex){
    ModelConfig &model=modelManager.getActiveModel();
    if(varIndex<0||varIndex>=MAX_MODEL_VARS||!model.vars[(uint8_t)varIndex].enabled){
        if(objects.vars_comment) lv_obj_add_flag(objects.vars_comment,LV_OBJ_FLAG_HIDDEN);
        if(objects.vars_comment_lbl) lv_obj_add_flag(objects.vars_comment_lbl,LV_OBJ_FLAG_HIDDEN);
        if(objects.vars_range) lv_obj_add_flag(objects.vars_range,LV_OBJ_FLAG_HIDDEN);
        if(objects.vars_range_lbl) lv_obj_add_flag(objects.vars_range_lbl,LV_OBJ_FLAG_HIDDEN);
        return;
    }

    VarConfig &var=model.vars[(uint8_t)varIndex];
    bool hasComment=var.comment[0]!='\0';

    if(hasComment){
        if(objects.vars_comment){
            lv_obj_remove_flag(objects.vars_comment,LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_y(objects.vars_comment,1);
        }
        if(objects.vars_comment_lbl){
            lv_obj_remove_flag(objects.vars_comment_lbl,LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_y(objects.vars_comment_lbl,17);
            lv_label_set_text(objects.vars_comment_lbl,var.comment);
        }
        if(objects.vars_range){
            lv_obj_remove_flag(objects.vars_range,LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_y(objects.vars_range,49);
        }
        if(objects.vars_range_lbl){
            lv_obj_remove_flag(objects.vars_range_lbl,LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_y(objects.vars_range_lbl,65);
        }
    }else{
        if(objects.vars_comment) lv_obj_add_flag(objects.vars_comment,LV_OBJ_FLAG_HIDDEN);
        if(objects.vars_comment_lbl) lv_obj_add_flag(objects.vars_comment_lbl,LV_OBJ_FLAG_HIDDEN);
        if(objects.vars_range){
            lv_obj_remove_flag(objects.vars_range,LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_y(objects.vars_range,1);
        }
        if(objects.vars_range_lbl){
            lv_obj_remove_flag(objects.vars_range_lbl,LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_y(objects.vars_range_lbl,17);
        }
    }

    formatVarRangeLabel(var);
}

static void updateVarSummaryUi(){
    VarConfig *var=getEditingVar();
    if(!var) return;

    const int16_t current=getVarCurrentDisplayValue(*var);
    formatVarPercent(objects.vars_value_lbl,current);
    lastRenderedEditorVar=editingVarIndex;
    lastRenderedEditorValue=current;
    if(selectedVarIndex==editingVarIndex) updateVarsListInfo(selectedVarIndex);
}


static lv_obj_t *getVarValueRow(uint8_t slot){
    if(slot>=10||slot==0) return nullptr;
    switch(slot){
        case 1:return objects.vars_value_1;
        case 2:return objects.vars_value_2;
        case 3:return objects.vars_value_3;
        case 4:return objects.vars_value_4;
        case 5:return objects.vars_value_5;
        case 6:return objects.vars_value_6;
        case 7:return objects.vars_value_7;
        case 8:return objects.vars_value_8;
        case 9:return objects.vars_value_9;
        default:return nullptr;
    }
}

static lv_obj_t *getVarValueConditionLabel(uint8_t slot){
    switch(slot){
        case 1:return objects.vars_value_1_condition_lbl;
        case 2:return objects.vars_value_2_condition_lbl;
        case 3:return objects.vars_value_3_condition_lbl;
        case 4:return objects.vars_value_4_condition_lbl;
        case 5:return objects.vars_value_5_condition_lbl;
        case 6:return objects.vars_value_6_condition_lbl;
        case 7:return objects.vars_value_7_condition_lbl;
        case 8:return objects.vars_value_8_condition_lbl;
        case 9:return objects.vars_value_9_condition_lbl;
        default:return nullptr;
    }
}

static lv_obj_t *getVarValuePanel(uint8_t slot){
    switch(slot){
        case 0:return objects.vars_value_0_value_panel;
        case 1:return objects.vars_value_1_value_panel;
        case 2:return objects.vars_value_2_value_panel;
        case 3:return objects.vars_value_3_value_panel;
        case 4:return objects.vars_value_4_value_panel;
        case 5:return objects.vars_value_5_value_panel;
        case 6:return objects.vars_value_6_value_panel;
        case 7:return objects.vars_value_7_value_panel;
        case 8:return objects.vars_value_8_value_panel;
        case 9:return objects.vars_value_9_value_panel;
        default:return nullptr;
    }
}

static lv_obj_t *getVarValueLabel(uint8_t slot){
    switch(slot){
        case 0:return objects.vars_value_0_value_lbl;
        case 1:return objects.vars_value_1_value_lbl;
        case 2:return objects.vars_value_2_value_lbl;
        case 3:return objects.vars_value_3_value_lbl;
        case 4:return objects.vars_value_4_value_lbl;
        case 5:return objects.vars_value_5_value_lbl;
        case 6:return objects.vars_value_6_value_lbl;
        case 7:return objects.vars_value_7_value_lbl;
        case 8:return objects.vars_value_8_value_lbl;
        case 9:return objects.vars_value_9_value_lbl;
        default:return nullptr;
    }
}

static void formatVarConditionLabel(const ConditionConfig &condition,char *buffer,size_t size){
    if(!buffer||size==0) return;
    if(condition.type==ConditionType::NONE||condition.type==ConditionType::NEVER){snprintf(buffer,size,"---");return;}
    if(condition.type==ConditionType::ALWAYS){snprintf(buffer,size,"Always on");return;}
    if(condition.type==ConditionType::FUNCTION_SWITCH){
        snprintf(buffer,size,"FS%u",(unsigned)(condition.value1+1));
        return;
    }
    if(condition.type==ConditionType::TRIM_POSITION){
        switch((TrimDirection)condition.value1){
            case TrimDirection::T4_LEFT:snprintf(buffer,size,"T4 Left");return;
            case TrimDirection::T4_RIGHT:snprintf(buffer,size,"T4 Right");return;
            case TrimDirection::T3_DOWN:snprintf(buffer,size,"T3 Down");return;
            case TrimDirection::T3_UP:snprintf(buffer,size,"T3 Up");return;
            case TrimDirection::T2_DOWN:snprintf(buffer,size,"T2 Down");return;
            case TrimDirection::T2_UP:snprintf(buffer,size,"T2 Up");return;
            case TrimDirection::T1_LEFT:snprintf(buffer,size,"T1 Left");return;
            case TrimDirection::T1_RIGHT:snprintf(buffer,size,"T1 Right");return;
            default:snprintf(buffer,size,"Trim");return;
        }
    }
    if(condition.type==ConditionType::SYSTEM_EVENT){
        static const char *names[]={
            "Throttle hold","Throttle cut","Throttle active","Telemetry active",
            "Rssi Low","Trainer active","Flight reset","Throttle in dead band"
        };
        uint8_t index=(uint8_t)condition.value1;
        if(index<sizeof(names)/sizeof(names[0])) snprintf(buffer,size,"%s",names[index]);
        else snprintf(buffer,size,"System event");
        return;
    }
    if(condition.type==ConditionType::SOURCE_POSITIVE&&condition.sourceA.type==SourceType::LOGICAL_SWITCH){
        snprintf(buffer,size,"LS%u",(unsigned)(condition.sourceA.index+1));
        return;
    }
    if(condition.type==ConditionType::SOURCE_EQUAL&&condition.sourceA.type==SourceType::SWITCH_INPUT){
        char switchName[48];
        formatMixSourceName(condition.sourceA,switchName,sizeof(switchName));
        const char *pos=condition.value1>0?"↑":(condition.value1<0?"↓":"-");
        snprintf(buffer,size,"%s%s",switchName,pos);
        return;
    }
    snprintf(buffer,size,"Condition");
}

// ETHOS-style condition markers: Edge (rising edge) and Invert.
// Keep these in the label, not in the model or runtime condition itself.
static void formatVarConditionWithOptions(const ConditionConfig &condition,bool edge,char *buffer,size_t size){
    if(!buffer||size==0) return;
    char name[64];
    formatVarConditionLabel(condition,name,sizeof(name));
    if(!condition.isValid()){
        snprintf(buffer,size,"%s",name);
        return;
    }
    snprintf(buffer,size,"%s%s%s",edge?"\xEE\xA4\x80":"",condition.inverted?"!":"",name);
}

static void updateVarValuesUi(){
    VarConfig *var=getEditingVar();
    if(!var) return;
    ensureVarDefaultValue(*var);
    if(var->valueCount>MAX_VAR_VALUES) var->valueCount=MAX_VAR_VALUES;

    for(uint8_t slot=1;slot<MAX_VAR_VALUES;slot++){
        lv_obj_t *row=getVarValueRow(slot);
        if(!row) continue;
        bool visible=slot<var->valueCount&&var->values[slot].enabled;
        if(!visible){lv_obj_add_flag(row,LV_OBJ_FLAG_HIDDEN);continue;}
        lv_obj_remove_flag(row,LV_OBJ_FLAG_HIDDEN);

        char conditionLabel[32];
        formatVarConditionWithOptions(var->values[slot].condition,false,conditionLabel,sizeof(conditionLabel));
        lv_obj_t *condition=getVarValueConditionLabel(slot);
        if(condition) setConditionText(condition,conditionLabel);
        formatVarPercent(getVarValueLabel(slot),varProcessor.getSlotValue((uint8_t)editingVarIndex,slot));
    }

    if(objects.vars_value_add_panel){
        if(var->valueCount>=MAX_VAR_VALUES) lv_obj_add_state(objects.vars_value_add_panel,LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.vars_value_add_panel,LV_STATE_DISABLED);
    }
}

static void clearVarConditionMembers(){
    if(!objects.vars_members_panel) return;
    while(lv_obj_get_child_count(objects.vars_members_panel)>1){
        lv_obj_t *child=lv_obj_get_child(objects.vars_members_panel,1);
        if(!child) break;
        lv_obj_delete(child);
    }
}

static void stopVarConditionAutoDetect(){
    varConditionAutoDetectActive=false;
}

static void rebaseVarConditionAutoDetect(){
    for(uint8_t i=0;i<(uint8_t)SwitchInput::COUNT;i++)
        varConditionSwitchBaseline[i]=inputManager.getSwitch((SwitchInput)i);
    for(uint8_t i=0;i<FUNCTION_SWITCH_COUNT;i++)
        varConditionFunctionBaseline[i]=functionSwitchManager.getState(i);
    for(uint8_t i=0;i<MAX_LOGICAL_SWITCHES;i++)
        varConditionLogicBaseline[i]=logicalSwitchManager.getState(i);
    for(uint8_t i=0;i<(uint8_t)TrimDirection::COUNT;i++)
        varConditionTrimBaseline[i]=trimSwitchManager.getState((TrimDirection)i);
}

static void startVarConditionAutoDetect(){
    rebaseVarConditionAutoDetect();
    varConditionAutoDetectActive=true;
}

static void closeVarConditionSelector(){
    stopVarConditionAutoDetect();
    if(objects.vars_active_condition_panel) lv_obj_add_flag(objects.vars_active_condition_panel,LV_OBJ_FLAG_HIDDEN);
    hidePopupDismissOverlay();
    editingVarConditionSlot=-1;
    editingVarConditionIsAction=false;
}

static void commitVarValueCondition(const ConditionConfig &condition){
    VarConfig *var=getEditingVar();
    if(!var||editingVarConditionSlot<0) return;
    if(editingVarConditionIsAction){
        uint8_t index=(uint8_t)editingVarConditionSlot;
        if(index>=var->actionCount||index>=MAX_VAR_ACTIONS) return;
        var->actions[index].condition=condition;
        updateVarActionsUi();
    }else{
        uint8_t slot=(uint8_t)editingVarConditionSlot;
        if(slot<1||slot>=var->valueCount||slot>=MAX_VAR_VALUES) return;
        var->values[slot].condition=condition;
        updateVarValuesUi();
    }
    modelManager.requestSaveActiveModel();
    closeVarConditionSelector();
    updateVarSummaryUi();
}

static lv_obj_t *createVarConditionMember(const char *text,lv_event_cb_t callback,uintptr_t userData){
    if(!objects.vars_members_panel||!text) return nullptr;
    lv_obj_t *row=lv_obj_create(objects.vars_members_panel);
    lv_obj_set_size(row,LV_PCT(95),40);
    lv_obj_set_style_pad_all(row,0,LV_PART_MAIN);
    lv_obj_set_style_border_width(row,0,LV_PART_MAIN);
    lv_obj_set_style_radius(row,0,LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row,255,LV_PART_MAIN);
    lv_obj_set_style_bg_color(row,lv_color_hex(theme_colors[active_theme_index][11]),LV_PART_MAIN);
    lv_obj_set_style_text_color(row,lv_color_hex(theme_colors[active_theme_index][16]),LV_PART_MAIN);
    lv_obj_set_style_bg_color(row,lv_color_hex(theme_colors[active_theme_index][3]),static_cast<lv_style_selector_t>(static_cast<uint32_t>(LV_PART_MAIN)|static_cast<uint32_t>(LV_STATE_FOCUSED)));
    lv_obj_set_style_text_color(row,lv_color_hex(theme_colors[active_theme_index][9]),static_cast<lv_style_selector_t>(static_cast<uint32_t>(LV_PART_MAIN)|static_cast<uint32_t>(LV_STATE_FOCUSED)));
    lv_obj_set_style_text_font(row,&lv_font_montserrat_20,LV_PART_MAIN);
    lv_obj_add_flag(row,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(row,LV_OBJ_FLAG_CHECKABLE);
    if(callback) lv_obj_add_event_cb(row,callback,LV_EVENT_CLICKED,(void *)userData);
    lv_obj_t *label=lv_label_create(row);
    setConditionText(label,text);
    lv_obj_center(label);
    return row;
}

static void varConditionSwitchMemberClicked(lv_event_t *e){
    if(!e) return;
    uint16_t packed=(uint16_t)(uintptr_t)lv_event_get_user_data(e);
    uint8_t switchRaw=(uint8_t)(packed>>8);
    uint8_t position=(uint8_t)(packed&0xff);
    if(switchRaw>=static_cast<uint8_t>(SwitchInput::COUNT)||position>2) return;
    ConditionConfig c{};
    c.type=ConditionType::SOURCE_EQUAL;
    c.sourceA=Sources::switchInput(static_cast<SwitchInput>(switchRaw));
    c.value1=position==0?-1000:(position==1?0:1000);
    c.tolerance=100;
    commitVarValueCondition(c);
}

static void buildVarSwitchMembers(){
    clearVarConditionMembers();
    for(uint8_t slot=0;slot<HARDWARE_SUPPORTED_SWITCHES;slot++){
        uint8_t type=hardwareSwitchType(slot);
        if(type==0) continue;
        char name[48];hardwareSwitchName(slot,name,sizeof(name));
        auto add=[&](const char *symbol,uint8_t pos){
            char label[64];snprintf(label,sizeof(label),"%s %s",name,symbol);
            uint16_t packed=((uint16_t)static_cast<uint8_t>(hardwareSwitchInput(slot))<<8)|pos;
            createVarConditionMember(label,varConditionSwitchMemberClicked,packed);
        };
        add("↑",0);
        if(type==2) add("-",1);
        add("↓",2);
    }
}

static void varConditionFunctionSwitchClicked(lv_event_t *e){
    if(!e) return;uint8_t index=(uint8_t)(uintptr_t)lv_event_get_user_data(e);if(index>=FUNCTION_SWITCH_COUNT) return;
    ConditionConfig c{};c.type=ConditionType::FUNCTION_SWITCH;c.value1=index;commitVarValueCondition(c);
}
static void buildVarFunctionSwitchMembers(){clearVarConditionMembers();for(uint8_t i=0;i<FUNCTION_SWITCH_COUNT;i++){char t[8];snprintf(t,sizeof(t),"FS%u",(unsigned)(i+1));createVarConditionMember(t,varConditionFunctionSwitchClicked,i);}}

static void varConditionLogicSwitchClicked(lv_event_t *e){
    if(!e) return;uint8_t index=(uint8_t)(uintptr_t)lv_event_get_user_data(e);if(index>=MAX_LOGICAL_SWITCHES) return;
    ModelConfig &model=modelManager.getActiveModel();if(!model.logicalSwitches[index].enabled) return;
    ConditionConfig c{};c.type=ConditionType::SOURCE_POSITIVE;c.sourceA=Sources::logicalSwitch(index);commitVarValueCondition(c);
}
static void buildVarLogicSwitchMembers(){
    clearVarConditionMembers();ModelConfig &model=modelManager.getActiveModel();
    for(uint8_t i=0;i<MAX_LOGICAL_SWITCHES;i++) if(model.logicalSwitches[i].enabled){char t[8];snprintf(t,sizeof(t),"LS%u",(unsigned)(i+1));createVarConditionMember(t,varConditionLogicSwitchClicked,i);}
}

static void varConditionTrimClicked(lv_event_t *e){
    if(!e) return;uint8_t direction=(uint8_t)(uintptr_t)lv_event_get_user_data(e);if(direction>=(uint8_t)TrimDirection::COUNT) return;
    ConditionConfig c{};c.type=ConditionType::TRIM_POSITION;c.value1=direction;commitVarValueCondition(c);
}
static void buildVarTrimMembers(){
    clearVarConditionMembers();
    struct Entry{const char *label;TrimDirection direction;};
    static const Entry entries[]={
        {"T4 Left",TrimDirection::T4_LEFT},{"T4 Right",TrimDirection::T4_RIGHT},{"T3 Down",TrimDirection::T3_DOWN},{"T3 Up",TrimDirection::T3_UP},
        {"T2 Down",TrimDirection::T2_DOWN},{"T2 Up",TrimDirection::T2_UP},{"T1 Left",TrimDirection::T1_LEFT},{"T1 Right",TrimDirection::T1_RIGHT}
    };
    for(const Entry &entry:entries) createVarConditionMember(entry.label,varConditionTrimClicked,(uintptr_t)entry.direction);
}

static void varConditionSystemEventClicked(lv_event_t *e){
    if(!e) return;uint8_t id=(uint8_t)(uintptr_t)lv_event_get_user_data(e);if(id>=(uint8_t)SystemEventId::COUNT) return;
    ConditionConfig c{};c.type=ConditionType::SYSTEM_EVENT;c.value1=id;commitVarValueCondition(c);
}
static void buildVarSystemEventMembers(){
    clearVarConditionMembers();
    struct Entry{const char *label;SystemEventId event;};
    static const Entry entries[]={
        {"Throttle hold",SystemEventId::THROTTLE_HOLD},{"Throttle cut",SystemEventId::THROTTLE_CUT},{"Throttle active",SystemEventId::THROTTLE_ACTIVE},
        {"Telemetry active",SystemEventId::TELEMETRY_ACTIVE},{"Rssi Low",SystemEventId::RSSI_LOW},{"Trainer active",SystemEventId::TRAINER_ACTIVE},
        {"Flight reset",SystemEventId::FLIGHT_RESET},{"Throttle in dead band",SystemEventId::THROTTLE_IN_DEAD_BAND}
    };
    for(const Entry &entry:entries) createVarConditionMember(entry.label,varConditionSystemEventClicked,(uintptr_t)entry.event);
}

static void focusVarConditionCategory(lv_obj_t *target){
    lv_obj_t *rows[]={
        objects.vars_category_none,objects.vars_category_analog,objects.vars_category_direction,objects.vars_category_sp,
        objects.vars_category_fs,objects.vars_category_ls,objects.vars_category_tp,objects.vars_category_gyro,objects.vars_category_se
    };
    for(lv_obj_t *row:rows) if(row) lv_obj_remove_state(row,LV_STATE_FOCUSED);
    if(target) lv_obj_add_state(target,LV_STATE_FOCUSED);
}

static void focusVarConditionMemberByText(const char *text){
    if(!objects.vars_members_panel||!text) return;
    uint32_t count=lv_obj_get_child_count(objects.vars_members_panel);
    for(uint32_t i=1;i<count;i++){
        lv_obj_t *row=lv_obj_get_child(objects.vars_members_panel,(int32_t)i);
        if(!row) continue;
        lv_obj_remove_state(row,LV_STATE_FOCUSED);
        if(lv_obj_get_child_count(row)==0) continue;
        lv_obj_t *label=lv_obj_get_child(row,0);
        if(label&&strcmp(lv_label_get_text(label),text)==0){
            lv_obj_add_state(row,LV_STATE_FOCUSED);
            lv_obj_scroll_to_view(row,LV_ANIM_OFF);
        }
    }
}

static const char *varTrimDirectionLabel(TrimDirection direction){
    switch(direction){
        case TrimDirection::T4_LEFT:return "T4 Left";
        case TrimDirection::T4_RIGHT:return "T4 Right";
        case TrimDirection::T3_DOWN:return "T3 Down";
        case TrimDirection::T3_UP:return "T3 Up";
        case TrimDirection::T2_DOWN:return "T2 Down";
        case TrimDirection::T2_UP:return "T2 Up";
        case TrimDirection::T1_LEFT:return "T1 Left";
        case TrimDirection::T1_RIGHT:return "T1 Right";
        default:return nullptr;
    }
}

static void updateVarConditionAutoDetect(){
    if(!varConditionAutoDetectActive) return;
    if(editingVarConditionSlot<1||editingVarConditionSlot>=MAX_VAR_VALUES){stopVarConditionAutoDetect();return;}
    if(!objects.vars_active_condition_panel||lv_obj_has_flag(objects.vars_active_condition_panel,LV_OBJ_FLAG_HIDDEN)){stopVarConditionAutoDetect();return;}

    // Switch names and supported positions come from Hardware configuration.
    for(uint8_t slot=0;slot<HARDWARE_SUPPORTED_SWITCHES;slot++){
        uint8_t type=hardwareSwitchType(slot);
        if(type==0) continue;
        SwitchInput input=hardwareSwitchInput(slot);
        uint8_t raw=(uint8_t)input;
        int16_t current=inputManager.getSwitch(input);
        if(current==varConditionSwitchBaseline[raw]) continue;
        buildVarSwitchMembers();
        focusVarConditionCategory(objects.vars_category_sp);
        char name[48],text[64];hardwareSwitchName(slot,name,sizeof(name));
        const char *symbol=current< -500?"↑":(current>500?"↓":"-");
        if(type!=2&&current>=-500&&current<=500) {
            rebaseVarConditionAutoDetect();
            continue;
        }
        snprintf(text,sizeof(text),"%s %s",name,symbol);
        focusVarConditionMemberByText(text);
        rebaseVarConditionAutoDetect();
        return;
    }

    for(uint8_t i=0;i<FUNCTION_SWITCH_COUNT;i++){
        bool current=functionSwitchManager.getState(i);
        if(current==varConditionFunctionBaseline[i]) continue;
        buildVarFunctionSwitchMembers();
        focusVarConditionCategory(objects.vars_category_fs);
        char text[8];snprintf(text,sizeof(text),"FS%u",(unsigned)(i+1));
        focusVarConditionMemberByText(text);
        rebaseVarConditionAutoDetect();
        return;
    }

    // For trims, react to the press edge only. Release should not move focus away.
    for(uint8_t i=0;i<(uint8_t)TrimDirection::COUNT;i++){
        bool current=trimSwitchManager.getState((TrimDirection)i);
        bool previous=varConditionTrimBaseline[i];
        if(current==previous) continue;
        if(current){
            buildVarTrimMembers();
            focusVarConditionCategory(objects.vars_category_tp);
            const char *label=varTrimDirectionLabel((TrimDirection)i);
            if(label) focusVarConditionMemberByText(label);
            rebaseVarConditionAutoDetect();
            return;
        }
    }

    ModelConfig &model=modelManager.getActiveModel();
    for(uint8_t i=0;i<MAX_LOGICAL_SWITCHES;i++){
        if(!model.logicalSwitches[i].enabled) continue;
        bool current=logicalSwitchManager.getState(i);
        if(current==varConditionLogicBaseline[i]) continue;
        buildVarLogicSwitchMembers();
        focusVarConditionCategory(objects.vars_category_ls);
        char text[8];snprintf(text,sizeof(text),"LS%u",(unsigned)(i+1));
        focusVarConditionMemberByText(text);
        rebaseVarConditionAutoDetect();
        return;
    }

    // Keep baselines synchronized for trim release and inactive entries.
    rebaseVarConditionAutoDetect();
}

// Direction members are UI placeholders until Vars direction semantics are defined.
// Selection only highlights one member; it does not alter or persist conditions.
static void varConditionDirectionPlaceholderClicked(lv_event_t *e){
    if(!e) return;
    lv_obj_t *row=(lv_obj_t *)lv_event_get_target(e);
    if(!row||lv_obj_get_child_count(row)==0) return;
    lv_obj_t *label=lv_obj_get_child(row,0);
    if(label) focusVarConditionMemberByText(lv_label_get_text(label));
}

static void buildVarDirectionPlaceholderMembers(){
    clearVarConditionMembers();
    static const char *labels[]={"right","left","Top","Bottom"};
    for(const char *label:labels)
        createVarConditionMember(label,varConditionDirectionPlaceholderClicked,0);
}

static void varConditionCategoryClicked(lv_event_t *e){
    if(!e||editingVarConditionSlot<0) return;
    uint8_t category=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(category==0){ConditionConfig c{};c.type=ConditionType::NEVER;commitVarValueCondition(c);return;}
    if(category==1){ConditionConfig c{};c.type=ConditionType::ALWAYS;commitVarValueCondition(c);return;}
    if(category==2){buildVarDirectionPlaceholderMembers();return;}
    if(category==3){buildVarSwitchMembers();return;}
    if(category==4){buildVarFunctionSwitchMembers();return;}
    if(category==5){buildVarLogicSwitchMembers();return;}
    if(category==6){buildVarTrimMembers();return;}
    if(category==8){buildVarSystemEventMembers();return;}
    // Gyro switches are not backed by ConditionType or a switch registry yet.
    // Show a non-selectable placeholder; do not create a false saved condition.
    clearVarConditionMembers();
    lv_obj_t *placeholder=createVarConditionMember("Gyro switches unavailable",nullptr,0);
    if(placeholder){
        lv_obj_remove_flag(placeholder,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(placeholder,LV_OBJ_FLAG_CHECKABLE);
    }
}

static void setupVarConditionCategoryCallbacks(){
    struct Bind{lv_obj_t *obj;uint8_t category;};
    Bind binds[]={
        {objects.vars_category_none,0},{objects.vars_category_analog,1},{objects.vars_category_direction,2},{objects.vars_category_sp,3},
        {objects.vars_category_fs,4},{objects.vars_category_ls,5},{objects.vars_category_tp,6},{objects.vars_category_gyro,7},{objects.vars_category_se,8}
    };
    for(const Bind &bind:binds){
        if(!bind.obj) continue;
        lv_obj_add_flag(bind.obj,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_event_cb(bind.obj,varConditionCategoryClicked);
        lv_obj_add_event_cb(bind.obj,varConditionCategoryClicked,LV_EVENT_CLICKED,(void *)(uintptr_t)bind.category);
    }
    if(objects.vars_category_analog&&lv_obj_get_child_count(objects.vars_category_analog)>0){
        lv_obj_t *label=lv_obj_get_child(objects.vars_category_analog,0);if(label) lv_label_set_text(label,"Always on");
    }
    if(objects.vars_category_gyro&&lv_obj_get_child_count(objects.vars_category_gyro)>0){
        lv_obj_t *label=lv_obj_get_child(objects.vars_category_gyro,0);if(label) lv_label_set_text(label,"Gyro switches");
    }
}

static lv_obj_t *getVarActionRow(uint8_t index){
    switch(index){
        case 0:return objects.vars_actions_1;case 1:return objects.vars_actions_2;case 2:return objects.vars_actions_3;case 3:return objects.vars_actions_4;case 4:return objects.vars_actions_5;
        case 5:return objects.vars_actions_6;case 6:return objects.vars_actions_7;case 7:return objects.vars_actions_8;case 8:return objects.vars_actions_9;case 9:return objects.vars_actions_10;
        default:return nullptr;
    }
}
static lv_obj_t *getVarActionConditionLabel(uint8_t index){
    switch(index){
        case 0:return objects.vars_actions_1_condition_lbl;case 1:return objects.vars_actions_2_condition_lbl;case 2:return objects.vars_actions_3_condition_lbl;case 3:return objects.vars_actions_4_condition_lbl;case 4:return objects.vars_actions_5_condition_lbl;
        case 5:return objects.vars_actions_6_condition_lbl;case 6:return objects.vars_actions_7_condition_lbl;case 7:return objects.vars_actions_8_condition_lbl;case 8:return objects.vars_actions_9_condition_lbl;case 9:return objects.vars_actions_10_condition_lbl;
        default:return nullptr;
    }
}
static lv_obj_t *getVarActionFunctionPanel(uint8_t index){
    switch(index){
        case 0:return objects.vars_actions_1_function_panel;case 1:return objects.vars_actions_2_function_panel;case 2:return objects.vars_actions_3_function_panel;case 3:return objects.vars_actions_4_function_panel;case 4:return objects.vars_actions_5_function_panel;
        case 5:return objects.vars_actions_6_function_panel;case 6:return objects.vars_actions_7_function_panel;case 7:return objects.vars_actions_8_function_panel;case 8:return objects.vars_actions_9_function_panel;case 9:return objects.vars_actions_10_function_panel;
        default:return nullptr;
    }
}
static lv_obj_t *getVarActionFunctionLabel(uint8_t index){
    switch(index){
        case 0:return objects.vars_actions_1_function_lbl;case 1:return objects.vars_actions_2_function_lbl;case 2:return objects.vars_actions_3_function_lbl;case 3:return objects.vars_actions_4_function_lbl;case 4:return objects.vars_actions_5_function_lbl;
        case 5:return objects.vars_actions_6_function_lbl;case 6:return objects.vars_actions_7_function_lbl;case 7:return objects.vars_actions_8_function_lbl;case 8:return objects.vars_actions_9_function_lbl;case 9:return objects.vars_actions_10_function_lbl;
        default:return nullptr;
    }
}
static lv_obj_t *getVarActionTrimPanel(uint8_t index){
    switch(index){
        case 0:return objects.vars_trim_panel_1;case 1:return objects.vars_trim_panel_2;case 2:return objects.vars_trim_panel_3;case 3:return objects.vars_trim_panel_4;case 4:return objects.vars_trim_panel_5;
        case 5:return objects.vars_trim_panel_6;case 6:return objects.vars_trim_panel_7;case 7:return objects.vars_trim_panel_8;case 8:return objects.vars_trim_panel_9;case 9:return objects.vars_trim_panel_10;
        default:return nullptr;
    }
}
static lv_obj_t *getVarActionTrimLabel(uint8_t index){
    switch(index){
        case 0:return objects.vars_trim_lbl_1;case 1:return objects.vars_trim_lbl_2;case 2:return objects.vars_trim_lbl_3;case 3:return objects.vars_trim_lbl_4;case 4:return objects.vars_trim_lbl_5;
        case 5:return objects.vars_trim_lbl_6;case 6:return objects.vars_trim_lbl_7;case 7:return objects.vars_trim_lbl_8;case 8:return objects.vars_trim_lbl_9;case 9:return objects.vars_trim_lbl_10;
        default:return nullptr;
    }
}
static lv_obj_t *getVarActionValuePanel(uint8_t index){
    switch(index){
        case 0:return objects.vars_actions_1_value_panel;case 1:return objects.vars_actions_2_value_panel;case 2:return objects.vars_actions_3_value_panel;case 3:return objects.vars_actions_4_value_panel;case 4:return objects.vars_actions_5_value_panel;
        case 5:return objects.vars_actions_6_value_panel;case 6:return objects.vars_actions_7_value_panel;case 7:return objects.vars_actions_8_value_panel;case 8:return objects.vars_actions_9_value_panel;case 9:return objects.vars_actions_10_value_panel;
        default:return nullptr;
    }
}
static lv_obj_t *getVarActionValueLabel(uint8_t index){
    switch(index){
        case 0:return objects.vars_actions_1_value_lbl;case 1:return objects.vars_actions_2_value_lbl;case 2:return objects.vars_actions_3_value_lbl;case 3:return objects.vars_actions_4_value_lbl;case 4:return objects.vars_actions_5_value_lbl;
        case 5:return objects.vars_actions_6_value_lbl;case 6:return objects.vars_actions_7_value_lbl;case 7:return objects.vars_actions_8_value_lbl;case 8:return objects.vars_actions_9_value_lbl;case 9:return objects.vars_actions_10_value_lbl;
        default:return nullptr;
    }
}
static const char *varActionFunctionName(VarActionFunction function){
    switch(function){
        case VarActionFunction::ASSIGN:return "Assign(=)";
        case VarActionFunction::ADD:return "Add(+)";
        case VarActionFunction::SUBTRACT:return "Subtract(-)";
        case VarActionFunction::MULTIPLY:return "Multiply(*)";
        case VarActionFunction::DIVIDE:return "Divide(/)";
        case VarActionFunction::PERCENT:return "Percent";
        case VarActionFunction::MINIMUM:return "Min";
        case VarActionFunction::MAXIMUM:return "Max";
        case VarActionFunction::REPURPOSE:return "Repurpose";
        default:return "Assign(=)";
    }
}
static const char *varTrimName(int8_t index){
    static const char *names[6]={"Trim Rudder","Trim Elevator","Trim Throttle","Trim Aileron","T5","T6"};
    if(index<0||index>=6) return "---";
    return names[index];
}
static void formatVarActionParameter(lv_obj_t *label,int16_t value){
    if(!label) return;
    int32_t a=abs((int)value);
    lv_label_set_text_fmt(label,"%s%ld.%ld",value<0?"-":"",(long)(a/10),(long)(a%10));
}
static void updateVarActionsUi(){
    VarConfig *var=getEditingVar();
    if(!var) return;
    if(var->actionCount>MAX_VAR_ACTIONS) var->actionCount=MAX_VAR_ACTIONS;
    for(uint8_t i=0;i<MAX_VAR_ACTIONS;i++){
        lv_obj_t *row=getVarActionRow(i);
        if(!row) continue;
        bool visible=i<var->actionCount&&var->actions[i].enabled;
        if(!visible){lv_obj_add_flag(row,LV_OBJ_FLAG_HIDDEN);continue;}
        lv_obj_remove_flag(row,LV_OBJ_FLAG_HIDDEN);
        VarActionConfig &action=var->actions[i];
        char condition[32];
        formatVarConditionWithOptions(action.condition,action.edge,condition,sizeof(condition));
        if(getVarActionConditionLabel(i)) setConditionText(getVarActionConditionLabel(i),condition);
        if(getVarActionFunctionLabel(i)) lv_label_set_text(getVarActionFunctionLabel(i),varActionFunctionName(action.function));
        bool repurpose=action.function==VarActionFunction::REPURPOSE;
        lv_obj_t *functionPanel=getVarActionFunctionPanel(i);
        lv_obj_t *trimPanel=getVarActionTrimPanel(i);
        lv_obj_t *valuePanel=getVarActionValuePanel(i);
        if(functionPanel) lv_obj_remove_flag(functionPanel,LV_OBJ_FLAG_HIDDEN);
        if(trimPanel){if(repurpose) lv_obj_remove_flag(trimPanel,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(trimPanel,LV_OBJ_FLAG_HIDDEN);}
        if(valuePanel) lv_obj_remove_flag(valuePanel,LV_OBJ_FLAG_HIDDEN);
        if(repurpose){if(getVarActionTrimLabel(i)) lv_label_set_text(getVarActionTrimLabel(i),varTrimName(action.repurposeTrimIndex));}
        else formatVarActionParameter(getVarActionValueLabel(i),action.parameter);
    }
    if(objects.vars_actions_add){
        if(var->actionCount>=MAX_VAR_ACTIONS) lv_obj_add_state(objects.vars_actions_add,LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.vars_actions_add,LV_STATE_DISABLED);
    }
}

static void updateVarEditorUi(){
    VarConfig *var=getEditingVar();
    if(!var) return;
    ensureVarDefaultValue(*var);

    if(objects.vars_name) lv_textarea_set_text(objects.vars_name,var->name);
    if(objects.vars_edit_comment) lv_textarea_set_text(objects.vars_edit_comment,var->comment);
    formatVarPercent(objects.vars_range_from_lbl,var->rangeMin);
    formatVarPercent(objects.vars_range_to_lbl,var->rangeMax);
    formatVarPercent(objects.vars_value_0_value_lbl,varProcessor.getSlotValue((uint8_t)editingVarIndex,0));
    updateVarSummaryUi();

    updateVarValuesUi();

    updateVarActionsUi();
}

static void saveVarEditorText(){
    VarConfig *var=getEditingVar();
    if(!var) return;

    bool changed=false;
    if(objects.vars_name){
        const char *text=lv_textarea_get_text(objects.vars_name);
        if(text&&strncmp(var->name,text,sizeof(var->name))!=0){
            changed=true;
            strncpy(var->name,text,sizeof(var->name)-1);
            var->name[sizeof(var->name)-1]='\0';
        }
    }
    if(objects.vars_edit_comment){
        const char *text=lv_textarea_get_text(objects.vars_edit_comment);
        if(text&&strncmp(var->comment,text,sizeof(var->comment))!=0){
            changed=true;
            strncpy(var->comment,text,sizeof(var->comment)-1);
            var->comment[sizeof(var->comment)-1]='\0';
        }
    }
    updateVarSummaryUi();
    if(changed) modelManager.requestSaveActiveModel();
}

static void varTextEditorEvent(lv_event_t *e){
    if(!e) return;
    if(lv_event_get_code(e)==LV_EVENT_DEFOCUSED) saveVarEditorText();
}

static void hideVarPopup(){
    if(objects.curve_popup) lv_obj_add_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN);
    hidePopupDismissOverlay();
}

// Vars have stable numeric SourceId indices. Move changes display order only.
static uint8_t varDisplayOrder[MAX_MODEL_VARS]={};
static uint16_t varOrderModelId=UINT16_MAX;
static bool varMoveMode=false;
static int8_t varMoveFrom=-1;
static bool varClipboardValid=false;
static VarConfig varClipboard={};
static int8_t varPendingDelete=-1;
static int8_t varPendingPaste=-1;

static void loadVarDisplayOrder(){
    ModelConfig &model=modelManager.getActiveModel();
    if(varOrderModelId==model.modelId) return;
    varOrderModelId=model.modelId;
    for(uint8_t i=0;i<MAX_MODEL_VARS;i++) varDisplayOrder[i]=i;
    Preferences prefs;
    if(!prefs.begin("varsorder",true)) return;
    char key[16];
    snprintf(key,sizeof(key),"m%u",(unsigned)model.modelId);
    uint8_t saved[MAX_MODEL_VARS]={};
    size_t n=prefs.getBytes(key,saved,sizeof(saved));
    prefs.end();
    if(n!=sizeof(saved)) return;
    bool seen[MAX_MODEL_VARS]={};
    for(uint8_t i=0;i<MAX_MODEL_VARS;i++){
        if(saved[i]>=MAX_MODEL_VARS||seen[saved[i]]) return;
        seen[saved[i]]=true;
    }
    memcpy(varDisplayOrder,saved,sizeof(saved));
}

static void saveVarDisplayOrder(){
    ModelConfig &model=modelManager.getActiveModel();
    Preferences prefs;
    if(!prefs.begin("varsorder",false)) return;
    char key[16];
    snprintf(key,sizeof(key),"m%u",(unsigned)model.modelId);
    prefs.putBytes(key,varDisplayOrder,sizeof(varDisplayOrder));
    prefs.end();
}

static bool isVarMoveMode(){return varMoveMode;}

static void exitVarMoveMode(){
    if(!varMoveMode) return;
    varMoveMode=false;
    varMoveFrom=-1;
    lv_async_call([](void *){refreshVarsList();},nullptr);
}

static void varMoveOutsideClicked(lv_event_t *e){
    if(!e||!varMoveMode||lv_event_get_code(e)!=LV_EVENT_CLICKED) return;
    exitVarMoveMode();
}

static void moveSelectedVar(int8_t direction){
    if(!varMoveMode||direction==0) return;
    if(varMoveFrom<0||varMoveFrom>=MAX_MODEL_VARS) return;
    loadVarDisplayOrder();
    ModelConfig &model=modelManager.getActiveModel();
    uint8_t from=(uint8_t)varMoveFrom;
    uint8_t position=MAX_MODEL_VARS;
    for(uint8_t i=0;i<MAX_MODEL_VARS;i++){
        if(varDisplayOrder[i]==from){position=i;break;}
    }
    if(position>=MAX_MODEL_VARS) return;
    int16_t next=(int16_t)position+(direction<0?-1:1);
    while(next>=0&&next<MAX_MODEL_VARS&&!model.vars[varDisplayOrder[next]].enabled)
        next+=direction<0?-1:1;
    if(next<0||next>=MAX_MODEL_VARS) return;
    if(position<(uint8_t)next){
        for(uint8_t i=position;i<(uint8_t)next;i++) varDisplayOrder[i]=varDisplayOrder[i+1];
    }else{
        for(uint8_t i=position;i>(uint8_t)next;i--) varDisplayOrder[i]=varDisplayOrder[i-1];
    }
    varDisplayOrder[(uint8_t)next]=from;
    selectedVarIndex=(int8_t)from;
    saveVarDisplayOrder();
    // Rows are LVGL event targets; rebuild after the click callback finishes.
    lv_async_call([](void *){refreshVarsList();},nullptr);
}

static void varMoveArrowClicked(lv_event_t *e){
    if(!e) return;
    int8_t direction=(int8_t)(intptr_t)lv_event_get_user_data(e);
    moveSelectedVar(direction);
    lv_event_stop_bubbling(e);
}

static void varMoveMenuClicked(lv_event_t *e){
    (void)e;
    if(sharedEditDeletePopupTarget!=SharedEditDeletePopupTarget::VAR) return;
    if(selectedVarIndex<0||selectedVarIndex>=MAX_MODEL_VARS) return;
    varMoveFrom=selectedVarIndex;
    varMoveMode=true;
    hideVarPopup();
    lv_async_call([](void *){refreshVarsList();},nullptr);
}

static void varCopyMenuClicked(lv_event_t *e){
    (void)e;
    if(sharedEditDeletePopupTarget!=SharedEditDeletePopupTarget::VAR) return;
    if(selectedVarIndex<0||selectedVarIndex>=MAX_MODEL_VARS) return;
    ModelConfig &model=modelManager.getActiveModel();
    if(!model.vars[(uint8_t)selectedVarIndex].enabled) return;
    varClipboard=model.vars[(uint8_t)selectedVarIndex];
    varClipboardValid=true;
    hideVarPopup();
}

static void varCloneMenuClicked(lv_event_t *e){
    (void)e;
    if(sharedEditDeletePopupTarget!=SharedEditDeletePopupTarget::VAR) return;
    if(selectedVarIndex<0||selectedVarIndex>=MAX_MODEL_VARS) return;
    ModelConfig &model=modelManager.getActiveModel();
    uint8_t source=(uint8_t)selectedVarIndex;
    if(!model.vars[source].enabled) return;
    int16_t freeSlot=-1;
    for(uint8_t i=0;i<MAX_MODEL_VARS;i++){
        if(!model.vars[i].enabled){freeSlot=i;break;}
    }
    if(freeSlot<0){hideVarPopup();return;}
    model.vars[(uint8_t)freeSlot]=model.vars[source];
    selectedVarIndex=(int8_t)freeSlot;
    hideVarPopup();
    varProcessor.reset();
    modelManager.requestSaveActiveModel();
    refreshVarsList();
}

static void varDeleteCancelClicked(lv_event_t *e){
    varPendingDelete=-1;
    closeSystemConfirmation();
}

static void varDeleteConfirmClicked(lv_event_t *e){
    int8_t index=varPendingDelete;
    varPendingDelete=-1;
    closeSystemConfirmation();
    if(index<0||index>=MAX_MODEL_VARS) return;
    ModelConfig &model=modelManager.getActiveModel();
    model.vars[(uint8_t)index]=VarConfig{};
    if(selectedVarIndex==index) selectedVarIndex=-1;
    if(editingVarIndex==index) editingVarIndex=-1;
    varProcessor.reset();
    modelManager.requestSaveActiveModel();
    refreshVarsList();
}

static bool sourceReferencesVar(const SourceId &source,uint8_t index){
    return source.type==SourceType::VAR&&source.index==index;
}

static bool conditionReferencesVar(const ConditionConfig &condition,uint8_t index){
    return sourceReferencesVar(condition.sourceA,index)||sourceReferencesVar(condition.sourceB,index);
}

static bool modelReferencesVar(const ModelConfig &model,uint8_t index){
    for(uint8_t i=0;i<MAX_MODEL_INPUTS;i++){
        if(model.inputs[i].enabled&&sourceReferencesVar(model.inputs[i].source,index)) return true;
    }
    for(uint8_t i=0;i<MAX_MIX_LINES;i++){
        const MixLine &mix=model.mixes[i];
        if(!mix.enabled) continue;
        if(sourceReferencesVar(mix.source,index)||conditionReferencesVar(mix.condition,index)) return true;
        for(uint8_t j=0;j<mix.actionCount&&j<MAX_MIX_ACTIONS;j++){
            const MixAction &action=mix.actions[j];
            if(!action.enabled) continue;
            if(conditionReferencesVar(action.condition,index)) return true;
            for(uint8_t k=0;k<action.variantCount&&k<MAX_MIX_ACTION_VARIANTS;k++){
                if(action.variants[k].enabled&&conditionReferencesVar(action.variants[k].condition,index)) return true;
            }
        }
    }
    for(uint8_t i=0;i<MAX_FLIGHT_MODES;i++){
        if(model.flightModes[i].enabled&&conditionReferencesVar(model.flightModes[i].condition,index)) return true;
    }
    for(uint8_t i=0;i<MAX_LOGICAL_SWITCHES;i++){
        const LogicalSwitchConfig &ls=model.logicalSwitches[i];
        if(!ls.enabled) continue;
        if(conditionReferencesVar(ls.conditionA,index)||conditionReferencesVar(ls.conditionB,index)) return true;
    }
    for(uint8_t i=0;i<MAX_MODEL_VARS;i++){
        if(i==index||!model.vars[i].enabled) continue;
        const VarConfig &var=model.vars[i];
        for(uint8_t j=0;j<var.valueCount&&j<MAX_VAR_VALUES;j++){
            if(var.values[j].enabled&&conditionReferencesVar(var.values[j].condition,index)) return true;
        }
        for(uint8_t j=0;j<var.actionCount&&j<MAX_VAR_ACTIONS;j++){
            if(var.actions[j].enabled&&conditionReferencesVar(var.actions[j].condition,index)) return true;
        }
    }
    return false;
}

static void varDeleteMenuClicked(lv_event_t *e){
    (void)e;
    if(sharedEditDeletePopupTarget!=SharedEditDeletePopupTarget::VAR) return;
    if(selectedVarIndex<0||selectedVarIndex>=MAX_MODEL_VARS) return;
    ModelConfig &model=modelManager.getActiveModel();
    uint8_t index=(uint8_t)selectedVarIndex;
    if(!model.vars[index].enabled) return;
    hideVarPopup();
    if(modelReferencesVar(model,index)){
        showSystemConfirmation("This Var is referenced by the model. Remove its references before deleting it.","OK",varDeleteCancelClicked,nullptr,nullptr);
        return;
    }
    varPendingDelete=(int8_t)index;
    char message[120];
    snprintf(message,sizeof(message),"Delete %s (Var%u)?",model.vars[index].name,(unsigned)(index+1));
    showSystemConfirmation(message,"Delete",varDeleteConfirmClicked,"Cancel",varDeleteCancelClicked);
}

static void varPasteCancelClicked(lv_event_t *e){
    varPendingPaste=-1;
    closeSystemConfirmation();
}

static void varPasteConfirmClicked(lv_event_t *e){
    int8_t index=varPendingPaste;
    varPendingPaste=-1;
    closeSystemConfirmation();
    if(!varClipboardValid||index<0||index>=MAX_MODEL_VARS) return;
    ModelConfig &model=modelManager.getActiveModel();
    model.vars[(uint8_t)index]=varClipboard;
    model.vars[(uint8_t)index].enabled=true;
    varProcessor.reset();
    modelManager.requestSaveActiveModel();
    refreshVarsList();
}

static void varPasteMenuClicked(lv_event_t *e){
    (void)e;
    if(sharedEditDeletePopupTarget!=SharedEditDeletePopupTarget::VAR||!varClipboardValid) return;
    if(selectedVarIndex<0||selectedVarIndex>=MAX_MODEL_VARS) return;
    varPendingPaste=selectedVarIndex;
    hideVarPopup();
    showSystemConfirmation("Replace the selected Var with the copied settings?","Paste",varPasteConfirmClicked,"Cancel",varPasteCancelClicked);
}

static void prepareVarPopup(uint8_t index){
    if(!objects.curve_popup||!objects.scr_vars||index>=MAX_MODEL_VARS) return;
    ModelConfig &model=modelManager.getActiveModel();
    VarConfig &var=model.vars[index];
    if(!var.enabled) return;

    selectedVarIndex=(int8_t)index;
    sharedEditDeletePopupTarget=SharedEditDeletePopupTarget::VAR;
    resetSharedPopupSelectionState();

    if(lv_obj_get_parent(objects.curve_popup)!=objects.scr_vars)
        lv_obj_set_parent(objects.curve_popup,objects.scr_vars);

    setSharedPopupItemVisible(objects.curve_popup__popup_item1,true);
    setSharedPopupItemVisible(objects.curve_popup__popup_item2,true);
    setSharedPopupItemVisible(objects.curve_popup__popup_item3,true);
    setSharedPopupItemVisible(objects.curve_popup__popup_item4,true);
    setSharedPopupItemVisible(objects.curve_popup__popup_item5,true);
    setSharedPopupItemVisible(objects.curve_popup__popup_item6,false);
    setSharedPopupItemVisible(objects.curve_popup__popup_item7,false);

    if(objects.curve_popup__popup_item1_lbl) lv_label_set_text(objects.curve_popup__popup_item1_lbl,"Edit");
    if(objects.curve_popup__popup_item2_lbl) lv_label_set_text(objects.curve_popup__popup_item2_lbl,"Move");
    if(objects.curve_popup__popup_item3_lbl) lv_label_set_text(objects.curve_popup__popup_item3_lbl,"Copy");
    if(objects.curve_popup__popup_item4_lbl) lv_label_set_text(objects.curve_popup__popup_item4_lbl,"Clone");
    if(objects.curve_popup__popup_item5_lbl) lv_label_set_text(objects.curve_popup__popup_item5_lbl,"Delete");

    if(objects.curve_popup__edited_obj_name)
        lv_label_set_text(objects.curve_popup__edited_obj_name,var.name[0]!='\0'?var.name:"Var");

    // Shared popup is reused by Curve, Mix and Action. Remove stale handlers
    // before installing Vars-specific operations.
    if(objects.curve_popup__popup_item2){
        lv_obj_remove_event_cb(objects.curve_popup__popup_item2,mixActionCloneMenuClicked);
        lv_obj_remove_event_cb(objects.curve_popup__popup_item2,varMoveMenuClicked);
        lv_obj_add_event_cb(objects.curve_popup__popup_item2,varMoveMenuClicked,LV_EVENT_CLICKED,nullptr);
    }
    if(objects.curve_popup__popup_item3){
        lv_obj_remove_event_cb(objects.curve_popup__popup_item3,action_delete_menu_clicked);
        lv_obj_remove_event_cb(objects.curve_popup__popup_item3,mixActionAddMenuClicked);
        lv_obj_remove_event_cb(objects.curve_popup__popup_item3,varCopyMenuClicked);
        lv_obj_add_event_cb(objects.curve_popup__popup_item3,varCopyMenuClicked,LV_EVENT_CLICKED,nullptr);
    }
    if(objects.curve_popup__popup_item4){
        lv_obj_remove_event_cb(objects.curve_popup__popup_item4,mixActionMoveMenuClicked);
        lv_obj_remove_event_cb(objects.curve_popup__popup_item4,varCloneMenuClicked);
        lv_obj_add_event_cb(objects.curve_popup__popup_item4,varCloneMenuClicked,LV_EVENT_CLICKED,nullptr);
    }
    if(objects.curve_popup__popup_item5){
        lv_obj_remove_event_cb(objects.curve_popup__popup_item5,action_delete_menu_clicked);
        lv_obj_remove_event_cb(objects.curve_popup__popup_item5,varDeleteMenuClicked);
        lv_obj_add_event_cb(objects.curve_popup__popup_item5,varDeleteMenuClicked,LV_EVENT_CLICKED,nullptr);
    }
    // Clipboard Paste appears as an additional row once Copy was used.
    setSharedPopupItemVisible(objects.curve_popup__popup_item6,varClipboardValid);
    if(objects.curve_popup__popup_item6_lbl) lv_label_set_text(objects.curve_popup__popup_item6_lbl,"Paste");
    if(objects.curve_popup__popup_item6){
        lv_obj_remove_event_cb(objects.curve_popup__popup_item6,varPasteMenuClicked);
        lv_obj_add_event_cb(objects.curve_popup__popup_item6,varPasteMenuClicked,LV_EVENT_CLICKED,nullptr);
    }

    lv_obj_set_pos(objects.curve_popup,0,0);
    lv_obj_remove_flag(objects.curve_popup,LV_OBJ_FLAG_HIDDEN);
    showPopupDismissOverlay(objects.curve_popup);
}

static void varListRowClicked(lv_event_t *e){
    if(!e) return;
    uint8_t index=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(index>=MAX_MODEL_VARS) return;
    if(varMoveMode){
        exitVarMoveMode();
        return;
    }

    lv_obj_t *row=dynamicVarRows[index];
    bool alreadyFocused=selectedVarIndex==(int8_t)index&&row&&
        (lv_obj_has_state(row,LV_STATE_FOCUSED)||lv_obj_has_state(row,LV_STATE_CHECKED));

    selectedVarIndex=(int8_t)index;
    for(uint8_t i=0;i<MAX_MODEL_VARS;i++){
        if(!dynamicVarRows[i]) continue;
        lv_obj_remove_state(dynamicVarRows[i],LV_STATE_CHECKED);
        lv_obj_remove_state(dynamicVarRows[i],LV_STATE_FOCUSED);
    }
    if(row){
        lv_obj_add_state(row,LV_STATE_CHECKED);
        lv_obj_add_state(row,LV_STATE_FOCUSED);
    }

    updateVarsListInfo(selectedVarIndex);
    if(alreadyFocused) prepareVarPopup(index);
}

static void clearDynamicVarRows(){
    for(uint8_t i=0;i<MAX_MODEL_VARS;i++){
        if(!dynamicVarRows[i]) continue;
        lv_obj_del(dynamicVarRows[i]);
        dynamicVarRows[i]=nullptr;
        dynamicVarValueLabels[i]=nullptr;
        lastRenderedVarValues[i]=INT32_MIN;
    }
}

static void refreshVarsList(){
    if(!objects.vars_list_panel) return;
    clearDynamicVarRows();

    // EEZ contains one visual sample row (obj2). Runtime list replaces it.
    if(objects.obj2) lv_obj_add_flag(objects.obj2,LV_OBJ_FLAG_HIDDEN);

    ModelConfig &model=modelManager.getActiveModel();
    loadVarDisplayOrder();
    uint8_t visibleRow=0;
    int8_t firstEnabled=-1;

    for(uint8_t position=0;position<MAX_MODEL_VARS;position++){
        uint8_t i=varDisplayOrder[position];
        VarConfig &var=model.vars[i];
        if(!var.enabled) continue;
        if(firstEnabled<0) firstEnabled=(int8_t)i;
        ensureVarDefaultValue(var);

        lv_obj_t *row=lv_obj_create(objects.vars_list_panel);
        dynamicVarRows[i]=row;
        lv_obj_set_width(row,LV_PCT(100));
        lv_obj_set_height(row,50);
        lv_obj_set_style_pad_left(row,10,LV_PART_MAIN);
        lv_obj_set_style_pad_right(row,10,LV_PART_MAIN);
        lv_obj_set_style_pad_top(row,0,LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(row,0,LV_PART_MAIN);
        lv_obj_set_style_pad_column(row,10,LV_PART_MAIN);
        lv_obj_set_style_border_width(row,0,LV_PART_MAIN);
        lv_obj_set_style_radius(row,0,LV_PART_MAIN);
        lv_obj_set_style_layout(row,LV_LAYOUT_FLEX,LV_PART_MAIN);
        lv_obj_set_style_flex_flow(row,LV_FLEX_FLOW_ROW,LV_PART_MAIN);
        lv_obj_set_style_flex_track_place(row,LV_FLEX_ALIGN_CENTER,LV_PART_MAIN);
        lv_obj_remove_flag(row,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(row,LV_OBJ_FLAG_CHECKABLE);

        uint8_t bgId=(visibleRow&1)?COLOR_ID_FRONT_BG1:COLOR_ID_FRONT_BG;
        lv_obj_set_style_bg_color(row,lv_color_hex(theme_colors[active_theme_index][bgId]),LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row,255,LV_PART_MAIN);
        lv_obj_set_style_text_color(row,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_TEXT]),LV_PART_MAIN);

        lv_style_selector_t focused=(lv_style_selector_t)((uint32_t)LV_PART_MAIN|(uint32_t)LV_STATE_FOCUSED);
        lv_obj_set_style_bg_color(row,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_MS_TEXT]),focused);
        lv_obj_set_style_bg_opa(row,255,focused);
        lv_obj_set_style_text_color(row,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_BG]),focused);

        lv_style_selector_t checked=(lv_style_selector_t)((uint32_t)LV_PART_MAIN|(uint32_t)LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(row,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_MS_TEXT]),checked);
        lv_obj_set_style_bg_opa(row,255,checked);
        lv_obj_set_style_text_color(row,lv_color_hex(theme_colors[active_theme_index][COLOR_ID_BG]),checked);

        // varListRowClicked manages exclusive row selection and first/second tap semantics.
        lv_obj_add_event_cb(row,exclusiveListRowPressed,LV_EVENT_PRESSED,nullptr);
        lv_obj_add_event_cb(row,varListRowClicked,LV_EVENT_CLICKED,(void *)(uintptr_t)i);

        lv_obj_t *number=lv_label_create(row);
        lv_obj_set_size(number,120,22);
        lv_label_set_long_mode(number,LV_LABEL_LONG_WRAP);
        lv_label_set_text_fmt(number,"Var%u",(unsigned)(i+1));

        lv_obj_t *name=lv_label_create(row);
        lv_obj_set_size(name,220,22);
        lv_label_set_long_mode(name,LV_LABEL_LONG_CLIP);
        lv_label_set_text(name,var.name[0]!='\0'?var.name:"---");

        lv_obj_t *value=lv_label_create(row);
        dynamicVarValueLabels[i]=value;
        lv_obj_set_size(value,200,22);
        lv_label_set_long_mode(value,LV_LABEL_LONG_WRAP);
        int16_t displayValue=getVarCurrentDisplayValue(var);
        lastRenderedVarValues[i]=displayValue;
        int32_t absValue=abs((int)displayValue);
        lv_label_set_text_fmt(value,"%s%ld.%ld%%",displayValue<0?"-":"",(long)(absValue/10),(long)(absValue%10));

        if(varMoveMode&&varMoveFrom==(int8_t)i){
            // Same Up/Down interaction as FreeMix Actions, without changing Var SourceId.
            lv_obj_set_width(number,90);
            lv_obj_set_width(name,170);
            lv_obj_set_width(value,140);
            lv_obj_t *up=lv_button_create(row);
            lv_obj_set_size(up,38,32);
            lv_obj_remove_flag(up,LV_OBJ_FLAG_EVENT_BUBBLE);
            lv_obj_add_event_cb(up,varMoveArrowClicked,LV_EVENT_CLICKED,(void *)(intptr_t)-1);
            lv_obj_t *upLabel=lv_label_create(up);
            lv_label_set_text(upLabel,LV_SYMBOL_UP);
            lv_obj_center(upLabel);
            lv_obj_t *down=lv_button_create(row);
            lv_obj_set_size(down,38,32);
            lv_obj_remove_flag(down,LV_OBJ_FLAG_EVENT_BUBBLE);
            lv_obj_add_event_cb(down,varMoveArrowClicked,LV_EVENT_CLICKED,(void *)(intptr_t)1);
            lv_obj_t *downLabel=lv_label_create(down);
            lv_label_set_text(downLabel,LV_SYMBOL_DOWN);
            lv_obj_center(downLabel);
            uint8_t prev=0,next=0;
            for(uint8_t k=0;k<MAX_MODEL_VARS;k++){
                if(varDisplayOrder[k]!=i) continue;
                for(uint8_t j=0;j<k;j++){
                    if(model.vars[varDisplayOrder[j]].enabled) prev=1;
                }
                for(uint8_t j=(uint8_t)(k+1);j<MAX_MODEL_VARS;j++){
                    if(model.vars[varDisplayOrder[j]].enabled) next=1;
                }
                break;
            }
            if(!prev) lv_obj_add_state(up,LV_STATE_DISABLED);
            if(!next) lv_obj_add_state(down,LV_STATE_DISABLED);
        }

        visibleRow++;
    }

    if(selectedVarIndex<0||selectedVarIndex>=MAX_MODEL_VARS||!model.vars[(uint8_t)selectedVarIndex].enabled)
        selectedVarIndex=firstEnabled;

    if(selectedVarIndex>=0&&dynamicVarRows[(uint8_t)selectedVarIndex]){
        lv_obj_add_state(dynamicVarRows[(uint8_t)selectedVarIndex],LV_STATE_CHECKED);
        lv_obj_add_state(dynamicVarRows[(uint8_t)selectedVarIndex],LV_STATE_FOCUSED);
    }
    updateVarsListInfo(selectedVarIndex);
}

static void openVarEditor(uint8_t varIndex){
    if(varIndex>=MAX_MODEL_VARS) return;
    ModelConfig &model=modelManager.getActiveModel();
    if(!model.vars[varIndex].enabled) return;

    editingVarIndex=(int8_t)varIndex;
    lastRenderedEditorVar=-1;
    lastRenderedEditorValue=INT32_MIN;
    activeVarNumericField=VarNumericField::NONE;
    updateVarEditorUi();

    if(objects.vars_main_panel) lv_obj_add_flag(objects.vars_main_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.vars_addedit_main_panel) lv_obj_remove_flag(objects.vars_addedit_main_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.vars_addedit_panel) lv_obj_scroll_to_y(objects.vars_addedit_panel,0,LV_ANIM_OFF);
}

static int16_t *getActiveVarNumericPtr(){
    VarConfig *var=getEditingVar();
    if(!var) return nullptr;
    ensureVarDefaultValue(*var);
    switch(activeVarNumericField){
        case VarNumericField::RANGE_MIN:return &var->rangeMin;
        case VarNumericField::RANGE_MAX:return &var->rangeMax;
        case VarNumericField::VALUE_DEFAULT:return &var->values[0].value;
        case VarNumericField::VALUE_CONDITIONAL:
            if(activeVarValueSlot>0&&activeVarValueSlot<var->valueCount&&activeVarValueSlot<MAX_VAR_VALUES) return &var->values[activeVarValueSlot].value;
            return nullptr;
        case VarNumericField::ACTION_PARAMETER:
            if(activeVarActionSlot<var->actionCount&&activeVarActionSlot<MAX_VAR_ACTIONS) return &var->actions[activeVarActionSlot].parameter;
            return nullptr;
        default:return nullptr;
    }
}

static void getActiveVarNumericBounds(int16_t &minValue,int16_t &maxValue){
    VarConfig *var=getEditingVar();
    minValue=-5000;
    maxValue=5000;
    if(!var) return;

    switch(activeVarNumericField){
        case VarNumericField::RANGE_MIN:maxValue=var->rangeMax;break;
        case VarNumericField::RANGE_MAX:minValue=var->rangeMin;break;
        case VarNumericField::VALUE_DEFAULT:
        case VarNumericField::VALUE_CONDITIONAL:
            minValue=-5000;
            maxValue=5000;
            break;
        case VarNumericField::ACTION_PARAMETER:
            minValue=-5000;
            maxValue=5000;
            break;
        default:break;
    }
}

static void updateVarNumericLabel(){
    VarConfig *var=getEditingVar();
    if(!var) return;
    switch(activeVarNumericField){
        case VarNumericField::RANGE_MIN:formatVarPercent(objects.vars_range_from_lbl,var->rangeMin);break;
        case VarNumericField::RANGE_MAX:formatVarPercent(objects.vars_range_to_lbl,var->rangeMax);break;
        case VarNumericField::VALUE_DEFAULT:formatVarPercent(objects.vars_value_0_value_lbl,var->values[0].value);break;
        case VarNumericField::VALUE_CONDITIONAL:
            if(activeVarValueSlot>0&&activeVarValueSlot<var->valueCount) formatVarPercent(getVarValueLabel(activeVarValueSlot),var->values[activeVarValueSlot].value);
            break;
        case VarNumericField::ACTION_PARAMETER:
            if(activeVarActionSlot<var->actionCount) formatVarActionParameter(getVarActionValueLabel(activeVarActionSlot),var->actions[activeVarActionSlot].parameter);
            break;
        default:break;
    }
    updateVarSummaryUi();
}

static void updateVarChangerUi(){
    int16_t *value=getActiveVarNumericPtr();
    if(!value||!objects.vars_changer_panel) return;

    int16_t minValue,maxValue;
    getActiveVarNumericBounds(minValue,maxValue);
    *value=constrain(*value,minValue,maxValue);

    if(objects.vars_changer_panel__dropdown_step){
        const char *options=activeVarNumericField==VarNumericField::ACTION_PARAMETER?"0.1\n1.0\n10.0\n100.0":"0.1%\n1.0%\n10.0%\n100.0%";
        const char *current=lv_dropdown_get_options(objects.vars_changer_panel__dropdown_step);
        if(!current||strcmp(current,options)!=0)
            lv_dropdown_set_options(objects.vars_changer_panel__dropdown_step,options);
        if(varNumericStepSelected>3) varNumericStepSelected=3;
        lv_dropdown_set_selected(objects.vars_changer_panel__dropdown_step,varNumericStepSelected);
    }

    if(objects.vars_changer_panel__mcmsusd_slider){
        lv_slider_set_range(objects.vars_changer_panel__mcmsusd_slider,minValue,maxValue);
        lv_slider_set_value(objects.vars_changer_panel__mcmsusd_slider,*value,LV_ANIM_OFF);
    }

    if(objects.vars_changer_panel__reduce_step){
        if(varNumericStepSelected==0) lv_obj_add_state(objects.vars_changer_panel__reduce_step,LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.vars_changer_panel__reduce_step,LV_STATE_DISABLED);
    }
    if(objects.vars_changer_panel__increase_step){
        if(varNumericStepSelected>=3) lv_obj_add_state(objects.vars_changer_panel__increase_step,LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.vars_changer_panel__increase_step,LV_STATE_DISABLED);
    }
    if(objects.vars_changer_panel__step_down){
        if(*value<=minValue) lv_obj_add_state(objects.vars_changer_panel__step_down,LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.vars_changer_panel__step_down,LV_STATE_DISABLED);
    }
    if(objects.vars_changer_panel__step_up){
        if(*value>=maxValue) lv_obj_add_state(objects.vars_changer_panel__step_up,LV_STATE_DISABLED);
        else lv_obj_remove_state(objects.vars_changer_panel__step_up,LV_STATE_DISABLED);
    }

    updateVarNumericLabel();
}

static void hideVarChanger(){
    varChangerOpenTarget=nullptr;
    activeVarNumericField=VarNumericField::NONE;
    if(objects.vars_changer_panel) lv_obj_add_flag(objects.vars_changer_panel,LV_OBJ_FLAG_HIDDEN);
}

static void varNumericFieldClicked(lv_event_t *e){
    if(!e||!getEditingVar()) return;
    varChangerOpenTarget=(lv_obj_t *)lv_event_get_current_target(e);
    uintptr_t field=(uintptr_t)lv_event_get_user_data(e);
    if(field>=100&&field<100+MAX_VAR_VALUES){
        activeVarValueSlot=(uint8_t)(field-100);
        activeVarNumericField=activeVarValueSlot==0?VarNumericField::VALUE_DEFAULT:VarNumericField::VALUE_CONDITIONAL;
    }else if(field>=200&&field<200+MAX_VAR_ACTIONS){
        activeVarActionSlot=(uint8_t)(field-200);
        activeVarNumericField=VarNumericField::ACTION_PARAMETER;
    }else{
        if(field<1||field>3) return;
        activeVarValueSlot=0;
        activeVarNumericField=(VarNumericField)field;
    }

    if(objects.vars_changer_panel__button_panel)
        lv_obj_remove_flag(objects.vars_changer_panel__button_panel,LV_OBJ_FLAG_HIDDEN);
    if(objects.vars_changer_panel__slider_panel)
        lv_obj_add_flag(objects.vars_changer_panel__slider_panel,LV_OBJ_FLAG_HIDDEN);

    updateVarChangerUi();
    bindChangerDismissRecursive(objects.vars_addedit_main_panel,objects.vars_changer_panel,dismissVarChangerOnPress);
    if(objects.vars_changer_panel) lv_obj_remove_flag(objects.vars_changer_panel,LV_OBJ_FLAG_HIDDEN);
}

static void varsNumericChangeStep(lv_event_t *e){
    if(!e||activeVarNumericField==VarNumericField::NONE) return;
    uintptr_t direction=(uintptr_t)lv_event_get_user_data(e);
    if(direction==0){
        if(varNumericStepSelected>0) varNumericStepSelected--;
    }else if(direction==1){
        if(varNumericStepSelected<3) varNumericStepSelected++;
    }else return;
    updateVarChangerUi();
}

static void varsNumericDoStep(lv_event_t *e){
    if(!e||activeVarNumericField==VarNumericField::NONE) return;
    int16_t *value=getActiveVarNumericPtr();
    if(!value) return;

    uintptr_t command=(uintptr_t)lv_event_get_user_data(e);
    static const int16_t steps[4]={1,10,100,1000};
    int16_t minValue,maxValue;
    getActiveVarNumericBounds(minValue,maxValue);

    if(command==2){
        if(!objects.vars_changer_panel__mcmsusd_slider) return;
        *value=(int16_t)lv_slider_get_value(objects.vars_changer_panel__mcmsusd_slider);
    }else if(command==0){
        *value=(int16_t)(*value-steps[varNumericStepSelected]);
    }else if(command==1){
        *value=(int16_t)(*value+steps[varNumericStepSelected]);
    }else return;

    *value=constrain(*value,minValue,maxValue);

    updateVarNumericLabel();
    if(command!=2&&objects.vars_changer_panel__mcmsusd_slider)
        lv_slider_set_value(objects.vars_changer_panel__mcmsusd_slider,*value,LV_ANIM_OFF);
    updateVarChangerUi();
    modelManager.requestSaveActiveModel();
}

static void setVarOptionCheckbox(lv_obj_t *checkbox,bool checked){
    if(!checkbox) return;
    if(checked) lv_obj_add_state(checkbox,LV_STATE_CHECKED);
    else lv_obj_remove_state(checkbox,LV_STATE_CHECKED);
}

static void openVarOptions(uint8_t code,lv_obj_t *target){
    VarConfig *var=getEditingVar();
    if(!var||!objects.var_option_panel) return;
    bool isAction=code>=11&&code<11+MAX_VAR_ACTIONS;
    uint8_t slot=isAction?(uint8_t)(code-11):code;
    if(isAction){if(slot>=var->actionCount) return;}
    else if(slot<1||slot>=var->valueCount) return;
    varOptionsIsAction=isAction;
    varOptionsSlot=(int8_t)slot;
    varOptionSelectedPanel=nullptr;
    varOptionsLongPressTarget=target;
    suppressVarConditionClick=true;
    varOptionsLongPressTick=lv_tick_get();
    const ConditionConfig &condition=isAction?var->actions[slot].condition:var->values[slot].condition;
    setVarOptionCheckbox(objects.var_invert_checkbox,condition.inverted);
    setVarOptionCheckbox(objects.var_edge_checkbox,isAction&&var->actions[slot].edge);
    if(objects.var_edge_panel){
        if(isAction) lv_obj_remove_flag(objects.var_edge_panel,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(objects.var_edge_panel,LV_OBJ_FLAG_HIDDEN);
    }
    // A newly opened popup has no focused option.
    if(objects.var_invert_panel) lv_obj_remove_state(objects.var_invert_panel,LV_STATE_FOCUSED);
    if(objects.var_edge_panel) lv_obj_remove_state(objects.var_edge_panel,LV_STATE_FOCUSED);
    lv_obj_remove_flag(objects.var_option_panel,LV_OBJ_FLAG_HIDDEN);
    showPopupDismissOverlay(objects.var_option_panel);
}

static void varConditionLongPressed(lv_event_t *e){
    if(!e) return;
    openVarOptions((uint8_t)(uintptr_t)lv_event_get_user_data(e),(lv_obj_t *)lv_event_get_current_target(e));
}

static void varOptionChanged(lv_event_t *e){
    if(!e||varOptionsSlot<0) return;
    VarConfig *var=getEditingVar();
    if(!var) return;
    const uint8_t slot=(uint8_t)varOptionsSlot;
    if(varOptionsIsAction){if(slot>=var->actionCount) return;}
    else if(slot<1||slot>=var->valueCount) return;
    const bool invert=lv_obj_has_state(objects.var_invert_checkbox,LV_STATE_CHECKED);
    const bool edge=lv_obj_has_state(objects.var_edge_checkbox,LV_STATE_CHECKED);
    if(varOptionsIsAction){
        var->actions[slot].condition.inverted=invert;
        var->actions[slot].edge=edge;
        updateVarActionsUi();
    }else{
        var->values[slot].condition.inverted=invert;
        updateVarValuesUi();
    }
    updateVarSummaryUi();
    modelManager.requestSaveActiveModel();
}

static void varOptionPanelClicked(lv_event_t *e){
    if(!e||varOptionsSlot<0) return;
    lv_obj_t *target=(lv_obj_t *)lv_event_get_current_target(e);
    if(target!=objects.var_invert_panel&&target!=objects.var_edge_panel) return;
    if(target==objects.var_edge_panel&&!varOptionsIsAction) return;

    lv_obj_t *other=target==objects.var_invert_panel?objects.var_edge_panel:objects.var_invert_panel;
    // LVGL may set FOCUSED before CLICKED fires. Track selection separately
    // so the first click never changes the option, even if LVGL focused it.
    if(varOptionSelectedPanel!=target){
        varOptionSelectedPanel=target;
        if(other) lv_obj_remove_state(other,LV_STATE_FOCUSED);
        lv_obj_add_state(target,LV_STATE_FOCUSED);
        return;
    }

    // Only clicking the already selected option toggles its checkbox.
    lv_obj_t *checkbox=target==objects.var_invert_panel?objects.var_invert_checkbox:objects.var_edge_checkbox;
    if(!checkbox) return;
    setVarOptionCheckbox(checkbox,!lv_obj_has_state(checkbox,LV_STATE_CHECKED));
    varOptionChanged(e);
}

static void setupVarsUiCallbacks(){
    if(!objects.scr_vars) return;
    if(varsCallbacksBoundScreen==objects.scr_vars) return;
    varsCallbacksBoundScreen=objects.scr_vars;

    if(objects.vars_list_panel){
        lv_obj_add_flag(objects.vars_list_panel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_event_cb(objects.vars_list_panel,varMoveOutsideClicked);
        lv_obj_add_event_cb(objects.vars_list_panel,varMoveOutsideClicked,LV_EVENT_CLICKED,nullptr);
    }
    if(objects.vars_name){
        lv_obj_remove_event_cb(objects.vars_name,varTextEditorEvent);
        lv_obj_add_event_cb(objects.vars_name,varTextEditorEvent,LV_EVENT_DEFOCUSED,nullptr);
    }
    if(objects.vars_edit_comment){
        lv_textarea_set_max_length(objects.vars_edit_comment,sizeof(VarConfig{}.comment)-1);
        lv_obj_remove_event_cb(objects.vars_edit_comment,varTextEditorEvent);
        lv_obj_add_event_cb(objects.vars_edit_comment,varTextEditorEvent,LV_EVENT_DEFOCUSED,nullptr);
    }

    if(objects.vars_range_from_panel){
        lv_obj_add_flag(objects.vars_range_from_panel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(objects.vars_range_from_panel,varNumericFieldClicked,LV_EVENT_CLICKED,(void *)(uintptr_t)VarNumericField::RANGE_MIN);
    }
    if(objects.vars_range_to_panel){
        lv_obj_add_flag(objects.vars_range_to_panel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(objects.vars_range_to_panel,varNumericFieldClicked,LV_EVENT_CLICKED,(void *)(uintptr_t)VarNumericField::RANGE_MAX);
    }
    if(objects.vars_value_0_value_panel){
        lv_obj_add_flag(objects.vars_value_0_value_panel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(objects.vars_value_0_value_panel,varNumericFieldClicked,LV_EVENT_CLICKED,(void *)(uintptr_t)VarNumericField::VALUE_DEFAULT);
    }

    for(uint8_t slot=1;slot<MAX_VAR_VALUES;slot++){
        lv_obj_t *panel=getVarValuePanel(slot);
        if(!panel) continue;
        lv_obj_add_flag(panel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(panel,varNumericFieldClicked,LV_EVENT_CLICKED,(void *)(uintptr_t)(100+slot));
    }
    for(uint8_t index=0;index<MAX_VAR_ACTIONS;index++){
        lv_obj_t *panel=getVarActionValuePanel(index);
        if(!panel) continue;
        lv_obj_add_flag(panel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(panel,varNumericFieldClicked,LV_EVENT_CLICKED,(void *)(uintptr_t)(200+index));
    }

    for(uint8_t slot=1;slot<MAX_VAR_VALUES;slot++){
        lv_obj_t *label=getVarValueConditionLabel(slot);
        lv_obj_t *panel=label?lv_obj_get_parent(label):nullptr;
        if(panel) lv_obj_add_event_cb(panel,varConditionLongPressed,LV_EVENT_LONG_PRESSED,(void *)(uintptr_t)slot);
    }
    for(uint8_t index=0;index<MAX_VAR_ACTIONS;index++){
        lv_obj_t *label=getVarActionConditionLabel(index);
        lv_obj_t *panel=label?lv_obj_get_parent(label):nullptr;
        if(panel) lv_obj_add_event_cb(panel,varConditionLongPressed,LV_EVENT_LONG_PRESSED,(void *)(uintptr_t)(11+index));
    }
    // Both rows are the click targets; checkboxes are display-only indicators.
    // Focus styling mirrors the ms_text focused-state selection pattern.
    lv_obj_t *optionPanels[2]={objects.var_invert_panel,objects.var_edge_panel};
    for(lv_obj_t *panel:optionPanels){
        if(!panel) continue;
        lv_obj_add_flag(panel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(panel,lv_color_hex(theme_colors[active_theme_index][3]),LV_PART_MAIN|LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(panel,lv_color_hex(theme_colors[active_theme_index][9]),LV_PART_MAIN|LV_STATE_FOCUSED);
        lv_obj_add_event_cb(panel,varOptionPanelClicked,LV_EVENT_CLICKED,nullptr);
    }
    if(objects.var_invert_checkbox) lv_obj_remove_flag(objects.var_invert_checkbox,LV_OBJ_FLAG_CLICKABLE);
    if(objects.var_edge_checkbox) lv_obj_remove_flag(objects.var_edge_checkbox,LV_OBJ_FLAG_CLICKABLE);

    setupVarConditionCategoryCallbacks();
    bindChangerDismissRecursive(objects.vars_addedit_main_panel,objects.vars_changer_panel,dismissVarChangerOnPress);
}

void action_add_var(lv_event_t *e){
    ModelConfig &model=modelManager.getActiveModel();
    int16_t freeIndex=-1;
    for(uint8_t i=0;i<MAX_MODEL_VARS;i++){
        if(!model.vars[i].enabled){freeIndex=i;break;}
    }
    if(freeIndex<0) return;

    VarConfig &var=model.vars[(uint8_t)freeIndex];
    var=VarConfig{};
    var.enabled=true;
    snprintf(var.name,sizeof(var.name),"Var%u",(unsigned)(freeIndex+1));
    var.rangeMin=-1000;
    var.rangeMax=1000;
    var.valueCount=1;
    var.values[0].enabled=true;
    var.values[0].condition.type=ConditionType::ALWAYS;
    var.values[0].value=0;

    selectedVarIndex=(int8_t)freeIndex;
    modelManager.requestSaveActiveModel();
    refreshVarsList();
    openVarEditor((uint8_t)freeIndex);
}

// EEZ callbacks already present in the generated Vars screen. Their full behavior
// is implemented in the next Values/Actions increment; keeping definitions here
// makes the new EEZ screen linkable immediately.
void action_vars_del_value(lv_event_t *e){
    if(!e) return;
    VarConfig *var=getEditingVar();
    if(!var) return;
    uint8_t slot=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(slot==0||slot>=var->valueCount||slot>=MAX_VAR_VALUES) return;
    for(uint8_t i=slot;i+1<var->valueCount;i++) var->values[i]=var->values[i+1];
    var->values[var->valueCount-1]=VarValueConfig{};
    var->valueCount--;
    if(var->valueCount<1) var->valueCount=1;
    hideVarChanger();
    updateVarValuesUi();
    updateVarSummaryUi();
    modelManager.requestSaveActiveModel();
}

void action_vars_condition_show(lv_event_t *e){
    if(!e) return;
    if(suppressVarConditionClick){
        if(lv_event_get_current_target(e)==varOptionsLongPressTarget&&lv_tick_elaps(varOptionsLongPressTick)<1000){
            suppressVarConditionClick=false;
            return;
        }
        suppressVarConditionClick=false;
    }
    VarConfig *var=getEditingVar();
    if(!var) return;
    uint8_t code=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(code>=1&&code<MAX_VAR_VALUES){
        if(code>=var->valueCount) return;
        editingVarConditionIsAction=false;
        editingVarConditionSlot=(int8_t)code;
        if(objects.vars_category_direction) lv_obj_remove_flag(objects.vars_category_direction,LV_OBJ_FLAG_HIDDEN);
    }else if(code>=11&&code<11+MAX_VAR_ACTIONS){
        uint8_t index=(uint8_t)(code-11);
        if(index>=var->actionCount) return;
        editingVarConditionIsAction=true;
        editingVarConditionSlot=(int8_t)index;
        if(objects.vars_category_direction) lv_obj_add_flag(objects.vars_category_direction,LV_OBJ_FLAG_HIDDEN);
    }else return;
    clearVarConditionMembers();
    if(objects.vars_active_condition_panel){
        lv_obj_remove_flag(objects.vars_active_condition_panel,LV_OBJ_FLAG_HIDDEN);
        showPopupDismissOverlay(objects.vars_active_condition_panel);
        startVarConditionAutoDetect();
    }
}

void action_var_add_value(lv_event_t *e){
    (void)e;
    VarConfig *var=getEditingVar();
    if(!var||var->valueCount>=MAX_VAR_VALUES) return;
    uint8_t slot=var->valueCount;
    var->values[slot]=VarValueConfig{};
    var->values[slot].enabled=true;
    var->values[slot].condition.type=ConditionType::NEVER;
    var->values[slot].value=constrain(var->values[0].value,(int16_t)-5000,(int16_t)5000);
    var->valueCount++;
    updateVarValuesUi();
    modelManager.requestSaveActiveModel();
}

void action_vars_del_actions(lv_event_t *e){
    if(!e) return;
    VarConfig *var=getEditingVar();
    if(!var) return;
    uint8_t oneBased=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(oneBased<1||oneBased>var->actionCount) return;
    uint8_t index=(uint8_t)(oneBased-1);
    for(uint8_t i=index;i+1<var->actionCount;i++) var->actions[i]=var->actions[i+1];
    if(var->actionCount>0){var->actionCount--;var->actions[var->actionCount]=VarActionConfig{};}
    if(activeVarNumericField==VarNumericField::ACTION_PARAMETER) hideVarChanger();
    updateVarActionsUi();
    modelManager.requestSaveActiveModel();
}
void action_var_add_actions(lv_event_t *e){
    (void)e;
    VarConfig *var=getEditingVar();
    if(!var||var->actionCount>=MAX_VAR_ACTIONS) return;
    uint8_t index=var->actionCount;
    var->actions[index]=VarActionConfig{};
    var->actions[index].enabled=true;
    var->actions[index].condition.type=ConditionType::NEVER;
    var->actions[index].function=VarActionFunction::ADD;
    var->actions[index].parameter=0;
    var->actions[index].repurposeTrimIndex=-1;
    var->actionCount++;
    updateVarActionsUi();
    modelManager.requestSaveActiveModel();
}
void action_vars_function_show(lv_event_t *e){
    if(!e||!objects.vars_function_panel) return;
    VarConfig *var=getEditingVar();
    if(!var) return;
    uint8_t oneBased=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(oneBased<1||oneBased>var->actionCount) return;
    editingVarFunctionActionIndex=(int8_t)(oneBased-1);

    lv_obj_t *items[]={
        objects.funtion_assign_panel,objects.funtion_add_panel,objects.funtion_substract_panel,
        objects.funtion_multiply_panel,objects.funtion_divide_panel,objects.funtion_percent_panel,
        objects.funtion_min_panel,objects.funtion_max_panel,objects.funtion_repurpose_panel
    };
    for(lv_obj_t *item:items){
        if(!item) continue;
        lv_obj_remove_flag(item,LV_OBJ_FLAG_CHECKABLE);
        lv_obj_remove_state(item,LV_STATE_CHECKED);
        lv_obj_remove_state(item,LV_STATE_FOCUSED);
        lv_obj_remove_event_cb(item,exclusiveSelectionPressed);
        lv_obj_add_event_cb(item,exclusiveSelectionPressed,LV_EVENT_PRESSED,nullptr);
    }

    lv_obj_remove_flag(objects.vars_function_panel,LV_OBJ_FLAG_HIDDEN);
    showPopupDismissOverlay(objects.vars_function_panel);
}
void action_vars_function_selected(lv_event_t *e){
    if(!e) return;
    VarConfig *var=getEditingVar();
    if(!var||editingVarFunctionActionIndex<0||editingVarFunctionActionIndex>=var->actionCount) return;
    uint8_t function=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(function>=(uint8_t)VarActionFunction::REPURPOSE+1) return;
    VarActionConfig &action=var->actions[(uint8_t)editingVarFunctionActionIndex];
    action.function=(VarActionFunction)function;
    if(action.function==VarActionFunction::REPURPOSE&&action.repurposeTrimIndex<0) action.repurposeTrimIndex=0;
    if(objects.vars_function_panel) lv_obj_add_flag(objects.vars_function_panel,LV_OBJ_FLAG_HIDDEN);
    hidePopupDismissOverlay();
    updateVarActionsUi();
    modelManager.requestSaveActiveModel();
}
void action_vars_trim_show(lv_event_t *e){
    if(!e||!objects.vars_trim_type_select_panel) return;
    VarConfig *var=getEditingVar();
    if(!var) return;
    uint8_t oneBased=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(oneBased<1||oneBased>var->actionCount) return;
    editingVarActionIndex=(int8_t)(oneBased-1);
    trimSelectContext=TrimSelectContext::VAR_REPURPOSE;
    lv_obj_remove_flag(objects.vars_trim_type_select_panel,LV_OBJ_FLAG_HIDDEN);
    showPopupDismissOverlay(objects.vars_trim_type_select_panel);
}

// Called from loop() with LVGL on the same task. Refresh only the visible
// runtime value; changing Action settings never causes a Flash write here.
void update_vars_runtime_display(){
    // UI refresh is independent of the VarProcessor runtime cadence.
    if(lv_screen_active()!=objects.scr_vars) return;
    const bool listVisible=objects.vars_main_panel&&!lv_obj_has_flag(objects.vars_main_panel,LV_OBJ_FLAG_HIDDEN);
    const bool editorVisible=objects.vars_addedit_main_panel&&!lv_obj_has_flag(objects.vars_addedit_main_panel,LV_OBJ_FLAG_HIDDEN);
    if(!listVisible&&!editorVisible) return;

    static uint32_t lastUpdate=0;
    const uint32_t now=millis();
    if((uint32_t)(now-lastUpdate)<100U) return;
    lastUpdate=now;

    if(listVisible){
        const ModelConfig &model=modelManager.getActiveModel();
        for(uint8_t i=0;i<MAX_MODEL_VARS;i++){
            lv_obj_t *label=dynamicVarValueLabels[i];
            if(!label||!model.vars[i].enabled) continue;
            // Avoid work on scrolled-out rows; they will refresh when visible.
            if(!dynamicVarRows[i]) continue;
            lv_area_t rowArea,viewport;
            lv_obj_get_coords(dynamicVarRows[i],&rowArea);
            lv_obj_get_coords(objects.vars_list_panel,&viewport);
            if(rowArea.y2<viewport.y1||rowArea.y1>viewport.y2) continue;
            const int16_t current=varProcessor.getValue(i);
            if(lastRenderedVarValues[i]==current) continue;
            lastRenderedVarValues[i]=current;
            char formatted[24];
            const int32_t absValue=abs((int)current);
            snprintf(formatted,sizeof(formatted),"%s%ld.%ld%%",current<0?"-":"",(long)(absValue/10),(long)(absValue%10));
            if(strcmp(lv_label_get_text(label),formatted)!=0) lv_label_set_text(label,formatted);
        }
    }

    if(!editorVisible||editingVarIndex<0||editingVarIndex>=MAX_MODEL_VARS||!objects.vars_value_lbl) return;
    const int16_t current=varProcessor.getValue((uint8_t)editingVarIndex);
    // The header and every Value row can change independently. A Var Action
    // updates the active Value itself, not only the summary at the top.
    const ModelConfig &model=modelManager.getActiveModel();
    const VarConfig &var=model.vars[(uint8_t)editingVarIndex];
    const uint8_t count=var.valueCount<MAX_VAR_VALUES?var.valueCount:MAX_VAR_VALUES;
    for(uint8_t slot=0;slot<count;slot++){
        if(!var.values[slot].enabled) continue;
        lv_obj_t *label=getVarValueLabel(slot);
        if(!label) continue;
        const int16_t runtimeValue=varProcessor.getSlotValue((uint8_t)editingVarIndex,slot);
        char slotText[24];
        const int32_t magnitude=abs((int)runtimeValue);
        snprintf(slotText,sizeof(slotText),"%s%ld.%ld%%",runtimeValue<0?"-":"",(long)(magnitude/10),(long)(magnitude%10));
        if(strcmp(lv_label_get_text(label),slotText)!=0) lv_label_set_text(label,slotText);
    }
    if(lastRenderedEditorVar==editingVarIndex&&lastRenderedEditorValue==current) return;
    lastRenderedEditorVar=editingVarIndex;
    lastRenderedEditorValue=current;
    char formatted[24];
    const int32_t absValue=abs((int)current);
    snprintf(formatted,sizeof(formatted),"%s%ld.%ld%%",current<0?"-":"",(long)(absValue/10),(long)(absValue%10));
    if(strcmp(lv_label_get_text(objects.vars_value_lbl),formatted)!=0) lv_label_set_text(objects.vars_value_lbl,formatted);
}

// EEZ Studio binds this callback to hw_wifi_switch (LV_EVENT_VALUE_CHANGED).
void action_wifi_on_off(lv_event_t *e){
    if (e == NULL) return;
    lv_obj_t *sw = lv_event_get_target_obj(e);
    if (sw == NULL) return;
    bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    WiFi.mode(enabled ? WIFI_STA : WIFI_OFF);
}

extern "C" void action_mixes_trim_on_off(lv_event_t *e){
    aileronTrimChanged(e);
}
