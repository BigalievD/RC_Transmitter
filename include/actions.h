#ifndef ACTIONS_H
#define ACTIONS_H

#include <stdint.h>
#include "ui/ui.h"
#include "battery.h"

struct ChannelConfig;

// ============================================================================
// Типы редактора каналов
// ============================================================================
enum FieldIndex {
    FIELD_MIN = 0,
    FIELD_CENTER = 1,
    FIELD_MAX = 2,
    FIELD_EXPO_CURVE = 3,
    FIELD_SLOW_UP = 4,
    FIELD_SLOW_DOWN = 5
};

struct ChannelEditData {
    float min_val;
    float center_val;
    float max_val;
    float expo_curve;
    float slow_up;
    float slow_down;
};

// ============================================================================
// Общие типы changer-панелей
// ============================================================================
struct StepConfig {
    const char *options;
    uint32_t max_idx;
    float steps[3];
};

enum StepGroup {
    STEP_GROUP_LIMIT,
    STEP_GROUP_EXPO,
    STEP_GROUP_SLOW,
    STEP_GROUP_PWM
};

// ============================================================================
// Типы редактора кривых
// ============================================================================
enum CurveEditField {
    CURVE_EDIT_NONE,
    CURVE_EDIT_EXPO_WEIGHT,
    CURVE_EDIT_EXPO_OFFSET,
    CURVE_EDIT_EXPO_EXPO,
    CURVE_EDIT_FUNCTION_OFFSET,
    CURVE_EDIT_CUSTOM_OFFSET,
    CURVE_EDIT_POINT_X,
    CURVE_EDIT_POINT_Y
};

// ============================================================================
// Глобальные данные редактора каналов
// ============================================================================
extern FieldIndex active_field;
extern ChannelEditData ch_data;

// ============================================================================
// Публичные функции actions.cpp
// ============================================================================
float *get_active_field_ptr();
void get_current_field_bounds(float &min_out, float &max_out);
void update_active_field_ui();
void ads_init();
void update_icons();
void update_channels();
void update_vars_runtime_display();
void update_topbar_icon(int percent, int rssi);
void load_all_calibration_limits();
void load_channels_overview_ui();
void update_channels_overview_live();
void init_channel_widget_pointers();
void update_BB_widget_visibility(ScreensEnum screen_id);
void openScreen(ScreensEnum screen_id);
const char *getScreenTitle(ScreensEnum screen_id);
void goBack();
void loadThemeSettings();
void saveGeneralSettings();
float usToPercent(int us, int minUs = 988, int centerUs = 1500, int maxUs = 2012);
int percentToUs(float pct, int minUs = 988, int centerUs = 1500, int maxUs = 2012);
void updateBatteryUI(const BatteryStatus &battery);
void balanceEncoderLeft();
void balanceEncoderRight();
void balanceEncoderPressed();

#endif
