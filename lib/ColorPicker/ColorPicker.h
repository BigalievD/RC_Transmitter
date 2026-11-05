#pragma once

#include <lvgl.h>
#include <stdint.h>

class ColorPicker
{
public:
    using ApplyCallback = void (*)(lv_color_t color, void *user_data);

    explicit ColorPicker(lv_obj_t *parent);

    void setColor(lv_color_t color);
    lv_color_t getColor() const;
    void setApplyCallback(ApplyCallback callback, void *user_data = nullptr);
    lv_obj_t *getRoot() const;

private:
    lv_obj_t *root = nullptr;

    lv_obj_t *svArea = nullptr;
    lv_obj_t *svImage = nullptr;
    lv_obj_t *svMarker = nullptr;
    lv_obj_t *svMarkerInner = nullptr;

    lv_obj_t *colorPreview = nullptr;
    lv_obj_t *hexLabel = nullptr;
    lv_obj_t *hueSlider = nullptr;
    lv_obj_t *applyButton = nullptr;
    lv_obj_t *applyLabel = nullptr;

    uint16_t hue = 0;
    uint8_t saturation = 100;
    uint8_t value = 100;
    bool updating = false;

    uint16_t *svBuffer = nullptr;
    lv_image_dsc_t svImageDsc = {};

    ApplyCallback applyCallback = nullptr;
    void *callbackUserData = nullptr;

    void createUI();
    void updateColor();
    void updateSVFromTouch(lv_event_t *e);
    void rebuildSVImage();
    void updateMarker();

    static void svTouchEvent(lv_event_t *e);
    static void hueEvent(lv_event_t *e);
    static void hueDrawEvent(lv_event_t *e);
    static void applyEvent(lv_event_t *e);

    static void hsvToRgb888(uint16_t h, uint8_t s, uint8_t v, uint8_t &r, uint8_t &g, uint8_t &b);
    static uint16_t rgb888To565Dithered(uint8_t r, uint8_t g, uint8_t b, int32_t x, int32_t y);
    static void drawHueGradient(lv_layer_t *layer, const lv_area_t &area);
};
