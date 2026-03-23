#include "power_monitor.h"

#include <Arduino.h>
#include <Wire.h>

namespace coulmeter {

PowerMonitor::PowerMonitor() : sensor_(kIna226Address, &Wire) {
}

bool PowerMonitor::begin() {
  if (!sensor_.begin()) {
    return false;
  }

  if (!sensor_.setAverage(INA226_1024_SAMPLES)) {
    return false;
  }

  return sensor_.setMaxCurrentShunt(kIna226MaxCurrentAmps, kIna226ShuntOhms) == 0;
}

bool PowerMonitor::readSample(Ina226Sample& sample) {
  if (!sensor_.waitConversionReady(kIna226ConversionReadyTimeoutMs)) {
    return false;
  }

  sample = {};
  sample.timestampMs = millis();
  sample.busVoltageV = sensor_.getBusVoltage();
  sample.shuntVoltageMv = sensor_.getShuntVoltage_mV();
  sample.currentMa = sensor_.getCurrent_mA();
  sample.powerMw = sensor_.getPower_mW();
  return true;
}

void PowerMonitor::integrateSample(const Ina226Sample& sample) {
  if (!hasPreviousSample_) {
    previousSample_ = sample;
    hasPreviousSample_ = true;
    return;
  }

  const double deltaHours =
      static_cast<double>(sample.timestampMs - previousSample_.timestampMs) /
      3600000.0;
  const double averageCurrentMa =
      (static_cast<double>(previousSample_.currentMa) + sample.currentMa) / 2.0;
  const double averagePowerMw =
      (static_cast<double>(previousSample_.powerMw) + sample.powerMw) / 2.0;

  accumulatedChargeMah_ += averageCurrentMa * deltaHours;
  accumulatedEnergyMwh_ += averagePowerMw * deltaHours;
  previousSample_ = sample;
}

double PowerMonitor::chargeMah() const {
  return accumulatedChargeMah_;
}

double PowerMonitor::energyMwh() const {
  return accumulatedEnergyMwh_;
}

}  // namespace coulmeter
