#pragma once

/*
 * Initialize the GT911 touch controller (I2C) and register it with LVGL.
 * Must be called after lvgl_port_init().
 */
void touch_init(void);