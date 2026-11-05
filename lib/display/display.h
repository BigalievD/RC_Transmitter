#pragma once

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

/* Display resolution of this Waveshare 4.3" 480x800 ST7701 panel */
#define LCD_H_RES 480
#define LCD_V_RES 800

/*
 * Initialize the MIPI-DSI bus, the ST7701 panel and the LEDC backlight.
 * Must be called before lvgl_port_init().
 */
void display_init(void);

/* Handles owned by the display module, used by lvgl_port and touch. */
esp_lcd_panel_handle_t   display_get_panel_handle(void);
esp_lcd_panel_io_handle_t display_get_io_handle(void);

/* Backlight brightness in percent (0..100). Pin 26, active LOW. */
void display_set_brightness(int percent);