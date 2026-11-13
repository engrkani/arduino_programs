

#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

Servo tracker;
LiquidCrystal_I2C lcd(0x27, 16, 2);

const int ldrLeft = A0;
const int ldrRight = A1;
const int servoPin = 9;

int angle = 90;
const int tolerance = 40;
const int minAngle = 10;
const int maxAngle = 170;

void setup() {
  Serial.begin(9600);

  tracker.attach(servoPin);
  tracker.write(angle);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("SOLAR TRACKER");
  delay(2000);
  lcd.clear();
}

void loop() {
  int leftValue = analogRead(ldrLeft);
  int rightValue = analogRead(ldrRight);

  int difference = leftValue - rightValue;

  if (abs(difference) > tolerance) {
    if (difference > 0) {
      angle++;
    } else {
      angle--;
    }
  }

  angle = constrain(angle, minAngle, maxAngle);
  tracker.write(angle);

  lcd.setCursor(0, 0);
  lcd.print("L:");
  lcd.print(leftValue);
  lcd.print(" R:");
  lcd.print(rightValue);
  lcd.print("   ");

  lcd.setCursor(0, 1);
  lcd.print("Angle:");
  lcd.print(angle);
  lcd.print((char)223);
  lcd.print("       ");

  Serial.print("Left: ");
  Serial.print(leftValue);
  Serial.print(" Right: ");
  Serial.print(rightValue);
  Serial.print(" Angle: ");
  Serial.println(angle);

  delay(150);
}
