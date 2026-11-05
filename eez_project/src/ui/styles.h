#ifndef EEZ_LVGL_UI_STYLES_H
#define EEZ_LVGL_UI_STYLES_H

#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Style: style_screen
lv_style_t *get_style_style_screen_MAIN_DEFAULT();
void add_style_style_screen(lv_obj_t *obj);
void remove_style_style_screen(lv_obj_t *obj);

// Style: style_topbar
lv_style_t *get_style_style_topbar_MAIN_DEFAULT();
void add_style_style_topbar(lv_obj_t *obj);
void remove_style_style_topbar(lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_STYLES_H*/