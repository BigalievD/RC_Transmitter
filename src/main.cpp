#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "display.h"
#include "lvgl_port.h"
#include "touch.h"
#include "ui/ui.h"
#include "uart_receiver.h"
#include "lvgl.h"
#include "actions.h"
#include "Variables.h"
#include "calibration.h"
#include "sd_card.h"
#include "FrSkyEncoder.h"
#include "InputManager.h"
#include "ModelManager.h"
#include "Mixer.h"
#include "OutputProcessor.h"
#include "ModelInputProcessor.h"
#include "VarProcessor.h"
#include "BalanceProcessor.h"
#include "TrimProcessor.h"
#include "FlightModeManager.h"
#include "LogicalSwitchManager.h"
#include "FailsafeProcessor.h"
#include "ModelRuntime.h"
#include "ModelStorage.h"
#include "HalfRangeTestInput.h"

BatteryMonitor battery;
const char* ssid = "14 pro";
const char* password = "12341234";
static uint32_t lastIconsUpdate = 0;
extern FrSkyEncoder encoder;
void balanceEncoderLeft();
void balanceEncoderRight();
void goBack();
FrSkyEncoder encoder(46, 47, 48);
void setup(){
    Serial.begin(115200);
    delay(500);
    Serial.println("1");
    modelManager.begin();
    Serial.println("2");
    sd_card_init();
        if(sd_card_is_ready()){
        Serial.println("SD Card ready");
        modelStorage.begin();
        Serial.println("3");
        modelManager.reloadModelList();
        Serial.println("4");
        modelManager.loadLastActiveModel();
        Serial.println("5");
    }
    // Перефирия
    pinMode(R_BTN, INPUT_PULLUP);
    pinMode(HALF_RANGE_TEST_GPIO, INPUT_PULLUP);
    Serial.println("6");
    display_init();
    Serial.println("7");
    lvgl_port_init();
    Serial.println("8");
    touch_init();
    Serial.println("9");
    ads_init();
    Serial.println("10");
    calibration_init();
    Serial.println("11");
    inputManager.begin();
    Serial.println("12");
    modelRuntime.begin();
    Serial.println("13");
    flightModeManager.begin();
    Serial.println("14");
    logicalSwitchManager.begin();
    Serial.println("15");
    load_all_calibration_limits();
    Serial.println("16");
    ui_init();
    Serial.println("17");
    Serial.printf("AFTER UI: free DMA=%u, largest DMA=%u\n",
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    // Привязываем корректные указатели
    init_channel_widget_pointers();
    // Загружаем тексты и иконки каналов
    load_channels_overview_ui();
    uart_init();
    encoder.begin();
    encoder.setLongPressTime(800);
    if(objects.global_status_bar != NULL){
        lv_obj_set_parent(objects.global_status_bar, lv_layer_top());
        lv_obj_set_align(objects.global_status_bar, LV_ALIGN_TOP_MID);
        lv_obj_move_foreground(objects.global_status_bar);
    }
    if(objects.global_bottom_bar != NULL){
        lv_obj_set_parent(objects.global_bottom_bar, lv_layer_top());
        lv_obj_set_align(objects.global_bottom_bar, LV_ALIGN_BOTTOM_MID);
        lv_obj_move_foreground(objects.global_bottom_bar);
    }
    lv_dropdown_set_symbol(objects.ch_edit_changer_panel__dropdown_step, NULL);
    lv_dropdown_set_symbol(objects.curve_changer_panel__dropdown_step, NULL);
    lv_dropdown_set_symbol(objects.mix_changer_panel__dropdown_step, nullptr);
    lv_dropdown_set_symbol(objects.vars_changer_panel__dropdown_step, nullptr);
    lv_dropdown_set_symbol(objects.hw_changer_panel__dropdown_step, nullptr);
    loadThemeSettings();
}
void loop(){
    //ArduinoOTA.handle();
    encoder.update();
    if(encoder.left()) {
        balanceEncoderLeft();
        Serial.println("left");
    }
    if(encoder.right()) {
        balanceEncoderRight();
        Serial.println("right");
    }
    if(encoder.pressed()){ 
        Serial.println("ENCODER PRESSED"); 
        balanceEncoderPressed(); 
    }
    if(encoder.longPressed()) {
        goBack();
        Serial.println("long pressed");
    }
    if(millis() - lastIconsUpdate >= 5000){
        lastIconsUpdate = millis();
        //update_icons();
    }
    inputManager.update();
    ModelConfig &activeModel = modelManager.getActiveModel();
    logicalSwitchManager.updatePre(activeModel);
    flightModeManager.update(activeModel);
    modelInputProcessor.process(activeModel);
    logicalSwitchManager.updatePost(activeModel);
    varProcessor.process(activeModel);
    varProcessor.servicePersistence();
    update_vars_runtime_display();
    mixer.process(activeModel);
    outputProcessor.process(activeModel, mixer.getChannels());
    if(isChannelsScreenActive){
        update_channels_overview_live();
    }
    update_channels();
    lv_timer_handler();
    yield();
}