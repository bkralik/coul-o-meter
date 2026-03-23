#include "display_ui.h"

#include <Wire.h>

namespace coulmeter {

DisplayUi::DisplayUi()
    : display_(kDisplayWidth, kDisplayHeight, &Wire, kDisplayResetPin) {
}

bool DisplayUi::begin() {
  ready_ = display_.begin(SSD1306_SWITCHCAPVCC, kDisplayI2cAddress);
  return ready_;
}

void DisplayUi::clear() {
  if (!ready_) {
    return;
  }

  display_.clearDisplay();
  display_.display();
}

void DisplayUi::showBootSplash() {
  if (!ready_) {
    return;
  }

  display_.clearDisplay();
  display_.setTextSize(2);
  display_.setTextColor(SSD1306_WHITE);

  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t textWidth = 0;
  uint16_t textHeight = 0;
  display_.getTextBounds("coulmeter", 0, 0, &x1, &y1, &textWidth, &textHeight);

  const int16_t x = (kDisplayWidth - static_cast<int16_t>(textWidth)) / 2;
  const int16_t y = (kDisplayHeight - static_cast<int16_t>(textHeight)) / 2;

  display_.setCursor(x, y);
  display_.print("coulmeter");
  display_.display();
}

void DisplayUi::showMessage(const char* line1, const char* line2) {
  if (!ready_) {
    return;
  }

  display_.clearDisplay();
  display_.setTextColor(SSD1306_WHITE);
  display_.setTextSize(1);
  display_.setCursor(0, 16);
  display_.println(line1);

  if (line2 != nullptr) {
    display_.setCursor(0, 32);
    display_.println(line2);
  }

  display_.display();
}

void DisplayUi::renderMeasurementScreen(
    const Ina226Sample& sample,
    double chargeMah,
    double energyMwh,
    float batteryVoltageV,
    size_t spinnerIndex) {
  if (!ready_) {
    return;
  }

  char chargeLine[20];
  char energyLine[20];
  char batteryLine[12];

  formatDisplayQuantity('Q', chargeMah, "mAh", "Ah", chargeLine, sizeof(chargeLine));
  formatDisplayQuantity('E', energyMwh, "mWh", "Wh", energyLine, sizeof(energyLine));
  snprintf(batteryLine, sizeof(batteryLine), "B%.2fV", batteryVoltageV);

  display_.clearDisplay();
  display_.setTextColor(SSD1306_WHITE);
  display_.setTextSize(2);

  display_.setCursor(0, 0);
  display_.println(chargeLine);

  display_.setCursor(0, 18);
  display_.println(energyLine);

  display_.setTextSize(1);
  display_.setCursor(0, 40);
  display_.print("V ");
  display_.print(sample.busVoltageV, 3);
  display_.println(" V");

  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t textWidth = 0;
  uint16_t textHeight = 0;
  display_.getTextBounds(batteryLine, 0, 40, &x1, &y1, &textWidth, &textHeight);
  display_.setCursor(kDisplayWidth - textWidth, 40);
  display_.println(batteryLine);

  display_.setCursor(0, 52);
  display_.print("I ");
  display_.print(sample.currentMa, 3);
  display_.println(" mA");

  display_.setCursor(kDisplayWidth - 6, kDisplayHeight - 8);
  display_.print(kReadoutSpinnerChars[spinnerIndex]);

  display_.display();
}

void DisplayUi::formatDisplayQuantity(
    char prefix,
    double milliValue,
    const char* milliUnit,
    const char* baseUnit,
    char* output,
    size_t outputSize) {
  double displayValue = milliValue;
  const char* unit = milliUnit;
  double absoluteValue = milliValue;
  if (absoluteValue < 0.0) {
    absoluteValue = -absoluteValue;
  }

  if (absoluteValue >= 1000.0) {
    displayValue = milliValue / 1000.0;
    absoluteValue /= 1000.0;
    unit = baseUnit;
  }

  int decimals = 3;
  if (absoluteValue >= 100.0) {
    decimals = 1;
  } else if (absoluteValue >= 10.0) {
    decimals = 2;
  }
  if (absoluteValue >= 1000.0) {
    decimals = 0;
  }

  snprintf(output, outputSize, "%c%.*f%s", prefix, decimals, displayValue, unit);
}

}  // namespace coulmeter
