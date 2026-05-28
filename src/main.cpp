#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <RF24.h>
#include <U8g2lib.h>
#include "esp_bt.h"
#include "esp_wifi.h"
#include "pins.h"

// Most cheap "0.96 OLED I2C" modules ship with the SH1106 controller, not
// SSD1306. If your screen ends up shifted ~2 px right (or still blank),
// swap this class for U8G2_SSD1306_128X64_NONAME_F_HW_I2C.
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

SPIClass hspi(HSPI);
SPIClass vspi(VSPI);
RF24 radioH(HSPI_CE, HSPI_CSN, NRF_SPI_HZ);
RF24 radioV(VSPI_CE, VSPI_CSN, NRF_SPI_HZ);

static const uint8_t CH_MAX = 80;
static uint16_t chH = 45;
static uint16_t chV = 45;
static uint32_t hopCount = 0;
static uint32_t lastUiMs = 0;
static uint32_t lastBlinkMs = 0;
static bool ledState = false;
static bool radioH_ok = false;
static bool radioV_ok = false;

static void killEspRadios() {
  esp_bt_controller_disable();
  esp_wifi_stop();
  esp_wifi_deinit();
}

static bool initRadio(RF24& r, SPIClass& bus, const char* tag) {
  if (!r.begin(&bus)) {
    Serial.printf("[%s] begin() failed\n", tag);
    return false;
  }
  r.setAutoAck(false);
  r.stopListening();
  r.setRetries(0, 0);
  r.setPALevel(RF24_PA_MAX, true);
  r.setDataRate(RF24_2MBPS);
  r.setCRCLength(RF24_CRC_DISABLED);
  r.startConstCarrier(RF24_PA_MAX, 45);
  Serial.printf("[%s] up\n", tag);
  return true;
}

static void drawUi() {
  char buf[24];
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);

  oled.drawStr(0, 8, "ESP32 2.4GHz JAMMER");
  oled.drawHLine(0, 11, 128);

  if (radioH_ok) snprintf(buf, sizeof(buf), "HSPI ch: %u", chH);
  else           snprintf(buf, sizeof(buf), "HSPI ch: FAIL");
  oled.drawStr(0, 22, buf);

  if (radioV_ok) snprintf(buf, sizeof(buf), "VSPI ch: %u", chV);
  else           snprintf(buf, sizeof(buf), "VSPI ch: FAIL");
  oled.drawStr(0, 32, buf);

  snprintf(buf, sizeof(buf), "Mode: rand 0-%u", CH_MAX - 1);
  oled.drawStr(0, 42, buf);

  snprintf(buf, sizeof(buf), "Hops: %lu", (unsigned long)hopCount);
  oled.drawStr(0, 52, buf);

  uint32_t s = millis() / 1000;
  snprintf(buf, sizeof(buf), "Up: %lum %02lus",
           (unsigned long)(s / 60), (unsigned long)(s % 60));
  oled.drawStr(0, 62, buf);

  oled.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("\n== ESP32 jammer boot =="));

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(400000);
  oled.begin();
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(0, 12, "Booting...");
  oled.sendBuffer();

  killEspRadios();

  hspi.begin(HSPI_SCK, HSPI_MISO, HSPI_MOSI, HSPI_CSN);
  vspi.begin(VSPI_SCK, VSPI_MISO, VSPI_MOSI, VSPI_CSN);

  radioH_ok = initRadio(radioH, hspi, "HSPI");
  radioV_ok = initRadio(radioV, vspi, "VSPI");

  randomSeed(esp_random());
  lastUiMs = millis();
  lastBlinkMs = millis();
}

void loop() {
  if (radioH_ok) {
    chH = random(CH_MAX);
    radioH.setChannel(chH);
  }
  if (radioV_ok) {
    chV = random(CH_MAX);
    radioV.setChannel(chV);
  }
  hopCount++;
  delayMicroseconds(random(60));

  uint32_t now = millis();

  if (now - lastBlinkMs >= 125) {
    lastBlinkMs = now;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  }

  if (now - lastUiMs >= 200) {
    lastUiMs = now;
    drawUi();
  }
}
