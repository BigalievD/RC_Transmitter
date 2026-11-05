#include "styles.h"
#include "images.h"
#include "fonts.h"

#include "ui.h"
#include "screens.h"

//
// Style: style_channel_bar
//

void init_style_style_channel_bar_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][17]));
    lv_style_set_radius(style, 0);
    lv_style_set_bg_opa(style, 255);
    lv_style_set_border_color(style, lv_color_hex(theme_colors[active_theme_index][12]));
    lv_style_set_border_width(style, 1);
};

lv_style_t *get_style_style_channel_bar_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_style_channel_bar_MAIN_DEFAULT(style);
    }
    return style;
};

void init_style_style_channel_bar_INDICATOR_DEFAULT(lv_style_t *style) {
    lv_style_set_radius(style, 0);
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][3]));
    lv_style_set_border_color(style, lv_color_hex(theme_colors[active_theme_index][12]));
    lv_style_set_border_width(style, 1);
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][9]));
    lv_style_set_text_font(style, &ui_font_arial28);
};

lv_style_t *get_style_style_channel_bar_INDICATOR_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_style_channel_bar_INDICATOR_DEFAULT(style);
    }
    return style;
};

void add_style_style_channel_bar(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_style_channel_bar_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_style_channel_bar_INDICATOR_DEFAULT(), LV_PART_INDICATOR | LV_STATE_DEFAULT);
};

void remove_style_style_channel_bar(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_style_channel_bar_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_style_channel_bar_INDICATOR_DEFAULT(), LV_PART_INDICATOR | LV_STATE_DEFAULT);
};

//
// Style: mx_bar
//

void init_style_mx_bar_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][17]));
    lv_style_set_radius(style, 0);
    lv_style_set_bg_opa(style, 255);
    lv_style_set_border_width(style, 1);
    lv_style_set_border_color(style, lv_color_hex(theme_colors[active_theme_index][12]));
};

lv_style_t *get_style_mx_bar_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_mx_bar_MAIN_DEFAULT(style);
    }
    return style;
};

void init_style_mx_bar_INDICATOR_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][14]));
    lv_style_set_radius(style, 0);
    lv_style_set_border_color(style, lv_color_hex(theme_colors[active_theme_index][12]));
    lv_style_set_border_width(style, 1);
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][9]));
    lv_style_set_text_font(style, &ui_font_arial28);
};

lv_style_t *get_style_mx_bar_INDICATOR_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_mx_bar_INDICATOR_DEFAULT(style);
    }
    return style;
};

void add_style_mx_bar(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_mx_bar_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_mx_bar_INDICATOR_DEFAULT(), LV_PART_INDICATOR | LV_STATE_DEFAULT);
};

void remove_style_mx_bar(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_mx_bar_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_mx_bar_INDICATOR_DEFAULT(), LV_PART_INDICATOR | LV_STATE_DEFAULT);
};

//
// Style: microsec
//

void init_style_microsec_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_font(style, &ui_font_arial28);
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][3]));
};

lv_style_t *get_style_microsec_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_microsec_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_microsec(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_microsec_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_microsec(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_microsec_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: Good
//

void init_style_good_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][6]));
};

lv_style_t *get_style_good_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_good_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_good(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_good_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_good(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_good_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: Excellent
//

void init_style_excellent_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][5]));
};

lv_style_t *get_style_excellent_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_excellent_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_excellent(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_excellent_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_excellent(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_excellent_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: Bad
//

void init_style_bad_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][8]));
};

lv_style_t *get_style_bad_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_bad_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_bad(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_bad_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_bad(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_bad_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: Label32
//

void init_style_label32_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_font(style, &ui_font_arial32);
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][16]));
};

lv_style_t *get_style_label32_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_label32_MAIN_DEFAULT(style);
    }
    return style;
};

void init_style_label32_MAIN_CHECKED(lv_style_t *style) {
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][9]));
};

lv_style_t *get_style_label32_MAIN_CHECKED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_label32_MAIN_CHECKED(style);
    }
    return style;
};

void add_style_label32(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_label32_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_label32_MAIN_CHECKED(), LV_PART_MAIN | LV_STATE_CHECKED);
};

void remove_style_label32(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_label32_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_label32_MAIN_CHECKED(), LV_PART_MAIN | LV_STATE_CHECKED);
};

//
// Style: edit32
//

void init_style_edit32_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][9]));
    lv_style_set_bg_opa(style, 255);
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][16]));
    lv_style_set_text_font(style, &ui_font_arial32);
    lv_style_set_pad_top(style, 0);
    lv_style_set_pad_bottom(style, 0);
    lv_style_set_pad_left(style, 0);
    lv_style_set_text_align(style, LV_TEXT_ALIGN_RIGHT);
    lv_style_set_pad_right(style, 0);
    lv_style_set_border_color(style, lv_color_hex(theme_colors[active_theme_index][12]));
    lv_style_set_border_width(style, 2);
};

lv_style_t *get_style_edit32_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_edit32_MAIN_DEFAULT(style);
    }
    return style;
};

void init_style_edit32_MAIN_CHECKED(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][3]));
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][9]));
};

lv_style_t *get_style_edit32_MAIN_CHECKED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_edit32_MAIN_CHECKED(style);
    }
    return style;
};

void add_style_edit32(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_edit32_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_edit32_MAIN_CHECKED(), LV_PART_MAIN | LV_STATE_CHECKED);
};

void remove_style_edit32(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_edit32_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_edit32_MAIN_CHECKED(), LV_PART_MAIN | LV_STATE_CHECKED);
};

//
// Style: Switch1
//

void init_style_switch1_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][11]));
};

lv_style_t *get_style_switch1_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_switch1_MAIN_DEFAULT(style);
    }
    return style;
};

void init_style_switch1_MAIN_FOCUSED(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][11]));
};

lv_style_t *get_style_switch1_MAIN_FOCUSED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_switch1_MAIN_FOCUSED(style);
    }
    return style;
};

void init_style_switch1_KNOB_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][16]));
};

lv_style_t *get_style_switch1_KNOB_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_switch1_KNOB_DEFAULT(style);
    }
    return style;
};

void init_style_switch1_KNOB_FOCUSED(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][3]));
};

lv_style_t *get_style_switch1_KNOB_FOCUSED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_switch1_KNOB_FOCUSED(style);
    }
    return style;
};

void init_style_switch1_INDICATOR_CHECKED(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][11]));
};

lv_style_t *get_style_switch1_INDICATOR_CHECKED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_switch1_INDICATOR_CHECKED(style);
    }
    return style;
};

void add_style_switch1(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_switch1_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_switch1_MAIN_FOCUSED(), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_add_style(obj, get_style_switch1_KNOB_DEFAULT(), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_switch1_KNOB_FOCUSED(), LV_PART_KNOB | LV_STATE_FOCUSED);
    lv_obj_add_style(obj, get_style_switch1_INDICATOR_CHECKED(), LV_PART_INDICATOR | LV_STATE_CHECKED);
};

void remove_style_switch1(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_switch1_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_switch1_MAIN_FOCUSED(), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_remove_style(obj, get_style_switch1_KNOB_DEFAULT(), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_switch1_KNOB_FOCUSED(), LV_PART_KNOB | LV_STATE_FOCUSED);
    lv_obj_remove_style(obj, get_style_switch1_INDICATOR_CHECKED(), LV_PART_INDICATOR | LV_STATE_CHECKED);
};

//
// Style: Label12
//

void init_style_label12_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_color(style, lv_color_hex(0xffffff));
    lv_style_set_text_font(style, &lv_font_montserrat_14);
};

lv_style_t *get_style_label12_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_label12_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_label12(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_label12_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_label12(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_label12_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: button1
//

void init_style_button1_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_radius(style, 0);
    lv_style_set_border_color(style, lv_color_hex(theme_colors[active_theme_index][9]));
    lv_style_set_border_width(style, 1);
    lv_style_set_border_side(style, LV_BORDER_SIDE_RIGHT);
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][10]));
};

lv_style_t *get_style_button1_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_button1_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_button1(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_button1_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_button1(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_button1_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: Container1
//

void init_style_container1_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][10]));
    lv_style_set_bg_opa(style, 255);
    lv_style_set_border_color(style, lv_color_hex(theme_colors[active_theme_index][12]));
    lv_style_set_border_width(style, 1);
    lv_style_set_radius(style, 12);
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][16]));
    lv_style_set_text_font(style, &ui_font_arial28);
};

lv_style_t *get_style_container1_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_container1_MAIN_DEFAULT(style);
    }
    return style;
};

void init_style_container1_MAIN_CHECKED(lv_style_t *style) {
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][9]));
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][3]));
    lv_style_set_text_font(style, &ui_font_arial32);
};

lv_style_t *get_style_container1_MAIN_CHECKED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_container1_MAIN_CHECKED(style);
    }
    return style;
};

void init_style_container1_MAIN_DISABLED(lv_style_t *style) {
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][9]));
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][12]));
    lv_style_set_text_font(style, &ui_font_arial32);
};

lv_style_t *get_style_container1_MAIN_DISABLED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_container1_MAIN_DISABLED(style);
    }
    return style;
};

void init_style_container1_SCROLLBAR_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][3]));
    lv_style_set_bg_opa(style, 255);
};

lv_style_t *get_style_container1_SCROLLBAR_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_container1_SCROLLBAR_DEFAULT(style);
    }
    return style;
};

void add_style_container1(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_container1_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_container1_MAIN_CHECKED(), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_add_style(obj, get_style_container1_MAIN_DISABLED(), LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_add_style(obj, get_style_container1_SCROLLBAR_DEFAULT(), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
};

void remove_style_container1(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_container1_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_container1_MAIN_CHECKED(), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_remove_style(obj, get_style_container1_MAIN_DISABLED(), LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_remove_style(obj, get_style_container1_SCROLLBAR_DEFAULT(), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
};

//
// Style: Container1_1
//

void init_style_container1_1_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_border_color(style, lv_color_hex(theme_colors[active_theme_index][12]));
    lv_style_set_border_width(style, 2);
    lv_style_set_radius(style, 12);
    lv_style_set_bg_opa(style, 255);
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][16]));
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][11]));
    lv_style_set_text_font(style, &ui_font_arial28);
};

lv_style_t *get_style_container1_1_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_container1_1_MAIN_DEFAULT(style);
    }
    return style;
};

void init_style_container1_1_MAIN_CHECKED(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][3]));
    lv_style_set_bg_opa(style, 255);
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][9]));
};

lv_style_t *get_style_container1_1_MAIN_CHECKED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_container1_1_MAIN_CHECKED(style);
    }
    return style;
};

void init_style_container1_1_SCROLLBAR_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_opa(style, 255);
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][3]));
};

lv_style_t *get_style_container1_1_SCROLLBAR_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_container1_1_SCROLLBAR_DEFAULT(style);
    }
    return style;
};

void add_style_container1_1(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_container1_1_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_container1_1_MAIN_CHECKED(), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_add_style(obj, get_style_container1_1_SCROLLBAR_DEFAULT(), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
};

void remove_style_container1_1(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_container1_1_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_container1_1_MAIN_CHECKED(), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_remove_style(obj, get_style_container1_1_SCROLLBAR_DEFAULT(), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
};

//
// Style: icons32
//

void init_style_icons32_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][16]));
    lv_style_set_text_font(style, &ui_font_icons32);
};

lv_style_t *get_style_icons32_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_icons32_MAIN_DEFAULT(style);
    }
    return style;
};

void init_style_icons32_MAIN_CHECKED(lv_style_t *style) {
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][9]));
};

lv_style_t *get_style_icons32_MAIN_CHECKED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_icons32_MAIN_CHECKED(style);
    }
    return style;
};

void add_style_icons32(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_icons32_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_icons32_MAIN_CHECKED(), LV_PART_MAIN | LV_STATE_CHECKED);
};

void remove_style_icons32(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_icons32_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_icons32_MAIN_CHECKED(), LV_PART_MAIN | LV_STATE_CHECKED);
};

//
// Style: Label18
//

void init_style_label18_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][16]));
    lv_style_set_text_font(style, &ui_font_arial18);
};

lv_style_t *get_style_label18_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_label18_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_label18(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_label18_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_label18(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_label18_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: Icons48
//

void init_style_icons48_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_color(style, lv_color_hex(0xffffff));
    lv_style_set_text_font(style, &ui_font_icons48);
};

lv_style_t *get_style_icons48_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_icons48_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_icons48(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_icons48_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_icons48(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_icons48_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: Icons56
//

void init_style_icons56_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_font(style, &ui_font_icons56);
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][16]));
};

lv_style_t *get_style_icons56_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_icons56_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_icons56(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_icons56_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_icons56(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_icons56_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: Divider
//

void init_style_divider_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_line_width(style, 1);
    lv_style_set_line_color(style, lv_color_hex(0xffffff));
};

lv_style_t *get_style_divider_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_divider_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_divider(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_divider_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_divider(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_divider_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: kayboard1
//

void init_style_kayboard1_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_border_color(style, lv_color_hex(theme_colors[active_theme_index][12]));
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][9]));
};

lv_style_t *get_style_kayboard1_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_kayboard1_MAIN_DEFAULT(style);
    }
    return style;
};

void init_style_kayboard1_ITEMS_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][11]));
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][3]));
    lv_style_set_text_font(style, &lv_font_montserrat_14);
};

lv_style_t *get_style_kayboard1_ITEMS_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_kayboard1_ITEMS_DEFAULT(style);
    }
    return style;
};

void init_style_kayboard1_ITEMS_CHECKED(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][12]));
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][3]));
};

lv_style_t *get_style_kayboard1_ITEMS_CHECKED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_kayboard1_ITEMS_CHECKED(style);
    }
    return style;
};

void add_style_kayboard1(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_kayboard1_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_kayboard1_ITEMS_DEFAULT(), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_kayboard1_ITEMS_CHECKED(), LV_PART_ITEMS | LV_STATE_CHECKED);
};

void remove_style_kayboard1(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_kayboard1_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_kayboard1_ITEMS_DEFAULT(), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_kayboard1_ITEMS_CHECKED(), LV_PART_ITEMS | LV_STATE_CHECKED);
};

//
// Style: Label14
//

void init_style_label14_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_font(style, &ui_font_arial14);
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][16]));
};

lv_style_t *get_style_label14_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_label14_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_label14(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_label14_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_label14(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_label14_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: Icons28
//

void init_style_icons28_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_color(style, lv_color_hex(0xffffff));
    lv_style_set_text_font(style, &ui_font_icons28);
};

lv_style_t *get_style_icons28_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_icons28_MAIN_DEFAULT(style);
    }
    return style;
};

void init_style_icons28_MAIN_DISABLED(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][12]));
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][9]));
    lv_style_set_text_font(style, &ui_font_arial32);
};

lv_style_t *get_style_icons28_MAIN_DISABLED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_icons28_MAIN_DISABLED(style);
    }
    return style;
};

void add_style_icons28(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_icons28_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_icons28_MAIN_DISABLED(), LV_PART_MAIN | LV_STATE_DISABLED);
};

void remove_style_icons28(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_icons28_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_icons28_MAIN_DISABLED(), LV_PART_MAIN | LV_STATE_DISABLED);
};

//
// Style: Label28
//

void init_style_label28_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_font(style, &ui_font_arial28);
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][16]));
};

lv_style_t *get_style_label28_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_label28_MAIN_DEFAULT(style);
    }
    return style;
};

void init_style_label28_MAIN_DISABLED(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][12]));
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][12]));
    lv_style_set_text_font(style, &ui_font_arial28);
};

lv_style_t *get_style_label28_MAIN_DISABLED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_label28_MAIN_DISABLED(style);
    }
    return style;
};

void init_style_label28_MAIN_CHECKED(lv_style_t *style) {
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][9]));
};

lv_style_t *get_style_label28_MAIN_CHECKED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_label28_MAIN_CHECKED(style);
    }
    return style;
};

void add_style_label28(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_label28_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_label28_MAIN_DISABLED(), LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_add_style(obj, get_style_label28_MAIN_CHECKED(), LV_PART_MAIN | LV_STATE_CHECKED);
};

void remove_style_label28(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_label28_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_label28_MAIN_DISABLED(), LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_remove_style(obj, get_style_label28_MAIN_CHECKED(), LV_PART_MAIN | LV_STATE_CHECKED);
};

//
//
//

void add_style(lv_obj_t *obj, int32_t styleIndex) {
    typedef void (*AddStyleFunc)(lv_obj_t *obj);
    static const AddStyleFunc add_style_funcs[] = {
        add_style_style_channel_bar,
        add_style_mx_bar,
        add_style_microsec,
        add_style_good,
        add_style_excellent,
        add_style_bad,
        add_style_label32,
        add_style_edit32,
        add_style_switch1,
        add_style_label12,
        add_style_button1,
        add_style_container1,
        add_style_container1_1,
        add_style_icons32,
        add_style_label18,
        add_style_icons48,
        add_style_icons56,
        add_style_divider,
        add_style_kayboard1,
        add_style_label14,
        add_style_icons28,
        add_style_label28,
    };
    add_style_funcs[styleIndex](obj);
}

void remove_style(lv_obj_t *obj, int32_t styleIndex) {
    typedef void (*RemoveStyleFunc)(lv_obj_t *obj);
    static const RemoveStyleFunc remove_style_funcs[] = {
        remove_style_style_channel_bar,
        remove_style_mx_bar,
        remove_style_microsec,
        remove_style_good,
        remove_style_excellent,
        remove_style_bad,
        remove_style_label32,
        remove_style_edit32,
        remove_style_switch1,
        remove_style_label12,
        remove_style_button1,
        remove_style_container1,
        remove_style_container1_1,
        remove_style_icons32,
        remove_style_label18,
        remove_style_icons48,
        remove_style_icons56,
        remove_style_divider,
        remove_style_kayboard1,
        remove_style_label14,
        remove_style_icons28,
        remove_style_label28,
    };
    remove_style_funcs[styleIndex](obj);
}