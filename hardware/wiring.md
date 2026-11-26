# Wiring

All pin numbers live in [`firmware/include/pins.h`](../firmware/include/pins.h) — change them there, not in code.
The board is the **Unexpected Maker FeatherS3[D]** (16 MB flash, 8 MB PSRAM).
"Board pin" below is the label printed on the board, the GPIO number is in brackets.

> ⚠️ Check every pin against the [FeatherS3[D] pinout card](https://feathers3.io) before soldering.
> On the board itself: SDA (IO8) / SCL (IO9) go to the MAX17048 fuel gauge, IO34 senses 5 V on USB-C,
> IO39 switches LDO2 (RGB LED, second STEMMA QT), IO13 drives the blue LED, IO41 the antenna switch,
> IO4 reads the ambient light sensor. None of these are on the header except SDA/SCL.

The board breaks out 21 GPIOs; SDA and SCL are shared with the fuel gauge, so 19 are left for the
19 signals below. Buttons and card detect sit on RTC GPIOs (IO0–IO21), so any button wakes the reader
from deep sleep. The octal-PSRAM pins of other S3 modules (IO33–IO37) are free on this board.

## Display (Waveshare 4.2" V2, 9-pin header)

| Display pin | Board pin | Notes |
|---|---|---|
| VCC | 3V3 | Module accepts 3.3 V or 5 V; use 3.3 V |
| GND | GND | |
| DIN | MO (IO35) | SPI2 |
| CLK | SCK (IO36) | SPI2 |
| CS  | RX (IO44) | Serial goes over native USB, the UART pins are free |
| DC  | TX (IO43) | The ROM boot log toggles it while CS is high; harmless |
| RST | MI (IO37) | MISO is not needed, the display is write-only |
| BUSY | 38 (IO38) | input, high = busy on SSD1683 |
| PWR | 33 (IO33) | Rev2.2 only. High = module powered. Driven low and held before deep sleep. If your header has no PWR pin, leave 33 unconnected. |

## microSD (Adafruit 4682, SDIO 4-bit)

| Breakout | Board pin |
|---|---|
| 3V | 3V3 |
| GND | GND |
| CLK | 17 (IO17) |
| SI / CMD | 18 (IO18) |
| SO / DAT0 | 14 (IO14) |
| DAT1 | 12 (IO12) |
| DAT2 | 6 (IO6) |
| CS / DAT3 | 5 (IO5) |
| DET (card detect) | 3 (IO3) — high with a card, so the reader can tell "no card" from a broken one |

The six SDIO pins are next to each other on the 16-pin header (17 … 5).

## Buttons (active low, internal pull-ups, all RTC GPIOs → can wake from deep sleep)

| Button | Board pin | Position on case |
|---|---|---|
| PREV | 11 (IO11) | grip side (left, or right if `left_handed = false`), upper |
| NEXT | 10 (IO10) | grip side, lower |
| MENU | 7 (IO7) | front, below screen, 1st |
| OK   | 1 (IO1) | front, 2nd |
| BACK | 0 (IO0) | front, 3rd |

Each switch: one leg to its GPIO, the other to GND. No external resistors needed.
IO0 is also the board's BOOT button: holding **BACK while pressing RESET** starts the ROM bootloader,
so the firmware can be flashed with the case closed even if the app does not start.

## Power

```
LiPo + ──[slide switch]── board BAT connector +
LiPo − ─────────────────── board BAT connector −
USB-C on the board charges the battery and is the USB mass-storage port.
```

The charge comes from the on-board **MAX17048** fuel gauge over I2C. It sits on LDO1, which stays on in
deep sleep, so it keeps counting while the reader sleeps. IO34 is high while USB-C supplies 5 V
(computer or charger); the reader shows a `+` after the battery percentage then.

## Diagram

```
                  ┌────── FeatherS3[D] (USB-C at the top) ──────┐
                  ┤RST                                          │
  Display VCC ────┤3V3                                          │
         BACK ─┤├─┤0                                            │
  Display GND ────┤GND                                       BAT├── (JST via slide switch)
       SD CLK ────┤17                                         EN│
       SD CMD ────┤18                                         5V│
      SD DAT0 ────┤14                                         11├──┤├── PREV
      SD DAT1 ────┤12                                         10├──┤├── NEXT
      SD DAT2 ────┤6                                           7├──┤├── MENU
      SD DAT3 ────┤5                                           3├──── SD DET
  Display CLK ────┤SCK (36)                                    1├──┤├── OK
  Display DIN ────┤MO  (35)                                   38├──── Display BUSY
  Display RST ────┤MI  (37)                                   33├──── Display PWR
  Display CS  ────┤RX  (44)                     fuel gauge   SCL│
  Display DC  ────┤TX  (43)                     fuel gauge   SDA│
                  ┤3V3.2 (LDO2, unused)                         │
                  └─────────────────────────────────────────────┘
```
