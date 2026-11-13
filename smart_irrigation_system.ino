/*
 * ============================================================
 * PROJECT TITLE:
 * DESIGN AND IMPLEMENTATION OF AN ARDUINO-BASED SMART
 * IRRIGATION SYSTEM
 * ============================================================
 *
 * PROJECT DESCRIPTION:
 *
 * This project involves the design and implementation of a
 * smart irrigation system using an Arduino UNO microcontroller
 * to automate the watering of plants based on soil moisture
 * conditions.
 *
 * The system uses a capacitive soil moisture sensor to
 * continuously monitor the moisture level of the soil.
 * When the moisture level falls below a predefined threshold,
 * the Arduino automatically activates a water pump through
 * a relay module to supply water to the plants.
 *
 * Once the soil reaches the required moisture level, the
 * system automatically switches OFF the water pump to prevent
 * overwatering and unnecessary water consumption.
 *
 * A 16x2 I2C LCD displays real-time information, including
 * soil moisture percentage, pump status, operating mode,
 * and system conditions.
 *
 * The system also incorporates a water level sensor to detect
 * insufficient water in the storage tank and prevent the pump
 * from operating under dry conditions. A buzzer provides an
 * audible warning when the water tank is empty or a system
 * fault occurs.
 *
 * Additionally, push buttons allow users to switch between
 * automatic and manual operating modes and adjust the soil
 * moisture thresholds according to plant requirements.
 *
 * The system includes pump runtime protection and irrigation
 * control logic to improve reliability and reduce unnecessary
 * water usage.
 *
 * PROJECT OBJECTIVES:
 *
 * 1. To design and construct an Arduino UNO-based irrigation
 *    system capable of monitoring soil moisture and water
 *    tank level.
 *
 * 2. To develop an automatic water pump control system
 *    based on calibrated soil moisture thresholds, with
 *    manual operation and safety protection.
 *
 * 3. To evaluate the performance of the developed system
 *    in terms of soil moisture monitoring, irrigation
 *    response, pump operation, and water consumption.
 *
 * MAJOR COMPONENTS:
 *
 * - Arduino UNO R3
 * - Capacitive Soil Moisture Sensor
 * - 16x2 LCD with I2C Module
 * - 5V Relay Module
 * - 12V DC Water Pump
 * - Water Level Float Switch
 * - Push Buttons
 * - Active Buzzer
 * - 12V DC Power Supply
 * - 5V Buck Converter
 *
 * SYSTEM FEATURES:
 *
 * - Automatic soil moisture monitoring
 * - Automatic irrigation control
 * - Real-time LCD monitoring
 * - Manual and automatic operating modes
 * - Adjustable soil moisture thresholds
 * - Water tank level monitoring
 * - Dry-run pump protection
 * - Audible fault indication
 * - Maximum pump runtime protection
 * - Water-efficient irrigation operation
 *
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// -------- Pin assignments --------
const byte SOIL_PIN  = A0;
const byte FLOAT_PIN = 2;
const byte MODE_BTN  = 3;
const byte UP_BTN    = 4;
const byte DOWN_BTN  = 5;
const byte RELAY_PIN = 7;
const byte BUZZER    = 8;

// Most relay modules are active LOW.
// Change these if your module is active HIGH.
const byte RELAY_ON  = LOW;
const byte RELAY_OFF = HIGH;

// -------- Sensor calibration --------
// Replace with your measured values.
const int DRY_ADC = 800;
const int WET_ADC = 350;

// -------- Irrigation settings --------
int startThreshold = 35;
int stopThreshold  = 60;

const unsigned long PUMP_MAX_MS = 30000UL;
const unsigned long SOAK_MS = 15000UL;
const unsigned long READ_MS = 500UL;
const unsigned long LCD_MS = 1500UL;
const unsigned long DEBOUNCE_MS = 50UL;

// -------- System variables --------
bool autoMode = true;
bool pumpOn = false;
bool tankOK = false;
bool sensorOK = true;
bool soakActive = false;
bool pumpLockout = false;

int soilRaw = 0;
int moisture = 0;

unsigned long pumpStart = 0;
unsigned long soakStart = 0;
unsigned long lastRead = 0;
unsigned long lastLCD = 0;

// Button state
const byte buttonPins[3] = {
  MODE_BTN, UP_BTN, DOWN_BTN
};
bool stableButton[3] = {HIGH, HIGH, HIGH};
bool lastButtonRead[3] = {HIGH, HIGH, HIGH};
unsigned long lastChange[3] = {0, 0, 0};

// -------- Pump functions --------
void stopPump(bool beginSoak) {
  if (pumpOn) {
    digitalWrite(RELAY_PIN, RELAY_OFF);
    pumpOn = false;

    if (beginSoak) {
      soakActive = true;
      soakStart = millis();
    }
  }
}

void startPump() {
  if (!tankOK || !sensorOK || pumpLockout)
    return;

  if (soakActive)
    return;

  digitalWrite(RELAY_PIN, RELAY_ON);
  pumpOn = true;
  pumpStart = millis();
}

// -------- Read soil and tank --------
void readSensors() {
  soilRaw = analogRead(SOIL_PIN);

  // Detect likely disconnected/shorted sensor.
  // These limits must be checked against your sensor.
  sensorOK = (soilRaw > 5 && soilRaw < 1018);

  long mapped = map(
    soilRaw, DRY_ADC, WET_ADC, 0, 100
  );

  moisture = constrain(mapped, 0L, 100L);

  // LOW means sufficient water.
  tankOK = (digitalRead(FLOAT_PIN) == LOW);

  if (!tankOK || !sensorOK) {
    stopPump(false);
    soakActive = false;
  }
}

// -------- Button handling --------
bool buttonPressed(byte index) {
  bool reading = digitalRead(buttonPins[index]);

  if (reading != lastButtonRead[index]) {
    lastChange[index] = millis();
    lastButtonRead[index] = reading;
  }

  if (millis() - lastChange[index] >= DEBOUNCE_MS) {
    if (reading != stableButton[index]) {
      stableButton[index] = reading;
      if (reading == LOW)
        return true;
    }
  }
  return false;
}

void handleButtons() {
  if (buttonPressed(0)) {
    stopPump(false);
    soakActive = false;
    pumpLockout = false;
    autoMode = !autoMode;
  }

  if (buttonPressed(1)) {
    if (autoMode) {
      if (stopThreshold < 90) {
        startThreshold += 5;
        stopThreshold += 5;
      }
    } else {
      startPump();
    }
  }

  if (buttonPressed(2)) {
    if (autoMode) {
      if (startThreshold > 10) {
        startThreshold -= 5;
        stopThreshold -= 5;
      }
    } else {
      stopPump(false);
      soakActive = false;
      pumpLockout = false;
    }
  }
}

// -------- Irrigation controller --------
void controlIrrigation() {
  unsigned long now = millis();

  // Safety timeout applies to both modes.
  if (pumpOn &&
      now - pumpStart >= PUMP_MAX_MS) {
    stopPump(false);
    soakActive = false;
    pumpLockout = true;
  }

  if (!tankOK || !sensorOK) {
    stopPump(false);
    return;
  }

  if (pumpLockout)
    return;

  if (soakActive) {
    if (now - soakStart >= SOAK_MS) {
      soakActive = false;
    } else {
      return;
    }
  }

  if (!autoMode)
    return;

  if (pumpOn) {
    if (moisture >= stopThreshold) {
      stopPump(true);
    }
  } else {
    if (moisture < startThreshold) {
      startPump();
    }
  }
}

// -------- LCD output --------
void updateLCD() {
  static bool page = false;
  page = !page;

  lcd.clear();

  if (page) {
    lcd.setCursor(0, 0);
    lcd.print("SOIL:");
    lcd.print(moisture);
    lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print("PUMP:");
    lcd.print(pumpOn ? "ON " : "OFF");
    lcd.print(autoMode ? " AUTO" : " MAN");
  } else {
    lcd.setCursor(0, 0);
    if (!tankOK)
      lcd.print("TANK EMPTY");
    else if (!sensorOK)
      lcd.print("SENSOR FAULT");
    else if (pumpLockout)
      lcd.print("PUMP TIMEOUT");
    else if (soakActive)
      lcd.print("SOAKING...");
    else
      lcd.print("SYSTEM READY");

    lcd.setCursor(0, 1);
    lcd.print("ON:");
    lcd.print(startThreshold);
    lcd.print("% OFF:");
    lcd.print(stopThreshold);
    lcd.print("%");
  }
}

void setup() {
  // Keep relay OFF before enabling output.
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(RELAY_PIN, OUTPUT);

  pinMode(FLOAT_PIN, INPUT_PULLUP);
  pinMode(MODE_BTN, INPUT_PULLUP);
  pinMode(UP_BTN, INPUT_PULLUP);
  pinMode(DOWN_BTN, INPUT_PULLUP);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  Serial.begin(9600);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("SMART IRRIGATION");
  lcd.setCursor(0, 1);
  lcd.print("INITIALIZING...");

  delay(1500);

  readSensors();
  updateLCD();
}

void loop() {
  unsigned long now = millis();

  handleButtons();

  if (now - lastRead >= READ_MS) {
    lastRead = now;
    readSensors();

    Serial.print("ADC=");
    Serial.print(soilRaw);
    Serial.print(" Moisture=");
    Serial.print(moisture);
    Serial.print("% Tank=");
    Serial.print(tankOK ? "OK" : "EMPTY");
    Serial.print(" Pump=");
    Serial.println(pumpOn ? "ON" : "OFF");
  }

  controlIrrigation();

  // Continuous alarm while water is unavailable
  // or a safety fault is active.
  bool alarm = !tankOK || !sensorOK || pumpLockout;
  digitalWrite(BUZZER, alarm ? HIGH : LOW);

  if (now - lastLCD >= LCD_MS) {
    lastLCD = now;
    updateLCD();
  }
}

