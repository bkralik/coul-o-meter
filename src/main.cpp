#include <Arduino.h>
#include <Wire.h>

#include "battery_monitor.h"
#include "coulmeter_config.h"
#include "display_ui.h"
#include "power_monitor.h"

namespace {

using coulmeter::BatteryMonitor;
using coulmeter::DisplayUi;
using coulmeter::Ina226Sample;
using coulmeter::PowerMonitor;

BatteryMonitor batteryMonitor;
DisplayUi displayUi;
PowerMonitor powerMonitor;
bool powerMonitorReady = false;
unsigned long lastIna226PollMs = 0;
size_t readoutSpinnerIndex = 0;
float lastBatteryVoltageV = 0.0f;

void logBootStep(const char* message) {
  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.println(message);
}

void advanceReadoutSpinner() {
  readoutSpinnerIndex =
      (readoutSpinnerIndex + 1) % coulmeter::kReadoutSpinnerCharCount;
}

void logIna226Readings(const Ina226Sample& sample) {
  Serial.print("[");
  Serial.print(sample.timestampMs);
  Serial.print(" ms] INA226 bus=");
  Serial.print(sample.busVoltageV, 3);
  Serial.print(" V, shunt=");
  Serial.print(sample.shuntVoltageMv, 3);
  Serial.print(" mV, current=");
  Serial.print(sample.currentMa, 3);
  Serial.print(" mA, power=");
  Serial.print(sample.powerMw, 3);
  Serial.print(" mW, bat=");
  Serial.print(lastBatteryVoltageV, 3);
  Serial.print(" V, charge=");
  Serial.print(powerMonitor.chargeMah(), 6);
  Serial.print(" mAh, energy=");
  Serial.print(powerMonitor.energyMwh(), 6);
  Serial.println(" mWh");
}

void renderMeasurementScreen(const Ina226Sample& sample) {
  displayUi.renderMeasurementScreen(
      sample,
      powerMonitor.chargeMah(),
      powerMonitor.energyMwh(),
      lastBatteryVoltageV,
      readoutSpinnerIndex);
}

void processSuccessfulSample(const Ina226Sample& sample) {
  lastBatteryVoltageV = batteryMonitor.readVoltageV();
  powerMonitor.integrateSample(sample);
  logIna226Readings(sample);
  advanceReadoutSpinner();
  renderMeasurementScreen(sample);
  lastIna226PollMs = sample.timestampMs;
}

}  // namespace

void setup() {
  pinMode(coulmeter::kDisplayPowerPin, OUTPUT);
  digitalWrite(coulmeter::kDisplayPowerPin, HIGH);
  delay(coulmeter::kDisplayPowerCycleOffMs);

  Serial.begin(coulmeter::kSerialBaudRate);
  batteryMonitor.begin();

  const unsigned long serialWaitStart = millis();
  while (!Serial &&
         (millis() - serialWaitStart) < coulmeter::kSerialWaitTimeoutMs) {
    delay(10);
  }

  digitalWrite(coulmeter::kDisplayPowerPin, HIGH);
  logBootStep("Display power enabled");
  delay(coulmeter::kDisplayPowerStabilizeMs);
  logBootStep("Display power stabilized");

  Wire.begin(coulmeter::kDisplaySdaPin, coulmeter::kDisplaySclPin);
  if (displayUi.begin()) {
    logBootStep("Showing boot splash");
    displayUi.showBootSplash();
    delay(coulmeter::kBootSplashDurationMs);
    displayUi.clear();
  } else {
    Serial.println("SSD1315 init failed");
  }

  powerMonitorReady = powerMonitor.begin();
  if (powerMonitorReady) {
    Serial.println("INA226 init OK");
    Ina226Sample sample = {};
    if (powerMonitor.readSample(sample)) {
      processSuccessfulSample(sample);
    } else {
      Serial.println("INA226 conversion timeout");
      displayUi.showMessage("INA226 conversion", "timeout");
    }
  } else {
    Serial.println("INA226 init failed");
    displayUi.showMessage("INA226 init failed");
  }

  Serial.println();
  Serial.println("Hello from ESP32-S3");
  Serial.print("Free RAM: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
}

void loop() {
  if (powerMonitorReady &&
      (millis() - lastIna226PollMs) >= coulmeter::kIna226SampleIntervalMs) {
    Ina226Sample sample = {};
    if (powerMonitor.readSample(sample)) {
      processSuccessfulSample(sample);
    } else {
      Serial.println("INA226 conversion timeout");
      lastIna226PollMs = millis();
    }
  }
}
