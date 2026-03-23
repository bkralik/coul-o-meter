#include "battery_monitor.h"

#include <Arduino.h>

#include "coulmeter_config.h"

namespace coulmeter {

void BatteryMonitor::begin() const {
  analogSetPinAttenuation(kBatterySensePin, ADC_11db);
}

float BatteryMonitor::readVoltageV() const {
  constexpr int kBatterySamples = 4;

  // Throw away the first sample to give the ADC input time to settle.
  analogReadMilliVolts(kBatterySensePin);

  uint32_t accumulatedMilliVolts = 0;
  for (int i = 0; i < kBatterySamples; ++i) {
    accumulatedMilliVolts += analogReadMilliVolts(kBatterySensePin);
  }

  const float dividerScale =
      (kBatteryDividerUpperOhms + kBatteryDividerLowerOhms) /
      kBatteryDividerLowerOhms;
  const float pinVoltageV =
      static_cast<float>(accumulatedMilliVolts) / kBatterySamples / 1000.0f;

  return pinVoltageV * dividerScale;
}

}  // namespace coulmeter
