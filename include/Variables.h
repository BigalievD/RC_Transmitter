#ifndef VARIABLES_H
#define VARIABLES_H

//Touch GT911
#define  TOUCH_SDA 19
#define  TOUCH_SCL 20
#define  TOUCH_INT 18
#define  TOUCH_RST 38
#define  R_BTN 38

// Размер экрана
#define SCREEN_WIDTH   800
#define SCREEN_HEIGHT  480
// Пины LCD
#define TFT_BL      2
#define TFT_DE      40
#define TFT_VSYNC   41
#define TFT_HSYNC   39
#define TFT_PCLK    42
// RGB DATA
#define TFT_R0 45
#define TFT_R1 48
#define TFT_R2 47
#define TFT_R3 21
#define TFT_R4 14
#define TFT_G0 5
#define TFT_G1 6
#define TFT_G2 7
#define TFT_G3 15
#define TFT_G4 16
#define TFT_G5 4
#define TFT_B0 8
#define TFT_B1 3
#define TFT_B2 46
#define TFT_B3 9
#define TFT_B4 1
// hsync
#define HSYNC_POL 0  
#define HSYNC_FP 8   
#define HSYNC_P 4   
#define HSYNC_BP 8 
// vsync
#define VSYNC_POL 0 
#define VSYNC_FP 8  
#define VSYNC_P 4   
#define VSYNC_BP 8   
//pixel clock 
#define PCLK 1       // pclk 1=active 0=neg
#define PC 15000000L  

//#define HSYNC_POL 1,   
//#define HSYNC_FP 20,   
//#define HSYNC_P 4,   
//#define HSYNC_BP 10,  
//#define VSYNC_POL 1,   
//#define VSYNC_FP 12,    //это тоже хорошие настройки
//#define VSYNC_P 4,   
//#define VSYNC_BP 6,   
//#define PCLK 1,         
//#define PC 15000000L  

// для uart_reciever
#define MAX_PACKET_SIZE 256

#define ADS1115_ADDR 0x48
#define ADS1115_ADDR2 0x4A
#define ADS_SDA 21
#define ADS_SCL 24

// Глобальный объект дисплея

extern uint8_t TFT_BR;
extern uint8_t editedChannel;
extern bool isChannelsScreenActive;
extern bool isChSettingsScreenActive;

struct StickLimits {
    int min = 0;
    int center = 13216; // Середина по умолчанию для ADS1115
    int max = 26432;
    bool isCalibrated = false;
};

extern StickLimits globalStickLimits[8];

#endif