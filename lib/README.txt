System Events v14 — only changed/new files

NEW:
lib/SystemEvent/SystemEventManager.h
lib/SystemEvent/SystemEventManager.cpp

CHANGED:
src/actions.cpp
lib/ConditionEvaluator/ConditionEvaluator.cpp
lib/Model/ModelConfig.h
lib/Model/ModelManager.cpp
lib/Model/ModelStorage.cpp
lib/Model/ModelStorage.h

Already real with current two-stick hardware:
- Throttle active
- Throttle in dead band

Prepared runtime flags for later subsystems:
- Throttle hold
- Throttle cut
- Telemetry active
- Rssi Low
- Trainer active
- Flight reset

Default thresholds:
- Throttle active: throttle > -950
- Dead band: abs(throttle) <= 50

These can later be adjusted using:
systemEventManager.setThrottleActiveThreshold(...)
systemEventManager.setThrottleDeadBand(...)

External subsystems can drive:
systemEventManager.setState(SystemEventId::THROTTLE_HOLD, state);
systemEventManager.setState(SystemEventId::THROTTLE_CUT, state);
systemEventManager.setState(SystemEventId::TELEMETRY_ACTIVE, state);
systemEventManager.setState(SystemEventId::RSSI_LOW, state);
systemEventManager.setState(SystemEventId::TRAINER_ACTIVE, state);

Flight reset pulse:
systemEventManager.triggerFlightReset();
systemEventManager.endCycle();

Model version: v14, with v13 -> v14 migration.

Preserved:
- centered popups
- live mix_source_percent
- popupDismissOverlay defined exactly once
