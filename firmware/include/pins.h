#pragma once
// Pin map for the Unexpected Maker FeatherS3[D] (16 MB flash, 8 MB QSPI PSRAM).
// See hardware/wiring.md. Change pins here only.
//
// The board breaks out 21 GPIOs. SDA/SCL (IO8/IO9) also go to the on-board
// MAX17048 fuel gauge, so 19 remain for 19 signals. The octal-PSRAM pins of
// other S3 modules (IO33-IO37) are free here, the PSRAM is a separate QSPI chip.
//
// Buttons and card detect sit on RTC GPIOs (IO0-IO21) so the buttons can wake
// the chip from deep sleep. IO0 is also the BOOT button: holding BACK while
// pressing RESET starts the ROM bootloader.

// ---- Waveshare 4.2" V2 e-paper (SPI2) ----
#define PIN_EPD_SCK   36   // SCK
#define PIN_EPD_MOSI  35   // MO
#define PIN_EPD_CS    44   // RX
#define PIN_EPD_DC    43   // TX
#define PIN_EPD_RST   37   // MI
#define PIN_EPD_BUSY  38   // 38
#define PIN_EPD_PWR   33   // 33; Rev2.2 power enable, -1 if your module has no PWR pin

// ---- microSD (SDMMC, 4-bit, any GPIO via the matrix) ----
#define PIN_SD_CLK    17   // 17
#define PIN_SD_CMD    18   // 18
#define PIN_SD_D0     14   // 14
#define PIN_SD_D1     12   // 12
#define PIN_SD_D2     6    // 6
#define PIN_SD_D3     5    // 5
#define PIN_SD_DET    3    // 3; -1 if not wired (a missing card then shows as a mount error)
#define SD_DET_ACTIVE HIGH // Adafruit 4682: DET is shorted to GND without a card, pulled up with one

// ---- buttons (active low, RTC GPIOs so they can wake from deep sleep) ----
#define PIN_BTN_PREV  11   // 11
#define PIN_BTN_NEXT  10   // 10
#define PIN_BTN_MENU  7    // 7
#define PIN_BTN_OK    1    // 1
#define PIN_BTN_BACK  0    // 0 (BOOT button)

// ---- on-board ----
#define PIN_I2C_SDA     8    // MAX17048 fuel gauge (I2C1 STEMMA QT, on the always-on LDO1)
#define PIN_I2C_SCL     9
#define PIN_LDO2        39   // high = LDO2 on: RGB LED and the I2C2 STEMMA QT; kept low to save power
#define PIN_LED         13   // blue LED, kept off
#define PIN_VBUS_SENSE  34   // high = 5 V present on USB-C
#define PIN_RF_SWITCH   41   // low = on-board antenna, high = u.FL
#define PIN_LIGHT_SENSE 4    // ambient light sensor (analog), not used yet
