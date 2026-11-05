RC Transmitter 021126 — cumulative Hardware stages 1–6.
Base: 021126.zip (not intermediate patch).
Files: src/actions.cpp; lib/InputManager/InputManager.cpp; lib/InputManager/InputManager.h; lib/InputManager/Source.cpp.
Stage 6: Mixes > Source > Analogs now includes 4 StickFunction sources plus LZ, RZ, two sliders, two linear and four rotary controls as SourceType::ANALOG_INPUT. Existing stick Mode mapping preserved. Mix Source title displays configured switch name via SourceResolver instead of hardcoded SA–SH.
The previous stage 3 Source.cpp change is included; omitted by intermediate stage 4/5 patches.
No modifications to EEZ-generated files. Active Conditions > Analogs currently means Always on and is NOT changed to a threshold picker: condition analog comparison UI requires a separate implementation.
No PlatformIO build or hardware test performed.
