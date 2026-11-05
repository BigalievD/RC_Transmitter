#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_SCR_MAIN = 1,
    SCREEN_ID_SCR_CHANNELS = 2,
    SCREEN_ID_SCR_CH_SETTINGS = 3,
    SCREEN_ID_SCR_MODEL = 4,
    SCREEN_ID_SCR_SETTINGS = 5,
    SCREEN_ID_SCR_MODEL_SELECT = 6,
    SCREEN_ID_SCR_MODEL_SETUP = 7,
    SCREEN_ID_SCR_MIXER = 8,
    _SCREEN_ID_LAST = 8
};

typedef struct _objects_t {
    lv_obj_t *scr_main;
    lv_obj_t *scr_channels;
    lv_obj_t *scr_ch_settings;
    lv_obj_t *scr_model;
    lv_obj_t *scr_settings;
    lv_obj_t *scr_model_select;
    lv_obj_t *scr_model_setup;
    lv_obj_t *scr_mixer;
} objects_t;

extern objects_t objects;

void create_screen_scr_main();
void tick_screen_scr_main();

void create_screen_scr_channels();
void tick_screen_scr_channels();

void create_screen_scr_ch_settings();
void tick_screen_scr_ch_settings();

void create_screen_scr_model();
void tick_screen_scr_model();

void create_screen_scr_settings();
void tick_screen_scr_settings();

void create_screen_scr_model_select();
void tick_screen_scr_model_select();

void create_screen_scr_model_setup();
void tick_screen_scr_model_setup();

void create_screen_scr_mixer();
void tick_screen_scr_mixer();

void create_user_widget_topbar(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_topbar(int startWidgetIndex);

void create_user_widget_bottombar(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_bottombar(int startWidgetIndex);

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/