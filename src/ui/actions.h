#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_action_goto_home(lv_event_t * e);
extern void action_action_goto_model(lv_event_t * e);
extern void action_action_goto_telemetry(lv_event_t * e);
extern void action_action_goto_settings(lv_event_t * e);
extern void action_action_edit_channel(lv_event_t * e);
extern void action_channels_screen_open(lv_event_t * e);
extern void action_action_start_calib(lv_event_t * e);
extern void action_action_calib_done(lv_event_t * e);
extern void action_action_calib_cancel(lv_event_t * e);
extern void action_action_calib_back(lv_event_t * e);
extern void action_action_calib_settings(lv_event_t * e);
extern void action_action_goto_calib(lv_event_t * e);
extern void action_goto_sd_card_screen(lv_event_t * e);
extern void action_open_scr_channel(lv_event_t * e);
extern void action_save_channel_edit(lv_event_t * e);
extern void action_goback(lv_event_t * e);
extern void action_show_changed_panel(lv_event_t * e);
extern void action_swipe_changer(lv_event_t * e);
extern void action_change_step(lv_event_t * e);
extern void action_do_step(lv_event_t * e);
extern void action_open_scr_trimm(lv_event_t * e);
extern void action_open_scr_general_settigs(lv_event_t * e);
extern void action_toggle_display_menu(lv_event_t * e);
extern void action_show_colorwheel_pallete(lv_event_t * e);
extern void action_theme_change(lv_event_t * e);
extern void action_change_brightness(lv_event_t * e);
extern void action_open_scr_mixes(lv_event_t * e);
extern void action_open_scr_model_select(lv_event_t * e);
extern void action_ch_curve_choice_panel_open(lv_event_t * e);
extern void action_selected_curve(lv_event_t * e);
extern void action_open_scr_curve(lv_event_t * e);
extern void action_select_curve(lv_event_t * e);
extern void action_set_curve_to(lv_event_t * e);
extern void action_add_curve(lv_event_t * e);
extern void action_open_points_picker(lv_event_t * e);
extern void action_open_function_selecter_panel(lv_event_t * e);
extern void action_set_point_count(lv_event_t * e);
extern void action_set_function_to(lv_event_t * e);
extern void action_edit_menu_clicked(lv_event_t * e);
extern void action_set_easymode(lv_event_t * e);
extern void action_show_curve_changed_panel(lv_event_t * e);
extern void action_set_curve_smooth(lv_event_t * e);
extern void action_edit_output_curve(lv_event_t * e);
extern void action_add_edit_curve_balance(lv_event_t * e);
extern void action_delete_menu_clicked(lv_event_t * e);
extern void action_channels_list_show(lv_event_t * e);
extern void action_add_mix(lv_event_t * e);
extern void action_mix_type_select(lv_event_t * e);
extern void action_mix_add_new_action(lv_event_t * e);
extern void action_mix_action_type_panel_show(lv_event_t * e);
extern void action_mix_action_type_selected(lv_event_t * e);
extern void action_mix_add_new_weight(lv_event_t * e);
extern void action_mix_del_weight(lv_event_t * e);
extern void action_mix_add_new_curve(lv_event_t * e);
extern void action_mix_del_curve(lv_event_t * e);
extern void action_trim_type_selected(lv_event_t * e);
extern void action_mix_trim_type_panel_show(lv_event_t * e);
extern void action_mix_action_value_edit(lv_event_t * e);
extern void action_source_selector_show(lv_event_t * e);
extern void action_operation_panel_show(lv_event_t * e);
extern void action_operation_selected(lv_event_t * e);
extern void action_set_invert(lv_event_t * e);
extern void action_invert_panel_show(lv_event_t * e);
extern void action_set_half_range_ignore_range(lv_event_t * e);
extern void action_add_var(lv_event_t * e);
extern void action_var_add_value(lv_event_t * e);
extern void action_var_add_actions(lv_event_t * e);
extern void action_vars_del_value(lv_event_t * e);
extern void action_vars_del_actions(lv_event_t * e);
extern void action_vars_condition_show(lv_event_t * e);
extern void action_vars_function_show(lv_event_t * e);
extern void action_vars_function_selected(lv_event_t * e);
extern void action_vars_trim_show(lv_event_t * e);
extern void action_open_scr_vars(lv_event_t * e);
extern void action_open_scr_hw(lv_event_t * e);
extern void action_wifi_on_off(lv_event_t * e);
extern void action_mixes_trim_on_off(lv_event_t * e);
extern void action_show_stick_mode_select_panel(lv_event_t * e);
extern void action_show_co_select_panel(lv_event_t * e);
extern void action_f4chf_on_off(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/