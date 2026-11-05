RC Transmitter — ADS1115 stage 5 (applies AFTER stage 4)

Files: src/actions.cpp, lib/InputManager/InputManager.{h,cpp}
Four ADC addresses: #1 0x48, #2 0x4A (existing), #3 0x49, #4 0x4B.
The 4 existing calibrated stick axes are unchanged.
10 additional analog input IDs (index 4..13) are sampled round-robin, one every >=5 ms.
Mapping in Preferences namespace hwsettings:
 adcch5..adcch14: physical channel index 0..15, 255=unassigned; defaults = index (4..13).
 adcmin5..adcmin14 / adcmax5..adcmax14: raw endpoints (default 0/26400).
These defaults are only approximate voltage normalization, not calibrated values.
The analog source picker and hardware editing UI are NOT changed in this stage.
The actual ADC wiring and validation remain necessary.
No PlatformIO build or device test performed.
