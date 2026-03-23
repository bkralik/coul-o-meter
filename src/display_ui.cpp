#include "display_ui.h"

#include <Wire.h>

namespace coulmeter {

DisplayUi::DisplayUi()
    : display_(U8G2_R0, kDisplayResetPin) {
}

bool DisplayUi::begin() {
  display_.setI2CAddress(kDisplayI2cAddress << 1);
  display_.begin();
  ready_ = true;
  return ready_;
}

void DisplayUi::clear() {
  if (!ready_) {
    return;
  }

  display_.clearBuffer();
  display_.sendBuffer();
}

void DisplayUi::showBootSplash() {
  if (!ready_) {
    return;
  }

  constexpr char kTitle[] = "coul-o-meter";
  constexpr char kSubtitle[] = "bkralik.cz";

  display_.clearBuffer();

  display_.setFont(u8g2_font_6x12_tf);
  display_.drawStr(centeredX(display_, kTitle), 26, kTitle);

  display_.setFont(u8g2_font_5x8_tf);
  display_.drawStr(centeredX(display_, kSubtitle), 40, kSubtitle);

  display_.sendBuffer();
}

void DisplayUi::showMessage(const char* line1, const char* line2) {
  if (!ready_) {
    return;
  }

  display_.clearBuffer();
  display_.setFont(u8g2_font_6x12_tf);
  display_.drawStr(0, 22, line1);

  if (line2 != nullptr) {
    display_.drawStr(0, 38, line2);
  }

  display_.sendBuffer();
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

  char voltageLine[18];
  char currentLine[18];

  snprintf(voltageLine, sizeof(voltageLine), "V %.3f V", sample.busVoltageV);
  snprintf(currentLine, sizeof(currentLine), "I %.3f mA", sample.currentMa);

  display_.clearBuffer();

  display_.setFont(u8g2_font_10x20_mf);
  display_.drawStr(0, 14, chargeLine);
  display_.drawStr(0, 31, energyLine);

  display_.setFont(u8g2_font_profont11_mf);
  display_.drawStr(0, 44, voltageLine);
  display_.drawStr(kDisplayWidth - display_.getStrWidth(batteryLine), 44, batteryLine);
  display_.drawStr(0, 56, currentLine);

  const uint8_t spinnerCenterX = kDisplayWidth - 4;
  const uint8_t spinnerCenterY = kDisplayHeight - 5;
  switch (spinnerIndex % kReadoutSpinnerFrameCount) {
    case 0:
      display_.drawLine(
          spinnerCenterX - 3, spinnerCenterY, spinnerCenterX + 3, spinnerCenterY);
      break;
    case 1:
      display_.drawLine(
          spinnerCenterX - 2,
          spinnerCenterY + 2,
          spinnerCenterX + 2,
          spinnerCenterY - 2);
      break;
    case 2:
      display_.drawLine(
          spinnerCenterX, spinnerCenterY - 3, spinnerCenterX, spinnerCenterY + 3);
      break;
    default:
      display_.drawLine(
          spinnerCenterX - 2,
          spinnerCenterY - 2,
          spinnerCenterX + 2,
          spinnerCenterY + 2);
      break;
  }

  display_.sendBuffer();
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

  snprintf(output, outputSize, "%c %.*f %s", prefix, decimals, displayValue, unit);
}

int DisplayUi::centeredX(U8G2& display, const char* text) {
  return (kDisplayWidth - display.getStrWidth(text)) / 2;
}

}  // namespace coulmeter
