#pragma once

#include <Arduino.h>

namespace coulmeter {

constexpr unsigned long kSerialBaudRate = 115200;
constexpr unsigned long kSerialWaitTimeoutMs = 2000;

constexpr int kDisplayWidth = 128;
constexpr int kDisplayHeight = 64;
constexpr int kDisplayResetPin = -1;
constexpr uint8_t kDisplayI2cAddress = 0x3C;
constexpr int kDisplaySclPin = 2;
constexpr int kDisplaySdaPin = 42;
constexpr int kDisplayPowerPin = 47;
constexpr unsigned long kDisplayPowerCycleOffMs = 100;
constexpr unsigned long kDisplayPowerStabilizeMs = 1000;
constexpr unsigned long kBootSplashDurationMs = 2000;

constexpr int kBatterySensePin = 9;
constexpr float kBatteryDividerUpperOhms = 1000000.0f;
constexpr float kBatteryDividerLowerOhms = 1300000.0f;

constexpr uint8_t kIna226Address = 0x40;
constexpr unsigned long kIna226SampleIntervalMs = 1000;
constexpr uint32_t kIna226ConversionReadyTimeoutMs = 3000;
constexpr float kIna226MaxCurrentAmps = 8.0f;
constexpr float kIna226ShuntOhms = 0.01f;

constexpr size_t kReadoutSpinnerFrameCount = 4;

struct Ina226Sample {
  unsigned long timestampMs;
  float busVoltageV;
  float shuntVoltageMv;
  float currentMa;
  float powerMw;
};

}  // namespace coulmeter
