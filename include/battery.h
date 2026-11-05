#pragma once
#include <Arduino.h>

struct BatteryStatus{
    float voltage;
    uint8_t percent;
    bool charging;
};

class BatteryMonitor{
public:
    explicit BatteryMonitor(uint8_t adcPin=37);

    void begin();

    // Возвращает только данные батареи.
    // charging передаётся снаружи, если у тебя есть отдельный сигнал "идёт зарядка".
    BatteryStatus read(bool charging=false);

private:
    uint8_t _adcPin;

    static constexpr float DIVIDER_RATIO=3.0f;
    static constexpr uint8_t SAMPLE_COUNT=12;

    float readVoltage();
    uint8_t voltageToPercent(float voltage) const;
};
