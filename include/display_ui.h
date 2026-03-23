#pragma once

#include <U8g2lib.h>

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

  static int centeredX(U8G2& display, const char* text);

  U8G2_SSD1306_128X64_NONAME_F_HW_I2C display_;
  bool ready_ = false;
};

}  // namespace coulmeter
