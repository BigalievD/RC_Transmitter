#ifndef EEZ_LVGL_UI_STYLES_H
#define EEZ_LVGL_UI_STYLES_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Style: style_channel_bar
lv_style_t *get_style_style_channel_bar_MAIN_DEFAULT();
lv_style_t *get_style_style_channel_bar_INDICATOR_DEFAULT();
void add_style_style_channel_bar(lv_obj_t *obj);
void remove_style_style_channel_bar(lv_obj_t *obj);

// Style: mx_bar
lv_style_t *get_style_mx_bar_MAIN_DEFAULT();
lv_style_t *get_style_mx_bar_INDICATOR_DEFAULT();
void add_style_mx_bar(lv_obj_t *obj);
void remove_style_mx_bar(lv_obj_t *obj);

// Style: microsec
lv_style_t *get_style_microsec_MAIN_DEFAULT();
void add_style_microsec(lv_obj_t *obj);
void remove_style_microsec(lv_obj_t *obj);

// Style: Good
lv_style_t *get_style_good_MAIN_DEFAULT();
void add_style_good(lv_obj_t *obj);
void remove_style_good(lv_obj_t *obj);

// Style: Excellent
lv_style_t *get_style_excellent_MAIN_DEFAULT();
void add_style_excellent(lv_obj_t *obj);
void remove_style_excellent(lv_obj_t *obj);

// Style: Bad
lv_style_t *get_style_bad_MAIN_DEFAULT();
void add_style_bad(lv_obj_t *obj);
void remove_style_bad(lv_obj_t *obj);

// Style: Label32
lv_style_t *get_style_label32_MAIN_DEFAULT();
lv_style_t *get_style_label32_MAIN_CHECKED();
void add_style_label32(lv_obj_t *obj);
void remove_style_label32(lv_obj_t *obj);

// Style: edit32
lv_style_t *get_style_edit32_MAIN_DEFAULT();
lv_style_t *get_style_edit32_MAIN_CHECKED();
void add_style_edit32(lv_obj_t *obj);
void remove_style_edit32(lv_obj_t *obj);

// Style: Switch1
lv_style_t *get_style_switch1_MAIN_DEFAULT();
lv_style_t *get_style_switch1_MAIN_FOCUSED();
lv_style_t *get_style_switch1_KNOB_DEFAULT();
lv_style_t *get_style_switch1_KNOB_FOCUSED();
lv_style_t *get_style_switch1_INDICATOR_CHECKED();
void add_style_switch1(lv_obj_t *obj);
void remove_style_switch1(lv_obj_t *obj);

// Style: Label12
lv_style_t *get_style_label12_MAIN_DEFAULT();
void add_style_label12(lv_obj_t *obj);
void remove_style_label12(lv_obj_t *obj);

// Style: button1
lv_style_t *get_style_button1_MAIN_DEFAULT();
void add_style_button1(lv_obj_t *obj);
void remove_style_button1(lv_obj_t *obj);

// Style: Container1
lv_style_t *get_style_container1_MAIN_DEFAULT();
lv_style_t *get_style_container1_MAIN_CHECKED();
lv_style_t *get_style_container1_MAIN_DISABLED();
lv_style_t *get_style_container1_SCROLLBAR_DEFAULT();
void add_style_container1(lv_obj_t *obj);
void remove_style_container1(lv_obj_t *obj);

// Style: Container1_1
lv_style_t *get_style_container1_1_MAIN_DEFAULT();
lv_style_t *get_style_container1_1_MAIN_CHECKED();
lv_style_t *get_style_container1_1_SCROLLBAR_DEFAULT();
void add_style_container1_1(lv_obj_t *obj);
void remove_style_container1_1(lv_obj_t *obj);

// Style: icons32
lv_style_t *get_style_icons32_MAIN_DEFAULT();
lv_style_t *get_style_icons32_MAIN_CHECKED();
void add_style_icons32(lv_obj_t *obj);
void remove_style_icons32(lv_obj_t *obj);

// Style: Label18
lv_style_t *get_style_label18_MAIN_DEFAULT();
void add_style_label18(lv_obj_t *obj);
void remove_style_label18(lv_obj_t *obj);

// Style: Icons48
lv_style_t *get_style_icons48_MAIN_DEFAULT();
void add_style_icons48(lv_obj_t *obj);
void remove_style_icons48(lv_obj_t *obj);

// Style: Icons56
lv_style_t *get_style_icons56_MAIN_DEFAULT();
void add_style_icons56(lv_obj_t *obj);
void remove_style_icons56(lv_obj_t *obj);

// Style: Divider
lv_style_t *get_style_divider_MAIN_DEFAULT();
void add_style_divider(lv_obj_t *obj);
void remove_style_divider(lv_obj_t *obj);

// Style: kayboard1
lv_style_t *get_style_kayboard1_MAIN_DEFAULT();
lv_style_t *get_style_kayboard1_ITEMS_DEFAULT();
lv_style_t *get_style_kayboard1_ITEMS_CHECKED();
void add_style_kayboard1(lv_obj_t *obj);
void remove_style_kayboard1(lv_obj_t *obj);

// Style: Label14
lv_style_t *get_style_label14_MAIN_DEFAULT();
void add_style_label14(lv_obj_t *obj);
void remove_style_label14(lv_obj_t *obj);

// Style: Icons28
lv_style_t *get_style_icons28_MAIN_DEFAULT();
lv_style_t *get_style_icons28_MAIN_DISABLED();
void add_style_icons28(lv_obj_t *obj);
void remove_style_icons28(lv_obj_t *obj);

// Style: Label28
lv_style_t *get_style_label28_MAIN_DEFAULT();
lv_style_t *get_style_label28_MAIN_DISABLED();
lv_style_t *get_style_label28_MAIN_CHECKED();
void add_style_label28(lv_obj_t *obj);
void remove_style_label28(lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_STYLES_H*/