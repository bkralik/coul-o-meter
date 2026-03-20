#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <INA226.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include "wifi_config.h"

namespace {
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
constexpr unsigned long kWifiReconnectIntervalMs = 10000;
constexpr int kBatterySensePin = 9;
constexpr float kBatteryDividerUpperOhms = 1000000.0f;
constexpr float kBatteryDividerLowerOhms = 1300000.0f;
constexpr uint8_t kIna226Address = 0x40;
constexpr unsigned long kIna226SampleIntervalMs = 1000;
constexpr uint32_t kIna226ConversionReadyTimeoutMs = 3000;
constexpr float kIna226MaxCurrentAmps = 8.0f;
constexpr float kIna226ShuntOhms = 0.01f;
constexpr char kReadoutSpinnerChars[] = {'-', '/', '|', '\\'};
constexpr size_t kReadoutSpinnerCharCount =
    sizeof(kReadoutSpinnerChars) / sizeof(kReadoutSpinnerChars[0]);

struct Ina226Sample {
  unsigned long timestampMs;
  float busVoltageV;
  float shuntVoltageMv;
  float currentMa;
  float powerMw;
};

Adafruit_SSD1306 display(kDisplayWidth, kDisplayHeight, &Wire, kDisplayResetPin);
INA226 ina226(kIna226Address, &Wire);
WebServer server(80);
bool displayReady = false;
bool ina226Ready = false;
bool hasIna226Sample = false;
bool httpServerStarted = false;
bool hasLatestMeasurement = false;
unsigned long lastIna226PollMs = 0;
unsigned long lastWifiConnectAttemptMs = 0;
size_t readoutSpinnerIndex = 0;
float lastBatteryVoltageV = 0.0f;
Ina226Sample latestIna226Sample = {};
Ina226Sample previousIna226Sample = {};
double accumulatedChargeMah = 0.0;
double accumulatedEnergyMwh = 0.0;

void logBootStep(const char* message) {
  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.println(message);
}

void updateLatestMeasurement(const Ina226Sample& sample) {
  latestIna226Sample = sample;
  hasLatestMeasurement = true;
}

float readBatteryVoltageV() {
  constexpr int kBatterySamples = 4;
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

bool readIna226Sample(Ina226Sample& sample) {
  if (!ina226.waitConversionReady(kIna226ConversionReadyTimeoutMs)) {
    return false;
  }

  sample = {};
  sample.timestampMs = millis();
  sample.busVoltageV = ina226.getBusVoltage();
  sample.shuntVoltageMv = ina226.getShuntVoltage_mV();
  sample.currentMa = ina226.getCurrent_mA();
  sample.powerMw = ina226.getPower_mW();
  lastBatteryVoltageV = readBatteryVoltageV();
  return true;
}

void integrateIna226Sample(const Ina226Sample& sample) {
  if (!hasIna226Sample) {
    previousIna226Sample = sample;
    hasIna226Sample = true;
    return;
  }

  const double deltaHours =
      static_cast<double>(sample.timestampMs - previousIna226Sample.timestampMs) /
      3600000.0;
  const double averageCurrentMa =
      (static_cast<double>(previousIna226Sample.currentMa) + sample.currentMa) / 2.0;
  const double averagePowerMw =
      (static_cast<double>(previousIna226Sample.powerMw) + sample.powerMw) / 2.0;

  accumulatedChargeMah += averageCurrentMa * deltaHours;
  accumulatedEnergyMwh += averagePowerMw * deltaHours;
  previousIna226Sample = sample;
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
  Serial.print(accumulatedChargeMah, 6);
  Serial.print(" mAh, energy=");
  Serial.print(accumulatedEnergyMwh, 6);
  Serial.println(" mWh");
}

String buildMeasurementResponse() {
  if (!hasLatestMeasurement) {
    return "measurements_available=0\n";
  }

  String response;
  response.reserve(256);
  response += "measurements_available=1\n";
  response += "timestamp_ms=";
  response += latestIna226Sample.timestampMs;
  response += "\n";
  response += "bus_voltage_v=";
  response += String(latestIna226Sample.busVoltageV, 3);
  response += "\n";
  response += "shunt_voltage_mv=";
  response += String(latestIna226Sample.shuntVoltageMv, 3);
  response += "\n";
  response += "current_ma=";
  response += String(latestIna226Sample.currentMa, 3);
  response += "\n";
  response += "power_mw=";
  response += String(latestIna226Sample.powerMw, 3);
  response += "\n";
  response += "charge_mah=";
  response += String(accumulatedChargeMah, 6);
  response += "\n";
  response += "energy_mwh=";
  response += String(accumulatedEnergyMwh, 6);
  response += "\n";
  response += "battery_voltage_v=";
  response += String(lastBatteryVoltageV, 3);
  response += "\n";
  return response;
}

void handleMeasurementsRequest() {
  server.send(200, "text/plain; charset=utf-8", buildMeasurementResponse());
}

void beginHttpServer() {
  if (httpServerStarted || WiFi.status() != WL_CONNECTED) {
    return;
  }

  server.begin();
  httpServerStarted = true;
  Serial.print("HTTP server ready at http://");
  Serial.println(WiFi.localIP());
}

void startWifiConnection() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(kWifiSsid, kWifiPassword);
  lastWifiConnectAttemptMs = millis();

  Serial.print("Connecting to WiFi SSID ");
  Serial.println(kWifiSsid);
}

void maintainWifiConnection() {
  if (WiFi.status() == WL_CONNECTED) {
    beginHttpServer();
    return;
  }

  if ((millis() - lastWifiConnectAttemptMs) >= kWifiReconnectIntervalMs) {
    startWifiConnection();
  }
}

void showDisplayMessage(const char* line1, const char* line2 = nullptr) {
  if (!displayReady) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 16);
  display.println(line1);

  if (line2 != nullptr) {
    display.setCursor(0, 32);
    display.println(line2);
  }

  display.display();
}

void formatDisplayQuantity(
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

void advanceReadoutSpinner() {
  readoutSpinnerIndex = (readoutSpinnerIndex + 1) % kReadoutSpinnerCharCount;
}

void renderMeasurementScreen(const Ina226Sample& sample) {
  if (!displayReady) {
    return;
  }

  char chargeLine[20];
  char energyLine[20];
  char batteryLine[12];
  formatDisplayQuantity(
      'Q', accumulatedChargeMah, "mAh", "Ah", chargeLine, sizeof(chargeLine));
  formatDisplayQuantity(
      'E', accumulatedEnergyMwh, "mWh", "Wh", energyLine, sizeof(energyLine));
  snprintf(batteryLine, sizeof(batteryLine), "B%.2fV", lastBatteryVoltageV);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);

  display.setCursor(0, 0);
  display.println(chargeLine);

  display.setCursor(0, 18);
  display.println(energyLine);

  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print("V ");
  display.print(sample.busVoltageV, 3);
  display.println(" V");

  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t textWidth = 0;
  uint16_t textHeight = 0;
  display.getTextBounds(batteryLine, 0, 40, &x1, &y1, &textWidth, &textHeight);
  display.setCursor(kDisplayWidth - textWidth, 40);
  display.println(batteryLine);

  display.setCursor(0, 52);
  display.print("I ");
  display.print(sample.currentMa, 3);
  display.println(" mA");

  if (WiFi.status() == WL_CONNECTED) {
    display.setCursor(kDisplayWidth - 12, kDisplayHeight - 8);
    display.print('W');
  }

  display.setCursor(kDisplayWidth - 6, kDisplayHeight - 8);
  display.print(kReadoutSpinnerChars[readoutSpinnerIndex]);

  display.display();
}

void showBootSplash() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t textWidth = 0;
  uint16_t textHeight = 0;
  display.getTextBounds("coulmeter", 0, 0, &x1, &y1, &textWidth, &textHeight);

  const int16_t x = (kDisplayWidth - static_cast<int16_t>(textWidth)) / 2;
  const int16_t y = (kDisplayHeight - static_cast<int16_t>(textHeight)) / 2;

  display.setCursor(x, y);
  display.print("coulmeter");
  display.display();
}
}  // namespace

void setup() {
  pinMode(kDisplayPowerPin, OUTPUT);
  digitalWrite(kDisplayPowerPin, HIGH);
  delay(kDisplayPowerCycleOffMs);

  Serial.begin(kSerialBaudRate);
  analogSetPinAttenuation(kBatterySensePin, ADC_11db);

  const unsigned long serialWaitStart = millis();
  while (!Serial && (millis() - serialWaitStart) < kSerialWaitTimeoutMs) {
    delay(10);
  }

  digitalWrite(kDisplayPowerPin, HIGH);
  logBootStep("Display power enabled");
  delay(kDisplayPowerStabilizeMs);
  logBootStep("Display power stabilized");

  Wire.begin(kDisplaySdaPin, kDisplaySclPin);
  displayReady = display.begin(SSD1306_SWITCHCAPVCC, kDisplayI2cAddress);
  if (displayReady) {
    logBootStep("Showing boot splash");
    showBootSplash();
    delay(kBootSplashDurationMs);
    display.clearDisplay();
    display.display();
  } else {
    Serial.println("SSD1315 init failed");
  }

  server.on("/", handleMeasurementsRequest);
  server.on("/metrics", handleMeasurementsRequest);
  startWifiConnection();

  ina226Ready = ina226.begin();
  if (ina226Ready) {
    if (!ina226.setAverage(INA226_1024_SAMPLES)) {
      ina226Ready = false;
      Serial.println("INA226 averaging config failed");
      showDisplayMessage("INA226 averaging", "config failed");
    } else {
      const int calibrationError =
          ina226.setMaxCurrentShunt(kIna226MaxCurrentAmps, kIna226ShuntOhms);
      if (calibrationError == 0) {
        Serial.println("INA226 init OK");
        Ina226Sample sample = {};
        if (readIna226Sample(sample)) {
          integrateIna226Sample(sample);
          updateLatestMeasurement(sample);
          logIna226Readings(sample);
          advanceReadoutSpinner();
          renderMeasurementScreen(sample);
          lastIna226PollMs = sample.timestampMs;
        } else {
          Serial.println("INA226 conversion timeout");
          showDisplayMessage("INA226 conversion", "timeout");
        }
      } else {
        ina226Ready = false;
        Serial.print("INA226 calibration failed: ");
        Serial.println(calibrationError);
        showDisplayMessage("INA226 calibration", "failed");
      }
    }
  } else {
    Serial.println("INA226 init failed");
    showDisplayMessage("INA226 init failed");
  }

  Serial.println();
  Serial.println("Hello from ESP32-S3");
  Serial.print("Free RAM: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
}

void loop() {
  maintainWifiConnection();
  if (httpServerStarted) {
    server.handleClient();
  }

  if (ina226Ready && (millis() - lastIna226PollMs) >= kIna226SampleIntervalMs) {
    Ina226Sample sample = {};
    if (readIna226Sample(sample)) {
      integrateIna226Sample(sample);
      updateLatestMeasurement(sample);
      logIna226Readings(sample);
      advanceReadoutSpinner();
      renderMeasurementScreen(sample);
      lastIna226PollMs = sample.timestampMs;
    } else {
      Serial.println("INA226 conversion timeout");
      lastIna226PollMs = millis();
    }
  }
}
