#ifndef CHANNEL_PROCESSOR_H
#define CHANNEL_PROCESSOR_H

#include <Arduino.h>
#include <Preferences.h>

struct ChannelConfig {
    char name[18] = "Channel";
    bool reverse = false;         // Реверс
    int outMin = 988;             // Мин. мкс
    int outCenter = 1500;         // Центр мкс
    int outMax = 2012;            // Макс. мкс
    
    int trim = 0;                 // Триммер (-100 .. +100)
    int expo = 0;                 // Экспонента (-100% .. +100%)
    
    float slowUp = 0.0f;          // Замедление вверх (сек на 100% хода)
    float slowDown = 0.0f;        // Замедление вниз (сек на 100% хода)

    void load(uint8_t ch) {
        Preferences pref;
        char ns[16];
        snprintf(ns, sizeof(ns), "ch_cfg_%d", ch);
        if (pref.begin(ns, true)) {
            char defaultName[18];
            snprintf(defaultName, sizeof(defaultName), "CH%d", ch + 1);
            String savedName = pref.getString("name", defaultName);
            if (savedName.length() == 0) {
                savedName = defaultName;
            }
            strncpy(name, savedName.c_str(), sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';

            reverse = pref.getBool("rev", false);
            outMin = pref.getInt("min", 988);
            outCenter = pref.getInt("center", 1500);
            outMax = pref.getInt("max", 2012);
            trim = pref.getInt("trim", 0);
            expo = pref.getInt("expo", 0);
            slowUp = pref.getFloat("slow_up", 0.0f);
            slowDown = pref.getFloat("slow_down", 0.0f);
            pref.end();
        }
    }

    void save(uint8_t ch) {
        Preferences pref;
        char ns[16];
        snprintf(ns, sizeof(ns), "ch_cfg_%d", ch);
        if (pref.begin(ns, false)) {
            pref.putString("name", name);
            pref.putBool("rev", reverse);
            pref.putInt("min", outMin);
            pref.putInt("center", outCenter);
            pref.putInt("max", outMax);
            pref.putInt("trim", trim);
            pref.putInt("expo", expo);
            pref.putFloat("slow_up", slowUp);
            pref.putFloat("slow_down", slowDown);
            pref.end();
        }
    }
};

class ChannelProcessor {
private:
    float currentFilteredValues[8] = {0.0f};
    uint32_t lastProcessTimeMicros[8] = {0}; // Время в микросекундах

    int16_t applyExpo(int16_t val, int expoPercent) {
        if (expoPercent == 0) return val;
        float x = val / 1000.0f;
        float k = expoPercent / 100.0f;
        float y = k * (x * x * x) + (1.0f - k) * x;
        return (int16_t)(y * 1000.0f);
    }

public:
    int processChannel(uint8_t ch, int16_t inputNorm, const ChannelConfig& config) {
        if (ch >= 8) return 1500;

        // 1. Точный расчет dt в секундах через micros()
        uint32_t now = micros();
        if (lastProcessTimeMicros[ch] == 0) lastProcessTimeMicros[ch] = now;
        
        float dt = (now - lastProcessTimeMicros[ch]) / 1000000.0f;
        lastProcessTimeMicros[ch] = now;

        // Защита от аномальных значений dt
        if (dt < 0.0f) dt = 0.001f;
        if (dt > 0.2f) dt = 0.2f; 

        // 2. Экспонента
        int16_t val = applyExpo(inputNorm, config.expo);

        // 3. Триммер
        val = constrain(val + (config.trim * 10), -1000, 1000);

        // 4. Slow Up / Slow Down
        float targetNorm = (float)val;
        float current = currentFilteredValues[ch];

        if (targetNorm > current) {
            if (config.slowUp > 0.0f) {
                // 1000.0f означает, что slowUp — это время на преодоление 100% хода (0..1000)
                float step = (1000.0f / config.slowUp) * dt;
                current += step;
                if (current > targetNorm) current = targetNorm;
            } else {
                current = targetNorm;
            }
        } else if (targetNorm < current) {
            if (config.slowDown > 0.0f) {
                float step = (1000.0f / config.slowDown) * dt;
                current -= step;
                if (current < targetNorm) current = targetNorm;
            } else {
                current = targetNorm;
            }
        }
        currentFilteredValues[ch] = current;

        // 5. Реверс
        int16_t finalNorm = (int16_t)current;
        if (config.reverse) finalNorm = -finalNorm;

        // 6. Перевод в мкс
        int minUs = (config.outMin == 0) ? 988 : config.outMin;
        int centerUs = (config.outCenter == 0) ? 1500 : config.outCenter;
        int maxUs = (config.outMax == 0) ? 2012 : config.outMax;

        int outputUs = centerUs;
        if (finalNorm <= 0) {
            outputUs = map(finalNorm, -1000, 0, minUs, centerUs);
        } else {
            outputUs = map(finalNorm, 0, 1000, centerUs, maxUs);
        }

        return constrain(outputUs, min(minUs, maxUs), max(minUs, maxUs));
    }
};

#endif