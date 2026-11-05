#pragma once

#include <Arduino.h>


// ============================================================
// ФИЗИЧЕСКИЕ АНАЛОГОВЫЕ ВХОДЫ
// ============================================================

enum class AnalogInput : uint8_t {

    // Left stick
    STICK_LX = 0,
    STICK_LY,
    STICK_LZ,

    // Right stick
    STICK_RX,
    STICK_RY,
    STICK_RZ,

    // Side sliders
    SLIDER_L,
    SLIDER_R,

    // Linear potentiometers
    LINEAR_1,
    LINEAR_2,

    // Rotary potentiometers
    POT_1,
    POT_2,
    POT_3,
    POT_4,

    COUNT
};


// ============================================================
// ФИЗИЧЕСКИЕ ДИСКРЕТНЫЕ ВХОДЫ
// ============================================================

enum class SwitchInput : uint8_t {

    // Stick buttons
    STICK_BTN_L = 0,
    STICK_BTN_R,

    // 3-position switches
    SW_3POS_1,
    SW_3POS_2,
    SW_3POS_3,
    SW_3POS_4,
    SW_3POS_5,
    SW_3POS_6,

    // 2-position switches
    SW_2POS_1,
    SW_2POS_2,

    // New hardware slots appended to preserve persisted source IDs.
    SW_SLOT_9,
    SW_SLOT_10,
    SW_SLOT_11,
    SW_SLOT_12,
    SW_SLOT_13,
    SW_SLOT_14,

    COUNT
};


// ============================================================
// ОСНОВНЫЕ ФУНКЦИОНАЛЬНЫЕ СТИКИ
// ============================================================

enum class StickFunction : uint8_t {
    RUDDER = 0,
    THROTTLE,
    AILERON,
    ELEVATOR,

    COUNT
};


// ============================================================
// INPUT MANAGER
// ============================================================

class InputManager {

public:

    void begin();
    void update();
    void reloadHardwareMapping();
    void initAnalogExpansion();


    // --------------------------------------------------------
    // Physical analog inputs
    // --------------------------------------------------------

    int16_t getAnalog(AnalogInput input) const;
    bool isAnalogEnabled(AnalogInput input) const;
    void reloadAnalogControlSettings();


    // --------------------------------------------------------
    // Physical switches/buttons
    // --------------------------------------------------------

    int16_t getSwitch(SwitchInput input) const;

    // Debug input for switches through Serial Monitor.
    // Examples:
    //   SA=UP / SA=MID / SA=DOWN
    //   SG=UP / SG=DOWN
    //   SA=100 / SA=0 / SA=-100
    //   SW?
    //   SWRESET
    void processSerialSwitchDebug();

    // --------------------------------------------------------
    // Primary RC controls
    // --------------------------------------------------------

    int16_t getStick(StickFunction function) const;

    void setStickMapping(
        StickFunction function,
        AnalogInput input
    );

    AnalogInput getStickMapping(
        StickFunction function
    ) const;


    // --------------------------------------------------------
    // Stick modes
    // --------------------------------------------------------

    void setStickMode(uint8_t mode);

    uint8_t getStickMode() const;


private:

    static constexpr uint8_t ANALOG_COUNT =
        static_cast<uint8_t>(AnalogInput::COUNT);

    static constexpr uint8_t SWITCH_COUNT =
        static_cast<uint8_t>(SwitchInput::COUNT);

    static constexpr uint8_t STICK_FUNCTION_COUNT =
        static_cast<uint8_t>(StickFunction::COUNT);


    // Нормализованные значения аналоговых органов
    // -1000 ... +1000
    int16_t analogValues[ANALOG_COUNT] = {};


    // Нормализованные значения переключателей
    //
    // 3-pos: -1000 / 0 / +1000
    // 2-pos: -1000 / +1000
    // button: -1000 / +1000
    int16_t switchValues[SWITCH_COUNT] = {};

    bool switchDebugOverride[SWITCH_COUNT] = {};

    // MCP23017: physical slots 0..13, each with up to two contacts.
    uint8_t hardwareSlot[14] = {};
    uint8_t hardwareType[14] = {};
    uint16_t switchMiddleDelayMs = 0;
    uint32_t middleStartMs[14] = {};
    bool middlePending[14] = {};
    uint8_t mcpPinA[14] = {};
    uint8_t mcpPinB[14] = {};
    uint8_t stickButtonPin[2] = {255, 255}; // MCP logical pin for each stick button.
    bool mcpDetected[2] = {};
    uint16_t mcpState[2] = {0xFFFF, 0xFFFF};
    uint32_t lastMcpPollMs = 0;
    bool readMcpRegister(uint8_t device, uint8_t reg, uint8_t &value);
    void writeMcpRegister(uint8_t device, uint8_t reg, uint8_t value);
    void initMcp23017();
    void pollMcp23017();
    bool mcpPinLow(uint8_t pin) const;


    // ADS1115 expansion: analog sources 4..13, one conversion per loop.
    bool extEnabled[2] = {true, true};
    bool sliderInverted[2] = {false, false};
    bool adsPresent[4] = {};
    uint8_t analogChannel[ANALOG_COUNT] = {};
    int16_t analogMin[ANALOG_COUNT] = {};
    int16_t analogMax[ANALOG_COUNT] = {};
    uint8_t analogCursor = 4;
    uint32_t lastAnalogPollMs = 0;
    void pollAnalogExpansion();

    char switchDebugLine[40] = {};
    uint8_t switchDebugLineLength = 0;

    bool parseSwitchDebugName(
        const char *name,
        SwitchInput &input
    ) const;

    bool parseSwitchDebugValue(
        SwitchInput input,
        const char *text,
        int16_t &value
    ) const;

    void printSwitchDebugState() const;


    // RUD / THR / AIL / ELE → physical analog input
    AnalogInput stickMapping[STICK_FUNCTION_COUNT] = {};


    uint8_t stickMode = 2;
};


extern InputManager inputManager;