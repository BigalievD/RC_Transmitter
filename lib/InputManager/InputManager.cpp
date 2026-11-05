#include "InputManager.h"
#include "HalfRangeTestInput.h"
#include "calibration.h"
#include <Preferences.h>
#include <Wire.h>
#include <ctype.h>


InputManager inputManager;


// ============================================================
// INIT
// ============================================================

void InputManager::begin()
{
    // Пока Mode 2 по умолчанию.
    // Позже будет загружаться из настроек передатчика.
    setStickMode(2);

    for (uint8_t i = 0; i < SWITCH_COUNT; i++) {
        switchDebugOverride[i] = false;
        switchValues[i] = 0;
    }

    switchDebugLineLength = 0;
    reloadHardwareMapping();
    initMcp23017();
    initAnalogExpansion();
    reloadAnalogControlSettings();
}


// ============================================================
// UPDATE
// ============================================================

void InputManager::update()
{
    // --------------------------------------------------------
    // 4 существующие откалиброванные оси стиков
    // --------------------------------------------------------

    analogValues[static_cast<uint8_t>(AnalogInput::STICK_LX)] =
        getNormalizedStickValue(0);

    analogValues[static_cast<uint8_t>(AnalogInput::STICK_LY)] =
        getNormalizedStickValue(1);

    analogValues[static_cast<uint8_t>(AnalogInput::STICK_RX)] =
        getNormalizedStickValue(2);

    analogValues[static_cast<uint8_t>(AnalogInput::STICK_RY)] =
        getNormalizedStickValue(3);


    // Read one additional ADS channel per tick; preserve calibrated LX/LY/RX/RY.
    pollAnalogExpansion();

    pollMcp23017();

    // Decode each configured logical switch from its assigned physical slot.
    // A 3-POS middle is accepted only when neither contact is asserted.
    // A brief pass through the middle can be ignored using switchMiddleDelayMs.
    uint32_t switchNow = millis();
    for(uint8_t row=0;row<14;row++){
        uint8_t input=static_cast<uint8_t>(SwitchInput::SW_3POS_1)+row;
        if(switchDebugOverride[input]) continue;
        uint8_t mode=hardwareType[row];
        uint8_t slot=hardwareSlot[row];
        if(mode==0||slot>=14){
            switchValues[input]=0;
            middlePending[row]=false;
            continue;
        }
        uint8_t pinA=mcpPinA[slot];
        if(pinA>=32||!mcpDetected[pinA/16]){
            switchValues[input]=0;
            middlePending[row]=false;
            continue;
        }
        bool a=mcpPinLow(pinA);
        int16_t next=a?1000:-1000;
        if(mode==2){
            uint8_t pinB=mcpPinB[slot];
            if(pinB>=32||!mcpDetected[pinB/16]){
                switchValues[input]=0;
                middlePending[row]=false;
                continue;
            }
            bool b=mcpPinLow(pinB);
            // Both contacts asserted is an invalid electrical state, not middle.
            if(a&&b){
                middlePending[row]=false;
                continue;
            }
            next=a?1000:(b?-1000:0);
            if(next==0&&switchValues[input]!=0&&switchMiddleDelayMs>0){
                if(!middlePending[row]){
                    middlePending[row]=true;
                    middleStartMs[row]=switchNow;
                }
                if((uint32_t)(switchNow-middleStartMs[row])<switchMiddleDelayMs) continue;
            }else middlePending[row]=false;
        }else middlePending[row]=false;
        switchValues[input]=next;
    }

    // Dedicated left/right stick pushbuttons: pressed +1000, released -1000.
    // Preserve serial override and do not fabricate an input for unmapped pins.
    for(uint8_t button=0;button<2;button++){
        if(switchDebugOverride[button]) continue;
        uint8_t pin=stickButtonPin[button];
        switchValues[button]=0;
        if(pin>=32||!mcpDetected[pin/16]) continue;
        switchValues[button]=mcpPinLow(pin)?1000:-1000;
    }

    processSerialSwitchDebug();
}



// MCP23017 hardware mapping: pins are numbered 0..15 on the first device,
// 16..31 on the second. 255 means unassigned, never guess a board pinout.
#ifndef RC_MCP23017_ADDR_0
#define RC_MCP23017_ADDR_0 0x20
#endif
#ifndef RC_MCP23017_ADDR_1
#define RC_MCP23017_ADDR_1 0x21
#endif
static constexpr uint8_t MCP_ADDRS[2] = {RC_MCP23017_ADDR_0, RC_MCP23017_ADDR_1};

void InputManager::reloadHardwareMapping(){
    Preferences prefs;
    bool ready = prefs.begin("hwsettings", true);
    for (uint8_t i = 0; i < 14; i++){
        char key[16];
        snprintf(key, sizeof(key), "swslot%u", (unsigned)(i + 1));
        hardwareSlot[i] = ready ? prefs.getUChar(key, i) : i;
        if (hardwareSlot[i] >= 14) hardwareSlot[i] = i;
        snprintf(key, sizeof(key), "swtype%u", (unsigned)(i + 1));
        hardwareType[i] = ready ? prefs.getUChar(key, i < 6 ? 2 : 1) : (i < 6 ? 2 : 1);
        if (hardwareType[i] > 3) hardwareType[i] = 0;
        snprintf(key, sizeof(key), "mcpA%u", (unsigned)(i + 1));
        mcpPinA[i] = ready ? prefs.getUChar(key, 255) : 255;
        snprintf(key, sizeof(key), "mcpB%u", (unsigned)(i + 1));
        mcpPinB[i] = ready ? prefs.getUChar(key, 255) : 255;
        if (mcpPinA[i] >= 32) mcpPinA[i] = 255;
        if (mcpPinB[i] >= 32) mcpPinB[i] = 255;
    }
    // Stick buttons are independent from the 14 switches.
    // mcpBtnL / mcpBtnR: 0..31, 255 means not configured.
    stickButtonPin[0] = ready ? prefs.getUChar("mcpBtnL", 255) : 255;
    stickButtonPin[1] = ready ? prefs.getUChar("mcpBtnR", 255) : 255;
    for(uint8_t i=0;i<2;i++){
        if(stickButtonPin[i]>=32) stickButtonPin[i]=255;
    }
    // System-wide 3-POS midpoint confirmation delay (ETHOS default: 0 ms).
    switchMiddleDelayMs=ready?prefs.getUShort("swMidDelay",0):0;
    if(switchMiddleDelayMs>1000) switchMiddleDelayMs=1000;
    for(uint8_t i=0;i<14;i++) middlePending[i]=false;
    if (ready) prefs.end();
}

void InputManager::writeMcpRegister(uint8_t device, uint8_t reg, uint8_t value){
    Wire.beginTransmission(MCP_ADDRS[device]);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

bool InputManager::readMcpRegister(uint8_t device, uint8_t reg, uint8_t &value){
    Wire.beginTransmission(MCP_ADDRS[device]);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(MCP_ADDRS[device], (uint8_t)1) != 1) return false;
    value = Wire.read();
    return true;
}

void InputManager::initMcp23017(){
    for (uint8_t d = 0; d < 2; d++){
        Wire.beginTransmission(MCP_ADDRS[d]);
        mcpDetected[d] = Wire.endTransmission() == 0;
        if (!mcpDetected[d]){
            Serial.printf("MCP23017 0x%02X not detected\n", MCP_ADDRS[d]);
            continue;
        }
        // BANK=0 defaults: IODIRA/B 0x00/01, GPPUA/B 0x0C/0D.
        writeMcpRegister(d, 0x00, 0xFF);
        writeMcpRegister(d, 0x01, 0xFF);
        writeMcpRegister(d, 0x0C, 0xFF);
        writeMcpRegister(d, 0x0D, 0xFF);
        Serial.printf("MCP23017 0x%02X ready\n", MCP_ADDRS[d]);
    }
}

void InputManager::pollMcp23017(){
    uint32_t now = millis();
    if (now - lastMcpPollMs < 20) return;
    lastMcpPollMs = now;
    for (uint8_t d = 0; d < 2; d++){
        if (!mcpDetected[d]) continue;
        uint8_t a, b;
        if (!readMcpRegister(d, 0x12, a) || !readMcpRegister(d, 0x13, b)){
            mcpState[d] = 0xFFFF;
            continue;
        }
        mcpState[d] = (uint16_t)a | ((uint16_t)b << 8);
    }
}

bool InputManager::mcpPinLow(uint8_t pin) const{
    if (pin >= 32 || !mcpDetected[pin / 16]) return false;
    return (mcpState[pin / 16] & (uint16_t(1) << (pin % 16))) == 0;
}

// Each analog source has a stable logical ID; the physical channel is configurable.
// adcch5..adcch14: 0..15 (four ADS devices), 255 means disconnected.
// adcmin5..adcmin14 / adcmax5..adcmax14: raw calibration endpoints.
// Existing four stick axes stay on their original calibrated read path.
extern int16_t readADS1115(uint8_t address, uint8_t channel);
static constexpr uint8_t RC_ADS_ADDRESSES[4] = {0x48, 0x4A, 0x49, 0x4B};

void InputManager::initAnalogExpansion(){
    Preferences prefs;
    bool ready = prefs.begin("hwsettings", true);
    for(uint8_t i = 4; i < ANALOG_COUNT; i++){
        char key[18];
        snprintf(key, sizeof(key), "adcch%u", (unsigned)(i + 1));
        analogChannel[i] = ready ? prefs.getUChar(key, i) : i;
        if(analogChannel[i] >= 16) analogChannel[i] = 255;
        snprintf(key, sizeof(key), "adcmin%u", (unsigned)(i + 1));
        analogMin[i] = ready ? prefs.getShort(key, 0) : 0;
        snprintf(key, sizeof(key), "adcmax%u", (unsigned)(i + 1));
        analogMax[i] = ready ? prefs.getShort(key, 26400) : 26400;
        analogValues[i] = 0;
    }
    if(ready) prefs.end();
    for(uint8_t d = 0; d < 4; d++){
        Wire.beginTransmission(RC_ADS_ADDRESSES[d]);
        adsPresent[d] = Wire.endTransmission() == 0;
        Serial.printf("ADS1115 0x%02X: %s\n", RC_ADS_ADDRESSES[d], adsPresent[d] ? "ready" : "not detected");
    }
}

void InputManager::pollAnalogExpansion(){
    uint32_t now = millis();
    if(now - lastAnalogPollMs < 5) return;
    lastAnalogPollMs = now;
    uint8_t index = analogCursor++;
    if(analogCursor >= ANALOG_COUNT) analogCursor = 4;
    uint8_t physical = analogChannel[index];
    if(physical >= 16 || !adsPresent[physical / 4]) return;
    int32_t minValue = analogMin[index];
    int32_t maxValue = analogMax[index];
    if(maxValue <= minValue) return;
    // ADS read uses the same gain and single-ended configuration as the sticks.
    int32_t raw = readADS1115(RC_ADS_ADDRESSES[physical / 4], physical % 4);
    int32_t value = ((raw - minValue) * 2000L) / (maxValue - minValue) - 1000L;
    analogValues[index] = (int16_t)constrain(value, -1000L, 1000L);
}

// ============================================================
// PHYSICAL ANALOG INPUT
// ============================================================

void InputManager::reloadAnalogControlSettings(){
    Preferences prefs;
    bool ready=prefs.begin("hwsettings",true);
    extEnabled[0]=ready?prefs.getBool("ext1en",true):true;
    extEnabled[1]=ready?prefs.getBool("ext2en",true):true;
    sliderInverted[0]=ready?prefs.getBool("slider1inv",false):false;
    sliderInverted[1]=ready?prefs.getBool("slider2inv",false):false;
    if(ready) prefs.end();
}

bool InputManager::isAnalogEnabled(AnalogInput input) const{
    if(input==AnalogInput::STICK_LZ) return extEnabled[0];
    if(input==AnalogInput::STICK_RZ) return extEnabled[1];
    return static_cast<uint8_t>(input)<ANALOG_COUNT;
}

int16_t InputManager::getAnalog(AnalogInput input) const{
    uint8_t index=static_cast<uint8_t>(input);
    if(index>=ANALOG_COUNT||!isAnalogEnabled(input)) return 0;
    int16_t value=analogValues[index];
    if(input==AnalogInput::SLIDER_L&&sliderInverted[0]) return -value;
    if(input==AnalogInput::SLIDER_R&&sliderInverted[1]) return -value;
    return value;
}


// ============================================================
// PHYSICAL SWITCH / BUTTON
// ============================================================

int16_t InputManager::getSwitch(SwitchInput input) const
{
    // GPIO32 drives only SG. SH and the other switches retain their own inputs.
    if (input == SwitchInput::SW_2POS_1 && hardwareType[6] != 0 && mcpPinA[hardwareSlot[6]] >= 32 && !switchDebugOverride[8]) return gpio32SgSwitchValue();

    uint8_t index = static_cast<uint8_t>(input);

    if (index >= SWITCH_COUNT) {
        return 0;
    }

    return switchValues[index];
}


// ============================================================
// SERIAL MONITOR SWITCH DEBUG
// ============================================================
//
// Aliases:
//   SA..SF -> six 3-position switches
//   SG..SH -> two 2-position switches
//
// 3-pos:
//   UP   = +1000
//   MID  = 0
//   DOWN = -1000
//
// 2-pos:
//   UP   = +1000
//   DOWN = -1000
//
// Numeric input uses percent:
//   SA=100
//   SA=0
//   SA=-100
//
// Utility:
//   SW?      -> print current switch values
//   SWRESET  -> clear Serial overrides
// ============================================================

bool InputManager::parseSwitchDebugName(
    const char *name,
    SwitchInput &input) const
{
    if (!name) return false;

    if (strcasecmp(name, "SA") == 0)
        input = SwitchInput::SW_3POS_1;
    else if (strcasecmp(name, "SB") == 0)
        input = SwitchInput::SW_3POS_2;
    else if (strcasecmp(name, "SC") == 0)
        input = SwitchInput::SW_3POS_3;
    else if (strcasecmp(name, "SD") == 0)
        input = SwitchInput::SW_3POS_4;
    else if (strcasecmp(name, "SE") == 0)
        input = SwitchInput::SW_3POS_5;
    else if (strcasecmp(name, "SF") == 0)
        input = SwitchInput::SW_3POS_6;
    else if (strcasecmp(name, "SG") == 0)
        input = SwitchInput::SW_2POS_1;
    else if (strcasecmp(name, "SH") == 0)
        input = SwitchInput::SW_2POS_2;
    else if (strlen(name) == 2 && toupper((unsigned char)name[0]) == 'S' &&
             toupper((unsigned char)name[1]) >= 'I' && toupper((unsigned char)name[1]) <= 'N')
        input = static_cast<SwitchInput>(static_cast<uint8_t>(SwitchInput::SW_SLOT_9) + toupper((unsigned char)name[1]) - 'I');
    else
        return false;

    return true;
}


bool InputManager::parseSwitchDebugValue(
    SwitchInput input,
    const char *text,
    int16_t &value) const
{
    if (!text) return false;

    bool threePos = false;
    uint8_t raw=static_cast<uint8_t>(input);
    if(raw>=static_cast<uint8_t>(SwitchInput::SW_3POS_1)){
        uint8_t slot=raw-static_cast<uint8_t>(SwitchInput::SW_3POS_1);
        Preferences prefs;
        if(prefs.begin("hwsettings",true)){
            char key[16];snprintf(key,sizeof(key),"swtype%u",(unsigned)(slot+1));
            threePos=prefs.getUChar(key,slot<6?2:1)==2;
            prefs.end();
        }else threePos=slot<6;
    }

    if (strcasecmp(text, "UP") == 0) {
        value = 1000;
        return true;
    }

    if (strcasecmp(text, "DOWN") == 0) {
        value = -1000;
        return true;
    }

    if (strcasecmp(text, "MID") == 0) {
        if (!threePos) return false;
        value = 0;
        return true;
    }

    char *end = nullptr;
    long percent = strtol(text, &end, 10);

    if (end == text || *end != '\0') return false;
    if (percent < -100 || percent > 100) return false;

    if (!threePos && percent != -100 && percent != 100)
        return false;

    value = (int16_t)(percent * 10L);
    return true;
}


void InputManager::printSwitchDebugState() const
{
    static const char *names[] = {
        "BTN_L",
        "BTN_R",
        "SA",
        "SB",
        "SC",
        "SD",
        "SE",
        "SF",
        "SG",
        "SH",
        "SI", "SJ", "SK", "SL", "SM", "SN"
    };

    Serial.println("SWITCH DEBUG:");

    for (uint8_t i = 0; i < SWITCH_COUNT; i++) {
        Serial.print(names[i]);
        Serial.print('=');
        Serial.print((int)(switchValues[i] / 10));
        Serial.print('%');

        if (switchDebugOverride[i])
            Serial.print(" [SERIAL]");

        Serial.println();
    }
}


void InputManager::processSerialSwitchDebug()
{
    static uint32_t lastByteTime = 0;

    auto processBufferedLine = [&]() {
        if (switchDebugLineLength == 0)
            return;

        switchDebugLine[switchDebugLineLength] = '\0';

        char *line = switchDebugLine;

        while (*line == ' ' || *line == '\t')
            line++;

        char *tail = line + strlen(line);

        while (tail > line && (tail[-1] == ' ' || tail[-1] == '\t')) {
            tail--;
            *tail = '\0';
        }

        Serial.print("SW RX: ");
        Serial.println(line);

        if (strcasecmp(line, "SW?") == 0) {
            printSwitchDebugState();
            switchDebugLineLength = 0;
            return;
        }

        if (strcasecmp(line, "SWRESET") == 0) {
            for (uint8_t i = 0; i < SWITCH_COUNT; i++) {
                switchDebugOverride[i] = false;
                switchValues[i] = 0;
            }

            Serial.println("SWITCH DEBUG RESET");
            switchDebugLineLength = 0;
            return;
        }

        char *equals = strchr(line, '=');

        if (!equals) {
            Serial.println("SWITCH DEBUG: use SA=UP, SA=MID, SA=DOWN, SG=UP, SW?, SWRESET");
            switchDebugLineLength = 0;
            return;
        }

        *equals = '\0';

        char *name = line;
        char *valueText = equals + 1;

        while (*valueText == ' ' || *valueText == '\t')
            valueText++;

        SwitchInput input;

        if (!parseSwitchDebugName(name, input)) {
            Serial.print("SWITCH DEBUG: unknown switch ");
            Serial.println(name);
            switchDebugLineLength = 0;
            return;
        }

        int16_t value = 0;

        if (!parseSwitchDebugValue(input, valueText, value)) {
            Serial.print("SWITCH DEBUG: invalid value for ");
            Serial.println(name);
            switchDebugLineLength = 0;
            return;
        }

        uint8_t index = static_cast<uint8_t>(input);

        switchValues[index] = value;
        switchDebugOverride[index] = true;

        Serial.print("SWITCH DEBUG: ");
        Serial.print(name);
        Serial.print('=');
        Serial.print((int)(value / 10));
        Serial.println('%');

        switchDebugLineLength = 0;
    };

    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        lastByteTime = millis();

        if (c == '\r' || c == '\n') {
            processBufferedLine();
            continue;
        }

        if (switchDebugLineLength < sizeof(switchDebugLine) - 1)
            switchDebugLine[switchDebugLineLength++] = c;
    }

    // Some terminal/USB combinations do not deliver CR/LF at all.
    // In that case, treat a short receive-idle gap as end-of-command.
    if (switchDebugLineLength > 0 &&
        lastByteTime != 0 &&
        (uint32_t)(millis() - lastByteTime) >= 50) {
        processBufferedLine();
    }
}


// ============================================================
// PRIMARY STICK FUNCTION
// ============================================================

int16_t InputManager::getStick(StickFunction function) const
{
    uint8_t index = static_cast<uint8_t>(function);

    if (index >= STICK_FUNCTION_COUNT) {
        return 0;
    }

    return getAnalog(stickMapping[index]);
}


// ============================================================
// STICK MAPPING
// ============================================================

void InputManager::setStickMapping(
    StickFunction function,
    AnalogInput input)
{
    uint8_t index = static_cast<uint8_t>(function);

    if (index >= STICK_FUNCTION_COUNT) {
        return;
    }

    stickMapping[index] = input;
}


AnalogInput InputManager::getStickMapping(
    StickFunction function) const
{
    uint8_t index = static_cast<uint8_t>(function);

    if (index >= STICK_FUNCTION_COUNT) {
        return AnalogInput::STICK_LX;
    }

    return stickMapping[index];
}


// ============================================================
// STICK MODE
// ============================================================

void InputManager::setStickMode(uint8_t mode)
{
    if (mode < 1 || mode > 4) {
        mode = 2;
    }

    stickMode = mode;

    switch (mode) {

        // ----------------------------------------------------
        // MODE 1
        //
        // Left:  Rudder / Elevator
        // Right: Aileron / Throttle
        // ----------------------------------------------------

        case 1:

            setStickMapping(
                StickFunction::RUDDER,
                AnalogInput::STICK_LX);

            setStickMapping(
                StickFunction::ELEVATOR,
                AnalogInput::STICK_LY);

            setStickMapping(
                StickFunction::AILERON,
                AnalogInput::STICK_RX);

            setStickMapping(
                StickFunction::THROTTLE,
                AnalogInput::STICK_RY);

            break;


        // ----------------------------------------------------
        // MODE 2
        //
        // Left:  Rudder / Throttle
        // Right: Aileron / Elevator
        // ----------------------------------------------------

        case 2:

            setStickMapping(
                StickFunction::RUDDER,
                AnalogInput::STICK_LX);

            setStickMapping(
                StickFunction::THROTTLE,
                AnalogInput::STICK_LY);

            setStickMapping(
                StickFunction::AILERON,
                AnalogInput::STICK_RX);

            setStickMapping(
                StickFunction::ELEVATOR,
                AnalogInput::STICK_RY);

            break;


        // ----------------------------------------------------
        // MODE 3
        //
        // Left:  Aileron / Throttle
        // Right: Rudder / Elevator
        // ----------------------------------------------------

        case 3:

            setStickMapping(
                StickFunction::AILERON,
                AnalogInput::STICK_LX);

            setStickMapping(
                StickFunction::THROTTLE,
                AnalogInput::STICK_LY);

            setStickMapping(
                StickFunction::RUDDER,
                AnalogInput::STICK_RX);

            setStickMapping(
                StickFunction::ELEVATOR,
                AnalogInput::STICK_RY);

            break;


        // ----------------------------------------------------
        // MODE 4
        //
        // Left:  Aileron / Elevator
        // Right: Rudder / Throttle
        // ----------------------------------------------------

        case 4:

            setStickMapping(
                StickFunction::AILERON,
                AnalogInput::STICK_LX);

            setStickMapping(
                StickFunction::ELEVATOR,
                AnalogInput::STICK_LY);

            setStickMapping(
                StickFunction::RUDDER,
                AnalogInput::STICK_RX);

            setStickMapping(
                StickFunction::THROTTLE,
                AnalogInput::STICK_RY);

            break;
    }
}


// ============================================================
// GET STICK MODE
// ============================================================

uint8_t InputManager::getStickMode() const
{
    return stickMode;
}