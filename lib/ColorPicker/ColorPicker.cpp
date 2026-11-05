#include "ColorPicker.h"
#include "ui/styles.h"

#include <cstdio>
#include <cstring>
#include <esp_heap_caps.h>

static constexpr int32_t PICKER_WIDTH = 800;
static constexpr int32_t PICKER_HEIGHT = 430;

static constexpr int32_t SV_X = 20;
static constexpr int32_t SV_Y = 15;
static constexpr int32_t SV_W = 760;
static constexpr int32_t SV_H = 320;

static constexpr int32_t MARKER_OUTER = 16;
static constexpr int32_t MARKER_INNER = 10;


// ============================================================
// CONSTRUCTOR
// ============================================================

ColorPicker::ColorPicker(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, PICKER_WIDTH, PICKER_HEIGHT);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);

    createUI();
    rebuildSVImage();
    updateColor();
}


// ============================================================
// CREATE UI
// ============================================================

void ColorPicker::createUI()
{
    // SV AREA
    svArea = lv_obj_create(root);
    lv_obj_remove_style_all(svArea);
    lv_obj_set_pos(svArea, SV_X, SV_Y);
    lv_obj_set_size(svArea, SV_W, SV_H);
    lv_obj_add_flag(svArea, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(svArea, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(svArea, svTouchEvent, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(svArea, svTouchEvent, LV_EVENT_PRESSING, this);

    // RGB565 buffer: 760 * 320 * 2 = 486400 bytes
    size_t svBytes = (size_t)SV_W * SV_H * sizeof(uint16_t);
    svBuffer = static_cast<uint16_t *>(heap_caps_malloc(svBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if(svBuffer == nullptr) svBuffer = static_cast<uint16_t *>(heap_caps_malloc(svBytes, MALLOC_CAP_8BIT));

    if(svBuffer != nullptr)
    {
        memset(svBuffer, 0, svBytes);

        svImageDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        svImageDsc.header.cf = LV_COLOR_FORMAT_RGB565;
        svImageDsc.header.flags = 0;
        svImageDsc.header.w = SV_W;
        svImageDsc.header.h = SV_H;
        svImageDsc.header.stride = SV_W * sizeof(uint16_t);
        svImageDsc.header.reserved_2 = 0;
        svImageDsc.data_size = svBytes;
        svImageDsc.data = reinterpret_cast<const uint8_t *>(svBuffer);
        svImageDsc.reserved = nullptr;

        svImage = lv_image_create(svArea);
        lv_image_set_src(svImage, &svImageDsc);
        lv_obj_set_pos(svImage, 0, 0);
        lv_obj_remove_flag(svImage, LV_OBJ_FLAG_CLICKABLE);
    }

    // MARKER
    svMarker = lv_obj_create(svArea);
    lv_obj_remove_style_all(svMarker);
    lv_obj_set_size(svMarker, MARKER_OUTER, MARKER_OUTER);
    lv_obj_set_style_radius(svMarker, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(svMarker, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(svMarker, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(svMarker, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(svMarker, LV_OBJ_FLAG_SCROLLABLE);

    svMarkerInner = lv_obj_create(svMarker);
    lv_obj_remove_style_all(svMarkerInner);
    lv_obj_set_size(svMarkerInner, MARKER_INNER, MARKER_INNER);
    lv_obj_set_style_radius(svMarkerInner, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(svMarkerInner, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_center(svMarkerInner);
    lv_obj_remove_flag(svMarkerInner, LV_OBJ_FLAG_CLICKABLE);

    // COLOR PREVIEW
    colorPreview = lv_obj_create(root);
    lv_obj_remove_style_all(colorPreview);
    lv_obj_set_pos(colorPreview, 20, 355);
    lv_obj_set_size(colorPreview, 42, 42);
    lv_obj_set_style_radius(colorPreview, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(colorPreview, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(colorPreview, 0, LV_PART_MAIN);

    // HEX LABEL
    hexLabel = lv_label_create(root);
    lv_obj_set_pos(hexLabel, 80, 367);
    lv_obj_set_width(hexLabel, 100);
    lv_obj_set_style_text_align(hexLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    // HUE SLIDER
    hueSlider = lv_slider_create(root);
    lv_obj_set_pos(hueSlider, 200, 365);
    lv_obj_set_size(hueSlider, 360, 22);
    lv_slider_set_range(hueSlider, 0, 359);
    lv_slider_set_value(hueSlider, hue, LV_ANIM_OFF);
    lv_obj_set_style_bg_opa(hueSlider, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(hueSlider, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(hueSlider, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(hueSlider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_border_color(hueSlider, lv_color_hex(0x666666), LV_PART_KNOB);
    lv_obj_set_style_border_width(hueSlider, 1, LV_PART_KNOB);
    lv_obj_set_style_radius(hueSlider, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(hueSlider, hueEvent, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(hueSlider, hueDrawEvent, LV_EVENT_DRAW_MAIN, this);

    // OK BUTTON
    applyButton = lv_button_create(root);
    lv_obj_remove_style_all(applyButton);
    add_style_button1(applyButton);
    lv_obj_set_pos(applyButton, 620, 352);
    lv_obj_set_size(applyButton, 150, 46);
    lv_obj_set_style_radius(applyButton, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(applyButton, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(applyButton, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_event_cb(applyButton, applyEvent, LV_EVENT_CLICKED, this);

    applyLabel = lv_label_create(applyButton);
    lv_label_set_text(applyLabel, "OK");
    lv_obj_center(applyLabel);
}


// ============================================================
// SET / GET COLOR
// ============================================================

void ColorPicker::setColor(lv_color_t color)
{
    uint32_t rgb = lv_color_to_u32(color);
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = rgb & 0xFF;

    lv_color_hsv_t hsv = lv_color_rgb_to_hsv(r, g, b);

    hue = hsv.h;
    saturation = hsv.s;
    value = hsv.v;

    updating = true;
    lv_slider_set_value(hueSlider, hue, LV_ANIM_OFF);
    updating = false;

    rebuildSVImage();
    updateColor();
}

lv_color_t ColorPicker::getColor() const
{
    return lv_color_hsv_to_rgb(hue, saturation, value);
}


// ============================================================
// CALLBACK / ROOT
// ============================================================

void ColorPicker::setApplyCallback(ApplyCallback callback, void *user_data)
{
    applyCallback = callback;
    callbackUserData = user_data;
}

lv_obj_t *ColorPicker::getRoot() const
{
    return root;
}


// ============================================================
// UPDATE CURRENT COLOR
// IMPORTANT: SV IMAGE IS NOT REBUILT HERE
// ============================================================

void ColorPicker::updateColor()
{
    lv_color_t color = getColor();

    lv_obj_set_style_bg_color(colorPreview, color, LV_PART_MAIN);
    lv_obj_set_style_bg_color(svMarkerInner, color, LV_PART_MAIN);

    uint32_t rgb = lv_color_to_u32(color);
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = rgb & 0xFF;

    char hex[8];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", r, g, b);
    lv_label_set_text(hexLabel, hex);

    updateMarker();
}


// ============================================================
// MARKER
// ============================================================

void ColorPicker::updateMarker()
{
    int32_t centerX = ((int32_t)saturation * (SV_W - 1)) / 100;
    int32_t centerY = ((int32_t)(100 - value) * (SV_H - 1)) / 100;

    int32_t x = centerX - MARKER_OUTER / 2;
    int32_t y = centerY - MARKER_OUTER / 2;

    if(x < 0) x = 0;
    if(y < 0) y = 0;
    if(x > SV_W - MARKER_OUTER) x = SV_W - MARKER_OUTER;
    if(y > SV_H - MARKER_OUTER) y = SV_H - MARKER_OUTER;

    lv_obj_set_pos(svMarker, x, y);
    lv_obj_move_foreground(svMarker);
}


// ============================================================
// TOUCH SV AREA
// ============================================================

void ColorPicker::svTouchEvent(lv_event_t *e)
{
    ColorPicker *picker = static_cast<ColorPicker *>(lv_event_get_user_data(e));
    if(picker == nullptr) return;
    picker->updateSVFromTouch(e);
}

void ColorPicker::updateSVFromTouch(lv_event_t *e)
{
    lv_indev_t *indev = lv_indev_get_act();
    if(indev == nullptr) return;

    lv_point_t point;
    lv_indev_get_point(indev, &point);

    lv_area_t area;
    lv_obj_get_coords(svArea, &area);

    int32_t width = lv_area_get_width(&area);
    int32_t height = lv_area_get_height(&area);
    int32_t x = point.x - area.x1;
    int32_t y = point.y - area.y1;

    if(x < 0) x = 0;
    if(y < 0) y = 0;
    if(x >= width) x = width - 1;
    if(y >= height) y = height - 1;

    saturation = (uint8_t)((x * 100) / (width - 1));
    value = (uint8_t)(100 - ((y * 100) / (height - 1)));

    updateColor();
}


// ============================================================
// HUE EVENT
// Heavy SV rebuild happens ONLY here
// ============================================================

void ColorPicker::hueEvent(lv_event_t *e)
{
    ColorPicker *picker = static_cast<ColorPicker *>(lv_event_get_user_data(e));
    if(picker == nullptr || picker->updating) return;

    uint16_t newHue = (uint16_t)lv_slider_get_value(picker->hueSlider);
    if(newHue == picker->hue) return;

    picker->hue = newHue;
    picker->rebuildSVImage();
    picker->updateColor();
}


// ============================================================
// APPLY
// ============================================================

void ColorPicker::applyEvent(lv_event_t *e)
{
    ColorPicker *picker = static_cast<ColorPicker *>(lv_event_get_user_data(e));
    if(picker == nullptr) return;

    if(picker->applyCallback != nullptr) picker->applyCallback(picker->getColor(), picker->callbackUserData);
}


// ============================================================
// EXACT HSV -> RGB888
// Avoid lv_color_t here because LV_COLOR_DEPTH=16 would quantize
// the color BEFORE dithering.
// ============================================================

void ColorPicker::hsvToRgb888(uint16_t h, uint8_t s, uint8_t v, uint8_t &r, uint8_t &g, uint8_t &b)
{
    if(s == 0)
    {
        uint8_t c = (uint8_t)(((uint32_t)v * 255 + 50) / 100);
        r = c;
        g = c;
        b = c;
        return;
    }

    uint32_t V = ((uint32_t)v * 255 + 50) / 100;
    uint32_t S = ((uint32_t)s * 255 + 50) / 100;

    uint16_t region = h / 60;
    uint16_t remainder = (h % 60) * 255 / 60;

    uint32_t p = (V * (255 - S)) / 255;
    uint32_t q = (V * (255 - ((S * remainder) / 255))) / 255;
    uint32_t t = (V * (255 - ((S * (255 - remainder)) / 255))) / 255;

    switch(region)
    {
        case 0: r = V; g = t; b = p; break;
        case 1: r = q; g = V; b = p; break;
        case 2: r = p; g = V; b = t; break;
        case 3: r = p; g = q; b = V; break;
        case 4: r = t; g = p; b = V; break;
        default: r = V; g = p; b = q; break;
    }
}


// ============================================================
// BAYER 4x4 RGB888 -> RGB565
// ============================================================

uint16_t ColorPicker::rgb888To565Dithered(uint8_t r, uint8_t g, uint8_t b, int32_t x, int32_t y)
{
    static constexpr uint8_t bayer[4][4] = {
        { 0,  8,  2, 10},
        {12,  4, 14,  6},
        { 3, 11,  1,  9},
        {15,  7, 13,  5}
    };

    int32_t d = (int32_t)bayer[y & 3][x & 3] * 2 - 15;

    int32_t rd = (int32_t)r + (d * 4) / 16;
    int32_t gd = (int32_t)g + (d * 2) / 16;
    int32_t bd = (int32_t)b + (d * 4) / 16;

    if(rd < 0) rd = 0; else if(rd > 255) rd = 255;
    if(gd < 0) gd = 0; else if(gd > 255) gd = 255;
    if(bd < 0) bd = 0; else if(bd > 255) bd = 255;

    uint16_t r5 = (uint16_t)(rd >> 3);
    uint16_t g6 = (uint16_t)(gd >> 2);
    uint16_t b5 = (uint16_t)(bd >> 3);

    return (uint16_t)((r5 << 11) | (g6 << 5) | b5);
}


// ============================================================
// BUILD CACHED SV IMAGE
// This is the expensive operation. It is NOT called while
// moving the finger inside the SV area.
// ============================================================

void ColorPicker::rebuildSVImage()
{
    if(svBuffer == nullptr || svImage == nullptr) return;

    for(int32_t y = 0; y < SV_H; y++)
    {
        uint8_t v = (uint8_t)(100 - ((y * 100) / (SV_H - 1)));

        for(int32_t x = 0; x < SV_W; x++)
        {
            uint8_t s = (uint8_t)((x * 100) / (SV_W - 1));

            uint8_t r, g, b;
            hsvToRgb888(hue, s, v, r, g, b);

            svBuffer[(size_t)y * SV_W + x] = rgb888To565Dithered(r, g, b, x, y);
        }
    }

    // Source descriptor stays the same; pixel memory changed in place.
    // Invalidating the image is enough for an uncompressed RGB565 variable image.
    lv_obj_invalidate(svImage);
}


// ============================================================
// DRAW HUE SLIDER
// ============================================================

void ColorPicker::hueDrawEvent(lv_event_t *e)
{
    lv_obj_t *obj = static_cast<lv_obj_t *>(lv_event_get_target(e));
    lv_layer_t *layer = lv_event_get_layer(e);

    lv_area_t area;
    lv_obj_get_coords(obj, &area);

    area.y1 += 5;
    area.y2 -= 5;

    drawHueGradient(layer, area);
}


// ============================================================
// HUE RAINBOW
// ============================================================

void ColorPicker::drawHueGradient(lv_layer_t *layer, const lv_area_t &area)
{
    static const lv_color_t colors[7] = {
        lv_color_hex(0xFF0000),
        lv_color_hex(0xFFFF00),
        lv_color_hex(0x00FF00),
        lv_color_hex(0x00FFFF),
        lv_color_hex(0x0000FF),
        lv_color_hex(0xFF00FF),
        lv_color_hex(0xFF0000)
    };

    constexpr int32_t segments = 6;
    int32_t width = lv_area_get_width(&area);

    for(int32_t i = 0; i < segments; i++)
    {
        lv_area_t part = area;
        part.x1 = area.x1 + (width * i) / segments;
        part.x2 = area.x1 + (width * (i + 1)) / segments;
        if(i < segments - 1) part.x2++;

        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);

        dsc.bg_opa = LV_OPA_COVER;
        dsc.bg_grad.dir = LV_GRAD_DIR_HOR;
        dsc.bg_grad.stops_count = 2;
        dsc.bg_grad.stops[0].color = colors[i];
        dsc.bg_grad.stops[0].opa = LV_OPA_COVER;
        dsc.bg_grad.stops[0].frac = 0;
        dsc.bg_grad.stops[1].color = colors[i + 1];
        dsc.bg_grad.stops[1].opa = LV_OPA_COVER;
        dsc.bg_grad.stops[1].frac = 255;
        dsc.radius = (i == 0 || i == segments - 1) ? 6 : 0;

        lv_draw_rect(layer, &dsc, &part);
    }
}
