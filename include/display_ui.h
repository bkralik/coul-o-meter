#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "coulmeter_config.h"

namespace coulmeter {

class DisplayUi {
 public:
  DisplayUi();

  bool begin();
  void clear();
  void showBootSplash();
  void showMessage(const char* line1, const char* line2 = nullptr);
  void renderMeasurementScreen(
      const Ina226Sample& sample,
      double chargeMah,
      double energyMwh,
      float batteryVoltageV,
      size_t spinnerIndex);

 private:
  static void formatDisplayQuantity(
      char prefix,
      double milliValue,
      const char* milliUnit,
      const char* baseUnit,
      char* output,
      size_t outputSize);

  Adafruit_SSD1306 display_;
  bool ready_ = false;
};

}  // namespace coulmeter
