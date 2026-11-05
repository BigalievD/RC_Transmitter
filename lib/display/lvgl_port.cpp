/*
 * LVGL port: bind the ST7701 display to LVGL 9 via esp_lvgl_adapter.
 *
 * Architecture note: we deliberately do NOT call esp_lv_adapter_start(),
 * because the user wants loop() to drive LVGL via lv_timer_handler().
 * esp_lv_adapter_init() sets up the esp_timer tick + lv_init(); register_display
 * wires the render/flush callbacks. Touch is polled inside LVGL's read_cb.
 */
#include "esp_log.h"
#include "esp_lv_adapter.h"

#include "display.h"
#include "lvgl_port.h"

#define TAG "lvgl_port"

static lv_display_t *s_disp = NULL;

void lvgl_port_init(void)
{
    ESP_LOGI(TAG, "Init LVGL (esp_lvgl_adapter)");
    esp_lv_adapter_config_t cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(esp_lv_adapter_init(&cfg));

    /* MIPI-DSI panel: physical 480x800, logical orientation = DISPLAY_ROTATION.
       The adapter creates the LVGL display at the rotated logical resolution and
       rotates the image during flush, so touch must be rotated too (see touch.cpp). */
    esp_lv_adapter_display_config_t disp_cfg =
        ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(display_get_panel_handle(),
                                                   display_get_io_handle(),
                                                   LCD_H_RES, LCD_V_RES,
                                                   DISPLAY_ROTATION);
    disp_cfg.profile.buffer_height = 40;
    disp_cfg.profile.use_psram = true;
    disp_cfg.profile.enable_ppa_accel = false;
    disp_cfg.profile.require_double_buffer = false;
    s_disp = esp_lv_adapter_register_display(&disp_cfg);
    ESP_ERROR_CHECK(s_disp == NULL ? ESP_FAIL : ESP_OK);
    ESP_LOGI(TAG, "LVGL display registered (%dx%d)", LCD_H_RES, LCD_V_RES);
}

lv_display_t *lvgl_port_get_display(void)
{
    return s_disp;
}