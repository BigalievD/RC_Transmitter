RC Transmitter / Hardware stage 10
Base: verified stage 9 actions/Source files + stage 7 InputManager MCP23017/ADS1115.
Ext1 (obj25) and Ext2 (obj31) enabled -> appear in Mixes Source and return actual values. Disabled -> hidden from Source and resolver returns 0 even for saved references.
Slider1 (obj43) and Slider2 (obj49) reverse the normalized analog signal in InputManager::getAnalog, without changing ADS calibration.
Preferences namespace hwsettings: ext1en, ext2en defaults true; slider1inv, slider2inv defaults false.
Settings update immediately and persist after reboot. EEZ generated files unchanged.
PlatformIO build and device tests NOT performed.
