#include "ui/vars.h"
#include <string.h>
#include <stdio.h>
#include "ui/screens.h"
#include "Variables.h"

StickLimits globalStickLimits[8];
bool isChSettingsScreenActive = false;


float usToPercent(int us, int minUs, int centerUs, int maxUs) {
    if (us <= centerUs) {
        int range = centerUs - minUs;
        if (range <= 0) return 0.0f;
        return ((float)(us - centerUs) / (float)range) * 100.0f;
    } else {
        int range = maxUs - centerUs;
        if (range <= 0) return 0.0f;
        return ((float)(us - centerUs) / (float)range) * 100.0f;
    }
}
// Перевод процентов (-100%..0%..+100%) в микросекунды (us)
int percentToUs(float pct, int minUs = 988, int centerUs = 1500, int maxUs = 2012) {
    if (pct < 0.0f) {
        // Отрицательный процент -> интервал [minUs, centerUs]
        int range = centerUs - minUs;
        return centerUs + (int)( (pct / 100.0f) * range );
    } else {
        // Положительный процент -> интервал [centerUs, maxUs]
        int range = maxUs - centerUs;
        return centerUs + (int)( (pct / 100.0f) * range );
    }
}