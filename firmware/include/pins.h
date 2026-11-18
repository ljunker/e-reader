#pragma once
// Pin map for the Unexpected Maker FeatherS3. See hardware/wiring.md.
// Change pins here only.

// ---- Waveshare 4.2" V2 e-paper (SPI2) ----
#define PIN_EPD_SCK   36
#define PIN_EPD_MOSI  35
#define PIN_EPD_CS    38
#define PIN_EPD_DC    33
#define PIN_EPD_RST   43
#define PIN_EPD_BUSY  44
#define PIN_EPD_PWR   18   // Rev2.2 power enable; -1 if your module has no PWR pin

// ---- microSD (SDMMC, 4-bit) ----
#define PIN_SD_CLK    12
#define PIN_SD_CMD    11
#define PIN_SD_D0     10
#define PIN_SD_D1     9
#define PIN_SD_D2     17
#define PIN_SD_D3     8
#define PIN_SD_DET    3    // card detect, -1 if not wired
#define SD_DET_ACTIVE LOW

// ---- buttons (active low, RTC GPIOs so they can wake from deep sleep) ----
#define PIN_BTN_PREV  14
#define PIN_BTN_NEXT  1
#define PIN_BTN_MENU  5
#define PIN_BTN_OK    6
#define PIN_BTN_BACK  7

// ---- on-board (FeatherS3) ----
#define PIN_VBAT_SENSE 2
#define PIN_VBUS_SENSE 34
#define PIN_LDO2       39   // second LDO / RGB LED power, switched off to save power
