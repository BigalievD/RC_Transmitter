#ifndef EEZ_LVGL_UI_FONTS_H
#define EEZ_LVGL_UI_FONTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_font_t ui_font_arial28;
extern const lv_font_t ui_font_arial32;
extern const lv_font_t ui_font_arial18;
extern const lv_font_t ui_font_arial14;
extern const lv_font_t ui_font_icons16;
extern const lv_font_t ui_font_icons18;
extern const lv_font_t ui_font_icons20;
extern const lv_font_t ui_font_icons28;
extern const lv_font_t ui_font_icons32;
extern const lv_font_t ui_font_icons48;
extern const lv_font_t ui_font_icons56;
extern const lv_font_t ui_font_arrow;
extern const lv_font_t ui_font_switches;

#ifndef EXT_FONT_DESC_T
#define EXT_FONT_DESC_T
typedef struct _ext_font_desc_t {
    const char *name;
    const void *font_ptr;
} ext_font_desc_t;
#endif

extern ext_font_desc_t fonts[];

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_FONTS_H*/