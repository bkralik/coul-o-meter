#pragma once

namespace coulmeter {

class BatteryMonitor {
 public:
  void begin() const;
  float readVoltageV() const;
};

}  // namespace coulmeter
