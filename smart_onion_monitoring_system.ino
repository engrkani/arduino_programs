
/*
  PROJECT TITLE:
  ARDUINO-BASED SMART ONION STORAGE
  MONITORING AND CONTROL SYSTEM

  Developed for an academic Arduino UNO prototype.

  COMPONENTS:
  Arduino UNO
  DHT22 Temperature and Humidity Sensor
  MQ-135 Air Quality Sensor
  20x4 I2C LCD
  5V Relay Module
  12V DC Ventilation Fan
  Active Buzzer
  Green, Yellow and Red LEDs
  Two Push Buttons

  FUNCTIONS:
  1. Monitor temperature and humidity.
  2. Read MQ-135 analog sensor output.
  3. Display measurements on LCD.
  4. Control ventilation fan automatically.
  5. Activate alarms for abnormal conditions.
  6. Adjust temperature and humidity limits.
  7. Detect DHT22 sensor failure.

  IMPORTANT:
  MQ-135 readings are raw analog values,
  NOT calibrated gas concentration in ppm.

  Temperature and humidity thresholds are
  experimental and must be validated for
  the actual onion storage conditions.

  Design By Engr. Kani
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <math.h>

// ---------- PIN CONFIGURATION ----------

#define DHTPIN 2
#define DHTTYPE DHT22

#define MQ135_PIN A0

#define RELAY_PIN 7
#define BUZZER_PIN 8

#define GREEN_LED 9
#define YELLOW_LED 10
#define RED_LED 11

#define TEMP_BUTTON 3
#define HUM_BUTTON 4

// ---------- LCD AND SENSOR ----------

LiquidCrystal_I2C lcd(0x27, 20, 4);
DHT dht(DHTPIN, DHTTYPE);

// ---------- RELAY SETTINGS ----------

// Most common relay modules are active LOW.
// Change to false for active HIGH modules.

const bool RELAY_ACTIVE_LOW = true;

// ---------- MONITORING LIMITS ----------

// Experimental starting thresholds.
// Adjust after testing the storage chamber.

float tempLimit = 30.0;
float humLimit = 70.0;

// Fan hysteresis
const float TEMP_HYST = 2.0;
const float HUM_HYST = 5.0;

// Experimental MQ-135 warning threshold
int gasLimit = 600;

// ---------- SENSOR VARIABLES ----------

float temperature = NAN;
float humidity = NAN;

int gasValue = 0;

bool sensorOK = false;
bool fanON = false;
bool alarmActive = false;
bool alarmMuted = false;

bool tempHigh = false;
bool humHigh = false;
bool gasHigh = false;

// ---------- TIMING ----------

unsigned long previousRead = 0;
unsigned long previousDisplay = 0;
unsigned long previousSerial = 0;

const unsigned long READ_INTERVAL = 2500;
const unsigned long DISPLAY_INTERVAL = 1000;
const unsigned long SERIAL_INTERVAL = 2500;

// ---------- BUTTON DEBOUNCE ----------

bool lastTempReading = HIGH;
bool lastHumReading = HIGH;

bool stableTempState = HIGH;
bool stableHumState = HIGH;

unsigned long tempDebounceTime = 0;
unsigned long humDebounceTime = 0;

const unsigned long DEBOUNCE_DELAY = 50;

// ---------- RELAY CONTROL ----------

void setFan(bool state) {

  fanON = state;

  if (RELAY_ACTIVE_LOW) {

    digitalWrite(
      RELAY_PIN,
      state ? LOW : HIGH
    );

  } else {

    digitalWrite(
      RELAY_PIN,
      state ? HIGH : LOW
    );
  }
}

// ---------- LCD LINE FUNCTION ----------

void printLCDLine(
  byte row,
  String message
) {

  if (message.length() > 20) {
    message = message.substring(0, 20);
  }

  lcd.setCursor(0, row);
  lcd.print(message);

  for (int i = message.length(); i < 20; i++) {
    lcd.print(" ");
  }
}

// ---------- SETUP ----------

void setup() {

  Serial.begin(9600);

  // Set relay inactive before enabling output.
  digitalWrite(
    RELAY_PIN,
    RELAY_ACTIVE_LOW ? HIGH : LOW
  );

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  pinMode(TEMP_BUTTON, INPUT_PULLUP);
  pinMode(HUM_BUTTON, INPUT_PULLUP);

  digitalWrite(BUZZER_PIN, LOW);

  setFan(false);

  dht.begin();

  lcd.init();
  lcd.backlight();

  printLCDLine(0, "SMART ONION STORAGE");
  printLCDLine(1, "MONITORING SYSTEM");
  printLCDLine(2, "Arduino UNO");
  printLCDLine(3, "Initializing...");

  delay(2000);

  lcd.clear();

  Serial.println("SMART ONION MONITOR");
  Serial.println("System Started");

  // First sensor reading
  readSensors();
  controlSystem();
  updateDisplay();

  previousRead = millis();
  previousDisplay = millis();
}

// ---------- MAIN LOOP ----------

void loop() {

  unsigned long currentTime = millis();

  checkButtons();

  if (currentTime - previousRead >= READ_INTERVAL) {

    previousRead = currentTime;

    readSensors();
    controlSystem();
  }

  if (currentTime - previousDisplay >= DISPLAY_INTERVAL) {

    previousDisplay = currentTime;

    updateDisplay();
  }

  if (currentTime - previousSerial >= SERIAL_INTERVAL) {

    previousSerial = currentTime;

    printSerialData();
  }
}

// ---------- SENSOR READING ----------

void readSensors() {

  float newTemp = dht.readTemperature();
  float newHum = dht.readHumidity();

  gasValue = analogRead(MQ135_PIN);

  if (isnan(newTemp) || isnan(newHum)) {

    sensorOK = false;

    Serial.println("ERROR: DHT22 reading failed");

    return;
  }

  temperature = newTemp;
  humidity = newHum;

  sensorOK = true;
}

// ---------- AUTOMATIC CONTROL ----------

void controlSystem() {

  // If DHT22 fails, stop automatic fan control.
  // A real installation needs a validated
  // fail-safe ventilation strategy.

  if (!sensorOK) {

    setFan(false);

    alarmActive = true;

    updateIndicators();

    return;
  }

  tempHigh = temperature >= tempLimit;
  humHigh = humidity >= humLimit;
  gasHigh = gasValue >= gasLimit;

  // Start fan if temperature or humidity
  // exceeds the selected threshold.

  if (!fanON) {

    if (tempHigh || humHigh) {
      setFan(true);
    }

  } else {

    // Keep fan ON until BOTH readings
    // have fallen below their reset levels.

    bool tempSafe =
      temperature <= (tempLimit - TEMP_HYST);

    bool humSafe =
      humidity <= (humLimit - HUM_HYST);

    if (tempSafe && humSafe) {
      setFan(false);
    }
  }

  // Gas sensor triggers warning only.
  // It is not a validated spoilage detector.

  alarmActive = tempHigh || humHigh || gasHigh;

  if (!alarmActive) {
    alarmMuted = false;
  }

  updateIndicators();
}

// ---------- LED AND BUZZER ----------

void updateIndicators() {

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);

  if (!sensorOK) {

    digitalWrite(RED_LED, HIGH);

  } else if (gasHigh) {

    digitalWrite(RED_LED, HIGH);

  } else if (tempHigh || humHigh) {

    digitalWrite(YELLOW_LED, HIGH);

  } else {

    digitalWrite(GREEN_LED, HIGH);
  }

  if (alarmActive && !alarmMuted) {

    digitalWrite(BUZZER_PIN, HIGH);

  } else {

    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ---------- LCD DISPLAY ----------

void updateDisplay() {

  printLCDLine(
    0,
    "SMART ONION MONITOR"
  );

  if (!sensorOK) {

    printLCDLine(1, "DHT22 SENSOR ERROR");
    printLCDLine(2, "Check Connections");
    printLCDLine(3, "FAN:OFF  ALARM:ON");

    return;
  }

  String line1 =
    "T:" + String(temperature, 1) +
    "C H:" + String(humidity, 1) + "%";

  printLCDLine(1, line1);

  String line2 =
    "AIR RAW:" + String(gasValue);

  printLCDLine(2, line2);

  String status;

  if (gasHigh) {

    status = "AIR WARNING";

  } else if (tempHigh || humHigh) {

    status = "WARNING";

  } else {

    status = "NORMAL";
  }

  String line3 =
    String("F:") +
    (fanON ? "ON " : "OFF") +
    " " + status;

  printLCDLine(3, line3);
}

// ---------- BUTTON CONTROLS ----------

void checkButtons() {

  bool tempReading = digitalRead(TEMP_BUTTON);
  bool humReading = digitalRead(HUM_BUTTON);

  unsigned long now = millis();

  // Temperature button debounce

  if (tempReading != lastTempReading) {
    tempDebounceTime = now;
  }

  if ((now - tempDebounceTime) > DEBOUNCE_DELAY) {

    if (tempReading != stableTempState) {

      stableTempState = tempReading;

      if (stableTempState == LOW) {

        tempLimit += 1.0;

        if (tempLimit > 40.0) {
          tempLimit = 25.0;
        }

        Serial.print("New Temperature Limit: ");
        Serial.println(tempLimit);

        controlSystem();
        updateDisplay();
      }
    }
  }

  lastTempReading = tempReading;

  // Humidity button debounce

  if (humReading != lastHumReading) {
    humDebounceTime = now;
  }

  if ((now - humDebounceTime) > DEBOUNCE_DELAY) {

    if (humReading != stableHumState) {

      stableHumState = humReading;

      if (stableHumState == LOW) {

        humLimit += 5.0;

        if (humLimit > 85.0) {
          humLimit = 55.0;
        }

        Serial.print("New Humidity Limit: ");
        Serial.println(humLimit);

        controlSystem();
        updateDisplay();
      }
    }
  }

  lastHumReading = humReading;
}

// ---------- SERIAL MONITOR ----------

void printSerialData() {

  Serial.println("----------------------");

  if (!sensorOK) {

    Serial.println("DHT22 SENSOR ERROR");
    Serial.print("MQ135 Raw: ");
    Serial.println(gasValue);

    return;
  }

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("MQ135 Raw: ");
  Serial.println(gasValue);

  Serial.print("Fan: ");
  Serial.println(fanON ? "ON" : "OFF");

  Serial.print("Temperature Limit: ");
  Serial.println(tempLimit);

  Serial.print("Humidity Limit: ");
  Serial.println(humLimit);

  Serial.print("Alarm: ");
  Serial.println(alarmActive ? "ACTIVE" : "NORMAL");
}
