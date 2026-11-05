#pragma once

#include "lvgl.h"
#include "esp_lv_adapter_display.h"

/*
 * Display orientation in the UI (logical), independent of the physical panel
 * resolution (480x800). The adapter rotates the image during flush; touch
 * coordinates must be rotated to match (see touch.cpp).
 *   ROTATE_0   = portrait (native)
 *   ROTATE_90  = landscape
 *   ROTATE_270 = landscape (flipped)
 */
#define DISPLAY_ROTATION ESP_LV_ADAPTER_ROTATE_90

/*
 * Initialize the LVGL library and register the display with it.
 * Uses esp_lvgl_adapter (esp_lv_adapter_init + register_display).
 * Must be called after display_init(), before touch_init() and ui_init().
 */
void lvgl_port_init(void);

lv_display_t *lvgl_port_get_display(void);