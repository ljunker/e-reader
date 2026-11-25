# Wiring

All pin numbers live in [`firmware/include/pins.h`](../firmware/include/pins.h) — change them there, not in code.

> ⚠️ Check every pin against Unexpected Maker's FeatherS3 pinout card before soldering. The header
> pins used below are the ones UM breaks out; IO2 (VBAT sense), IO34 (VBUS sense), IO39 (LDO2/RGB power),
> IO40 (RGB) and IO13 (LED) are used on the board itself.

## Display (Waveshare 4.2" V2, 9-pin header)

| Display pin | FeatherS3 | Notes |
|---|---|---|
| VCC | 3V3 | Module accepts 3.3 V or 5 V; use 3.3 V |
| GND | GND | |
| DIN | IO35 (MOSI) | SPI2 |
| CLK | IO36 (SCK) | SPI2 |
| CS  | IO38 | |
| DC  | IO33 | |
| RST | IO43 (TX) | UART0 unused, serial goes over native USB |
| BUSY | IO44 (RX) | input, high = busy on SSD1683 |
| PWR | IO18 | Rev2.2 only. High = module powered. Driven low before deep sleep. If your header has no PWR pin, leave IO18 unconnected. |

## microSD (Adafruit 4682, SDIO 4-bit)

| Breakout | FeatherS3 |
|---|---|
| 3V | 3V3 |
| GND | GND |
| CLK | IO12 |
| SI / CMD | IO11 |
| SO / DAT0 | IO10 |
| DAT1 | IO9 |
| DAT2 | IO17 |
| CS / DAT3 | IO8 |
| DET (card detect) | IO3 (input, pull-up; low = card present on most sockets — verify) |

## Buttons (active low, internal pull-ups, all RTC GPIOs → can wake from deep sleep)

| Button | FeatherS3 | Position on case |
|---|---|---|
| PREV | IO14 | right side, upper |
| NEXT | IO1  | right side, lower |
| MENU | IO5  | front, below screen, 1st |
| OK   | IO6  | front, 2nd |
| BACK | IO7  | front, 3rd |

Each switch: one leg to its GPIO, the other to GND. No external resistors needed.

## Power

```
LiPo + ──[slide switch]── FeatherS3 BAT connector +
LiPo − ─────────────────── FeatherS3 BAT connector −
USB-C on the FeatherS3 charges the battery (~500 mA) and is the USB mass-storage port.
```

Spare header pin: IO37 (MISO, free).

## Diagram

```
                 ┌───────────── FeatherS3 ─────────────┐
 Display VCC ────┤3V3                              VBAT├── (BAT JST via slide switch)
 Display GND ────┤GND                              VBUS│
 Display DIN ────┤IO35                             IO11├──── SD CMD
 Display CLK ────┤IO36                             IO10├──── SD DAT0
 Display CS  ────┤IO38                              IO9├──── SD DAT1
 Display DC  ────┤IO33                             IO17├──── SD DAT2
 Display RST ────┤IO43                              IO8├──── SD DAT3
 Display BUSY────┤IO44                             IO12├──── SD CLK
 Display PWR ────┤IO18                              IO3├──── SD DET
 PREV ──┤├─GND ──┤IO14                              IO5├──┤├── MENU
 NEXT ──┤├─GND ──┤IO1                               IO6├──┤├── OK
                 │                                  IO7├──┤├── BACK
                 └─────────────────────────────────────┘
```
