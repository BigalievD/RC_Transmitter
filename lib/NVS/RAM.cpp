// #include <Preferences.h>
// #include "Variables.h"

// Preferences prefs;

// void putToRam(const char *ns, const char *k, uint8_t value){
//     prefs.begin(ns, false);
//     prefs.putUChar(k, value);
//     prefs.end();
// }

// uint8_t getFromRam(const char *ns, const char *k, uint8_t defaultValue = 0){
//     prefs.begin(ns, true);    // true = только чтение
//     uint8_t value = prefs.getUChar(k, defaultValue);
//     prefs.end();
//     return value;
// }
