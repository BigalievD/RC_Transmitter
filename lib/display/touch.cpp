/*
 * GT911 capacitive touch for the Waveshare 4.3" P4 board.
 *
 * I2C (new esp-idf master driver): SDA=7, SCL=8, port=1, 400kHz.
 * GT911 probed at 0x5D then 0x14. Reset pin 23, INT not connected.
 * Registered with LVGL through esp_lvgl_adapter (read_cb polls it).
 */
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "driver/i2c_master.h"
#include "hal/gpio_types.h"

#include "display.h"
#include "lvgl_port.h"
#include "touch.h"

#define TAG "touch"

#define TP_I2C_PORT      1
#define TP_I2C_CLK       400000
#define TP_I2C_SDA       GPIO_NUM_7
#define TP_I2C_SCL       GPIO_NUM_8
#define TP_RST_PIN       GPIO_NUM_23
#define TP_INT_PIN       GPIO_NUM_NC

/*
 * Rotation flags for the touch controller. The GT911 reports raw coordinates
 * in the physical panel space (480x800), but LVGL works in the rotated logical
 * space (e.g. 800x480 for ROTATE_90). The adapter rotates the image during
 * flush but does NOT touch coordinates, so we must transform them here.
 *
 * The mapping below was tuned for THIS board (Waveshare P4 4.3"): the adapter's
 * default MIPI-DSI helper produced mirrored coordinates, so for ROTATE_90 we
 * use the mirrored variant (x_mirror=true, y_mirror=false) instead.
 */
static void touch_get_rotation_flags(esp_lv_adapter_rotation_t rotation,
                                     bool *swap_xy, bool *mirror_x, bool *mirror_y)
{
    bool swap = false;
    bool x_mirror = false;
    bool y_mirror = false;

    switch (rotation) {
    case ESP_LV_ADAPTER_ROTATE_90:
        swap = true;
        x_mirror = true;
        y_mirror = false;
        break;
    case ESP_LV_ADAPTER_ROTATE_180:
        swap = false;
        x_mirror = true;
        y_mirror = true;
        break;
    case ESP_LV_ADAPTER_ROTATE_270:
        swap = true;
        x_mirror = false;
        y_mirror = true;
        break;
    default: /* ROTATE_0 */
        swap = false;
        x_mirror = false;
        y_mirror = false;
        break;
    }

    *swap_xy = swap;
    *mirror_x = x_mirror;
    *mirror_y = y_mirror;
}

void touch_init(void)
{
    /* ---- I2C master bus ---- */
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = TP_I2C_PORT,
        .sda_io_num = TP_I2C_SDA,
        .scl_io_num = TP_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = { .enable_internal_pullup = true },
    };
    i2c_master_bus_handle_t bus = NULL;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    /* ---- probe GT911 at 0x5D, fall back to 0x14 ---- */
    /* Built manually instead of ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG(): that macro
       initializes scl_speed_hz out of declaration order, which is an error in C++ */
    esp_lcd_panel_io_i2c_config_t tp_io_config = {
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS,
        .control_phase_bytes = 1,
        .dc_bit_offset = 0,
        .lcd_cmd_bits = 16,
        .flags = { .disable_control_phase = 1 },
        .scl_speed_hz = TP_I2C_CLK,
    };
    if (i2c_master_probe(bus, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS, 100) == ESP_OK)
    {
        ESP_LOGI(TAG, "GT911 found at 0x%02X", ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS);
    }
    else if (i2c_master_probe(bus, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP, 100) == ESP_OK)
    {
        ESP_LOGI(TAG, "GT911 found at 0x%02X", ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP);
        tp_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP;
    }
    else
    {
        ESP_LOGE(TAG, "GT911 not found on I2C bus");
        return;
    }

    /* ---- touch panel I/O over I2C (v2 overload, takes the master-bus handle) ---- */
    esp_lcd_panel_io_handle_t tp_io = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(bus, &tp_io_config, &tp_io));

    /* ---- GT911 driver ---- */
    bool swap_xy = false, mirror_x = false, mirror_y = false;
    touch_get_rotation_flags(DISPLAY_ROTATION, &swap_xy, &mirror_x, &mirror_y);

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = TP_RST_PIN,
        .int_gpio_num = TP_INT_PIN,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
    };
    esp_lcd_touch_handle_t tp = NULL;
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, &tp));

    /* ---- register with LVGL ---- */
    esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(lvgl_port_get_display(), tp);
    lv_indev_t *indev = esp_lv_adapter_register_touch(&touch_cfg);
    ESP_ERROR_CHECK(indev == NULL ? ESP_FAIL : ESP_OK);

    ESP_LOGI(TAG, "GT911 touch registered");
}