#pragma once
// Pin map for the Adafruit ESP32-S3 Feather, 4 MB flash / 2 MB PSRAM
// (Adafruit 5477). See hardware/wiring.md. Change pins here only.
//
// The board breaks out 20 GPIOs. SDA/SCL are taken by the on-board battery
// monitor, so 18 remain for 18 signals: card detect is not wired, and the
// display's PWR line shares IO13 with the red LED (it lights while the
// display is powered, i.e. while the reader is awake).

// ---- Waveshare 4.2" V2 e-paper (SPI2) ----
#define PIN_EPD_SCK   36   // SCK
#define PIN_EPD_MOSI  35   // MO
#define PIN_EPD_CS    38   // RX
#define PIN_EPD_DC    39   // TX
#define PIN_EPD_RST   37   // MI
#define PIN_EPD_BUSY  12   // 12
#define PIN_EPD_PWR   13   // 13 (red LED); Rev2.2 power enable, -1 if your module has no PWR pin

// ---- microSD (SDMMC, 4-bit, any GPIO via the matrix) ----
#define PIN_SD_CLK    18   // A0
#define PIN_SD_CMD    17   // A1
#define PIN_SD_D0     16   // A2
#define PIN_SD_D1     15   // A3
#define PIN_SD_D2     14   // A4
#define PIN_SD_D3     8    // A5
#define PIN_SD_DET    -1   // card detect not wired (no pin left); a missing card shows as a mount error
#define SD_DET_ACTIVE LOW

// ---- buttons (active low, RTC GPIOs so they can wake from deep sleep) ----
#define PIN_BTN_PREV  5    // 5
#define PIN_BTN_NEXT  6    // 6
#define PIN_BTN_MENU  9    // 9
#define PIN_BTN_OK    10   // 10
#define PIN_BTN_BACK  11   // 11

// ---- on-board ----
#define PIN_I2C_SDA        3    // battery monitor (MAX17048 or LC709203F) + STEMMA QT
#define PIN_I2C_SCL        4
#define PIN_I2C_POWER      7    // high = I2C pull-ups / STEMMA QT powered (needed to talk to the monitor)
#define PIN_NEOPIXEL_POWER 21   // high = NeoPixel powered; kept low to save power
