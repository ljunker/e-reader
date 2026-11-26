# Wiring

All pin numbers live in [`firmware/include/pins.h`](../firmware/include/pins.h) — change them there, not in code.
The board is the **Adafruit ESP32-S3 Feather, 4 MB flash / 2 MB PSRAM** (Adafruit 5477).
"Feather pin" below is the label printed on the board, the GPIO number is in brackets.

> ⚠️ Check every pin against Adafruit's pinout diagram for the ESP32-S3 Feather before soldering.
> On the board itself: SDA (IO3) / SCL (IO4) go to the battery monitor, IO7 switches the I2C power,
> IO21 the NeoPixel power, IO13 drives the red LED.

The Feather breaks out 20 GPIOs; SDA and SCL are taken by the battery monitor, so exactly 18 are left
for the 18 signals below. That is why the microSD card-detect pin is not wired and the display's PWR
line shares pin 13 with the red LED (the LED is on while the reader is awake).

## Display (Waveshare 4.2" V2, 9-pin header)

| Display pin | Feather pin | Notes |
|---|---|---|
| VCC | 3V | Module accepts 3.3 V or 5 V; use 3.3 V |
| GND | GND | |
| DIN | MO (IO35) | SPI2 |
| CLK | SCK (IO36) | SPI2 |
| CS  | RX (IO38) | Serial goes over native USB, the UART pins are free |
| DC  | TX (IO39) | |
| RST | MI (IO37) | MISO is not needed, the display is write-only |
| BUSY | 12 (IO12) | input, high = busy on SSD1683 |
| PWR | 13 (IO13) | Rev2.2 only. High = module powered. Driven low and held before deep sleep. If your header has no PWR pin, leave pin 13 unconnected. |

## microSD (Adafruit 4682, SDIO 4-bit)

| Breakout | Feather pin |
|---|---|
| 3V | 3V |
| GND | GND |
| CLK | A0 (IO18) |
| SI / CMD | A1 (IO17) |
| SO / DAT0 | A2 (IO16) |
| DAT1 | A3 (IO15) |
| DAT2 | A4 (IO14) |
| CS / DAT3 | A5 (IO8) |
| DET (card detect) | not wired (no pin left); a missing card shows as "Cannot read the microSD card" |

## Buttons (active low, internal pull-ups, all RTC GPIOs → can wake from deep sleep)

| Button | Feather pin | Position on case |
|---|---|---|
| PREV | 5 (IO5)  | grip side (left, or right if `left_handed = false`), upper |
| NEXT | 6 (IO6)  | grip side, lower |
| MENU | 9 (IO9)  | front, below screen, 1st |
| OK   | 10 (IO10) | front, 2nd |
| BACK | 11 (IO11) | front, 3rd |

Each switch: one leg to its GPIO, the other to GND. No external resistors needed.

## Power

```
LiPo + ──[slide switch]── Feather BAT connector +
LiPo − ─────────────────── Feather BAT connector −
USB-C on the Feather charges the battery (~200 mA, MCP73831) and is the USB mass-storage port.
```

The battery charge comes from the on-board monitor over I2C: a **MAX17048** on newer boards,
an **LC709203F** on older ones. The firmware finds either by itself.
There is no USB-present pin on this board.

## Diagram

```
                 ┌──────── Adafruit ESP32-S3 Feather ────────┐
 Display VCC ────┤3V                                      BAT├── (JST via slide switch)
 Display GND ────┤GND                                      EN│
                 │                                        USB│
     SD CLK  ────┤A0  (18)                          (13)   13├──── Display PWR
     SD CMD  ────┤A1  (17)                          (12)   12├──── Display BUSY
     SD DAT0 ────┤A2  (16)                          (11)   11├──┤├── BACK
     SD DAT1 ────┤A3  (15)                          (10)   10├──┤├── OK
     SD DAT2 ────┤A4  (14)                           (9)    9├──┤├── MENU
     SD DAT3 ────┤A5   (8)                           (6)    6├──┤├── NEXT
 Display CLK ────┤SCK (36)                           (5)    5├──┤├── PREV
 Display DIN ────┤MO  (35)                     battery    SCL│
 Display RST ────┤MI  (37)                     monitor    SDA│
 Display CS  ────┤RX  (38)                                   │
 Display DC  ────┤TX  (39)                                   │
                 └───────────────────────────────────────────┘
```
