#pragma once

#include <INA226.h>

#include "coulmeter_config.h"

namespace coulmeter {

class PowerMonitor {
 public:
  PowerMonitor();

  bool begin();
  bool readSample(Ina226Sample& sample);
  void integrateSample(const Ina226Sample& sample);

  double chargeMah() const;
  double energyMwh() const;

 private:
  INA226 sensor_;
  bool hasPreviousSample_ = false;
  Ina226Sample previousSample_ = {};
  double accumulatedChargeMah_ = 0.0;
  double accumulatedEnergyMwh_ = 0.0;
};

}  // namespace coulmeter
