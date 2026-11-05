/*
 * Waveshare ESP32-P4-WIFI6-Touch-LCD 4.3" display driver.
 * MIPI-DSI (2-lane) + ST7701 panel + LEDC backlight (GPIO 26, active LOW).
 *
 * Init command sequence is board-specific and copied verbatim from the
 * official Waveshare BSP.
 */
#include <driver/ledc.h>
#include <esp_check.h>
#include <esp_ldo_regulator.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_st7701.h>
#include <esp_log.h>
#include <string.h>

#include "display.h"

#define TAG "display"

/* ----- pin / timing constants (from Waveshare BSP) ------------------------- */
#define LCD_BACKLIGHT_PIN GPIO_NUM_26 /* backlight, active LOW */
#define LCD_RST_PIN       GPIO_NUM_27
#define LCD_LEDC_CH       1
#define LCD_LEDC_TIMER    1

#define MIPI_DSI_LANE_NUM        2
#define MIPI_DSI_LANE_BITRATE_MBPS 500
#define MIPI_DSI_PHY_PWR_LDO_CHAN      3
#define MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV 2500

#define DPI_CLOCK_FREQ_MHZ 30

/* ----- ST7701 vendor-specific init commands (board-specific) ---------- */
static const uint8_t cmd_ff_13[]  = {0x77, 0x01, 0x00, 0x00, 0x13};
static const uint8_t cmd_ef_8[]   = {0x08};
static const uint8_t cmd_ff_10[]  = {0x77, 0x01, 0x00, 0x00, 0x10};
static const uint8_t cmd_c0[]     = {0x63, 0x00};
static const uint8_t cmd_c1[]     = {0x0D, 0x02};
static const uint8_t cmd_c2[]     = {0x17, 0x08};
static const uint8_t cmd_cc[]     = {0x10};
static const uint8_t cmd_b0_1[]   = {0x40, 0xC9, 0x94, 0x0E, 0x10, 0x05, 0x0B, 0x09, 0x08, 0x26, 0x04, 0x52, 0x10, 0x69, 0x6B, 0x69};
static const uint8_t cmd_b1_1[]   = {0x40, 0xD2, 0x98, 0x0C, 0x92, 0x07, 0x09, 0x08, 0x07, 0x25, 0x02, 0x0E, 0x0C, 0x6E, 0x78, 0x55};
static const uint8_t cmd_ff_11[]  = {0x77, 0x01, 0x00, 0x00, 0x11};
static const uint8_t cmd_b0_5d[]  = {0x5D};
static const uint8_t cmd_b1_4e[]  = {0x4E};
static const uint8_t cmd_b2_87[]  = {0x87};
static const uint8_t cmd_b3_80[]  = {0x80};
static const uint8_t cmd_b5_4e[]  = {0x4E};
static const uint8_t cmd_b7_85[]  = {0x85};
static const uint8_t cmd_b8_21[]  = {0x21};
static const uint8_t cmd_b9[]     = {0x10, 0x1F};
static const uint8_t cmd_bb_03[]  = {0x03};
static const uint8_t cmd_bc_00[]  = {0x00};
static const uint8_t cmd_c1_78[]  = {0x78};
static const uint8_t cmd_c2_78[]  = {0x78};
static const uint8_t cmd_d0_88[]  = {0x88};
static const uint8_t cmd_e0[]     = {0x00, 0x3A, 0x02};
static const uint8_t cmd_e1[]     = {0x04, 0xA0, 0x00, 0xA0, 0x05, 0xA0, 0x00, 0xA0, 0x00, 0x40, 0x40};
static const uint8_t cmd_e2[]     = {0x30, 0x00, 0x40, 0x40, 0x32, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00};
static const uint8_t cmd_e3[]     = {0x00, 0x00, 0x33, 0x33};
static const uint8_t cmd_e4[]     = {0x44, 0x44};
static const uint8_t cmd_e5[]     = {0x09, 0x2E, 0xA0, 0xA0, 0x0B, 0x30, 0xA0, 0xA0, 0x05, 0x2A, 0xA0, 0xA0, 0x07, 0x2C, 0xA0, 0xA0};
static const uint8_t cmd_e6[]     = {0x00, 0x00, 0x33, 0x33};
static const uint8_t cmd_e7[]     = {0x44, 0x44};
static const uint8_t cmd_e8[]     = {0x08, 0x2D, 0xA0, 0xA0, 0x0A, 0x2F, 0xA0, 0xA0, 0x04, 0x29, 0xA0, 0xA0, 0x06, 0x2B, 0xA0, 0xA0};
static const uint8_t cmd_eb[]     = {0x00, 0x00, 0x4E, 0x4E, 0x00, 0x00, 0x00};
static const uint8_t cmd_ec[]     = {0x08, 0x01};
static const uint8_t cmd_ed[]     = {0xB0, 0x2B, 0x98, 0xA4, 0x56, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0x65, 0x4A, 0x89, 0xB2, 0x0B};
static const uint8_t cmd_ef[]     = {0x08, 0x08, 0x08, 0x45, 0x3F, 0x54};
static const uint8_t cmd_ff_00[]  = {0x77, 0x01, 0x00, 0x00, 0x00};
/* 0x11 (sleep out, 120ms) and 0x29 (display on) carry no parameter bytes */
static const uint8_t cmd_11[] = {0x00};
static const uint8_t cmd_29[] = {0x00};

#define ST_INIT(cmd, data) { (cmd), (data), sizeof(data), 0 }
#define ST_INIT_SLEEP()   { 0x11, cmd_11, 0, 120 } /* 0x11 sleep out + delay 120ms */
#define ST_INIT_DISPON()  { 0x29, cmd_29, 0, 0 }   /* 0x29 display on */

static const st7701_lcd_init_cmd_t st7701_init_cmds[] = {
    ST_INIT(0xFF, cmd_ff_13),
    ST_INIT(0xEF, cmd_ef_8),
    ST_INIT(0xFF, cmd_ff_10),
    ST_INIT(0xC0, cmd_c0),
    ST_INIT(0xC1, cmd_c1),
    ST_INIT(0xC2, cmd_c2),
    ST_INIT(0xCC, cmd_cc),
    ST_INIT(0xB0, cmd_b0_1),
    ST_INIT(0xB1, cmd_b1_1),
    ST_INIT(0xFF, cmd_ff_11),
    ST_INIT(0xB0, cmd_b0_5d),
    ST_INIT(0xB1, cmd_b1_4e),
    ST_INIT(0xB2, cmd_b2_87),
    ST_INIT(0xB3, cmd_b3_80),
    ST_INIT(0xB5, cmd_b5_4e),
    ST_INIT(0xB7, cmd_b7_85),
    ST_INIT(0xB8, cmd_b8_21),
    ST_INIT(0xB9, cmd_b9),
    ST_INIT(0xBB, cmd_bb_03),
    ST_INIT(0xBC, cmd_bc_00),
    ST_INIT(0xC1, cmd_c1_78),
    ST_INIT(0xC2, cmd_c2_78),
    ST_INIT(0xD0, cmd_d0_88),
    ST_INIT(0xE0, cmd_e0),
    ST_INIT(0xE1, cmd_e1),
    ST_INIT(0xE2, cmd_e2),
    ST_INIT(0xE3, cmd_e3),
    ST_INIT(0xE4, cmd_e4),
    ST_INIT(0xE5, cmd_e5),
    ST_INIT(0xE6, cmd_e6),
    ST_INIT(0xE7, cmd_e7),
    ST_INIT(0xE8, cmd_e8),
    ST_INIT(0xEB, cmd_eb),
    ST_INIT(0xEC, cmd_ec),
    ST_INIT(0xED, cmd_ed),
    ST_INIT(0xEF, cmd_ef),
    ST_INIT(0xFF, cmd_ff_00),
    ST_INIT_SLEEP(),  /* 0x11 sleep out + 120ms */
    ST_INIT_DISPON(), /* 0x29 display on */
};

/* ---- module-local handles ------------------------------------------- */
static esp_lcd_panel_handle_t   s_panel = NULL;
static esp_lcd_panel_io_handle_t s_io   = NULL;

/******************************************************************************
 * backlight
 ******************************************************************************/
static esp_err_t brightness_init(void)
{
    const ledc_channel_config_t channel_config = {
        .gpio_num   = LCD_BACKLIGHT_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_1,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_1,
        .duty       = 0,
        .hpoint     = 0,
        .flags      = {.output_invert = 1}, /* active LOW */
    };
    const ledc_timer_config_t timer_config = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num       = LEDC_TIMER_1,
        .freq_hz         = 5000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_config), TAG, "LEDC timer config failed");
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel_config), TAG, "LEDC channel config failed");
    return ESP_OK;
}

void display_set_brightness(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    uint32_t duty = (uint32_t)(percent * 1023 / 100);
    /* output_invert=1: high duty -> pin LOW -> backlight ON */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

/******************************************************************************
 * Display driver
 ******************************************************************************/
void display_init(void)
{
    ESP_ERROR_CHECK(brightness_init());

    /* power the MIPI DSI PHY before creating the DSI bus */
    esp_ldo_channel_handle_t phy_pwr = NULL;
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id    = MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_cfg, &phy_pwr));

    /* MIPI DSI bus (init the PHY too) */
    esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
    const esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id            = 0,
        .num_data_lanes    = MIPI_DSI_LANE_NUM,
        .phy_clk_src       = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = MIPI_DSI_LANE_BITRATE_MBPS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &dsi_bus));

    /* DBI IO: send ST7701 commands/params over the DSI DBI interface */
    const esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_config, &s_io));

    /* DPI panel config (order must match declaration order for C++ designated init) */
    esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = 30,
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
        .num_fbs = 3,
        .video_timing = {
            .h_size = 480,
            .v_size = 800,
            .hsync_pulse_width = 12,
            .hsync_back_porch = 42,
            .hsync_front_porch = 42,
            .vsync_pulse_width = 8,
            .vsync_back_porch = 2,
            .vsync_front_porch = 60,
        },
        .flags = { .use_dma2d = true },
    };

    st7701_vendor_config_t vendor_config = {
        .init_cmds      = st7701_init_cmds,
        .init_cmds_size = sizeof(st7701_init_cmds) / sizeof(st7701_lcd_init_cmd_t),
        .mipi_config = {
            .dsi_bus = dsi_bus,
            .dpi_config = &dpi_config,
        },
        .flags = { .use_mipi_interface = 1 },
    };

    esp_lcd_panel_dev_config_t dev_config = {
        .reset_gpio_num = LCD_RST_PIN,
        .rgb_ele_order = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(s_io, &dev_config, &s_panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));

    display_set_brightness(100);
    ESP_LOGI(TAG, "Display initialized (480x800 RGB565, MIPI-DSI)");
    ESP_LOGI(TAG, "Backlight on (pin %d, active LOW)", LCD_BACKLIGHT_PIN);
}

esp_lcd_panel_handle_t display_get_panel_handle(void)
{
    return s_panel;
}

esp_lcd_panel_io_handle_t display_get_io_handle(void)
{
    return s_io;
}