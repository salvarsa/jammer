#include <Arduino.h>
#include <Wire.h>
#include "pins.h"

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n== I2C scanner =="));
  Serial.printf("SDA=GPIO%d  SCL=GPIO%d\n", OLED_SDA, OLED_SCL);

  // Internal pull-ups on, in case the OLED module lacks them.
  pinMode(OLED_SDA, INPUT_PULLUP);
  pinMode(OLED_SCL, INPUT_PULLUP);

  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(100000);  // slow & safe for diagnostics
}

void loop() {
  uint8_t found = 0;
  Serial.println(F("Scanning..."));
  for (uint8_t addr = 1; addr < 127; ++addr) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("  found 0x%02X\n", addr);
      found++;
    } else if (err == 4) {
      Serial.printf("  unknown err at 0x%02X\n", addr);
    }
  }
  if (!found) Serial.println(F("  (no devices)"));
  Serial.printf("Done. %u device(s).\n\n", found);
  delay(3000);
}
