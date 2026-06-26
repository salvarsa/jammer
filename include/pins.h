#pragma once

// HSPI — NRF24L01 #1
#define HSPI_SCK   14
#define HSPI_MISO  12
#define HSPI_MOSI  13
#define HSPI_CSN   15
#define HSPI_CE    16

// VSPI — NRF24L01 #2
#define VSPI_SCK   18
#define VSPI_MISO  19
#define VSPI_MOSI  23
#define VSPI_CSN   21
#define VSPI_CE    22

// OLED 0.96" SSD1306 I2C
#define OLED_SDA   4
#define OLED_SCL   5
#define OLED_W     128
#define OLED_H     64
#define OLED_ADDR  0x3C

// Status LEDs
#define LED_PIN     27   // Heartbeat / encendido
#define ANT_LED_PIN 25   // Testigo antenas: OFF = DISARMED, ON = ARMED

// SPI clock for NRF24
#define NRF_SPI_HZ 16000000
