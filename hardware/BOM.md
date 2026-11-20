# Bill of materials

Prices are rough (EUR, late 2026) and only for orientation. Reference designators
refer to the [carrier board](kicad/carrier/README.md).

## Main parts
| # | Part | Qty | Notes | ~€ |
|---|------|-----|-------|----|
| 1 | Waveshare 4.2" e-Paper Module V2 (Rev2.2) | 1 | Already owned. 400×300, SSD1683, B/W + 4 gray, SPI | – |
| 2 | Unexpected Maker **FeatherS3** (U1) | 1 | ESP32-S3, 16 MB flash, 8 MB QSPI PSRAM, LiPo charger, VBAT sense (IO2), VBUS sense (IO34), native USB | 25 |
| 3 | Carrier PCB | 1 | Order 5 from JLCPCB with `kicad/carrier/fab/carrier-gerbers.zip` | 2 (+ shipping) |
| 4 | microSD card, 4–32 GB, FAT32 | 1 | | 5 |
| 5 | LiPo, flat, 1500–2000 mAh, ≤ 6 × 50 × 60 mm, JST-PH 2.0 | 1 | Lies crosswise above the Feather. **Check polarity** against J2's silkscreen. Must have a protection circuit. | 10 |

## Parts on the carrier board
| Ref | Part | Qty | ~€ |
|---|---|---|---|
| J1 | microSD socket, Molex 104031-0811 | 1 | 1.5 |
| R1–R5 | 10 kΩ, 0805 | 5 | 0.1 |
| C1 | 10 µF 10 V X5R, 0805 | 1 | 0.1 |
| C2 | 100 nF X7R, 0805 | 1 | 0.1 |
| SW1–SW3 | Tactile switch 6×6 mm, **4.3 mm** tall, THT (e.g. PHAP3301) | 3 | 0.5 |
| SW4, SW5 | Right-angle tactile switch C&K PTS645VL31-2 LFS | 2 | 1 |
| SW6 | Right-angle slide switch C&K OS102011MA1QN1 | 1 | 0.7 |
| J2 | JST PH 2-pin, horizontal (S2B-PH-K-S) | 1 | 0.2 |
| J3 | 1×9 pin header 2.54 mm, right angle (or solder the display cable straight in) | 1 | 0.2 |
| – | Pin headers 2.54 mm, 16 + 12 pins (FeatherS3 → carrier) | 1 set | 0.5 |

## Case hardware
| Part | Qty | Notes | ~€ |
|---|---|---|---|
| M2 heat-set inserts, 3 mm long, OD 3.2 mm | 4 | Front frame screw bosses | 2 |
| M2×12 pan-head screws | 4 | Back shell → through the carrier board → inserts | 1 |
| Foam pads, 3 mm, single-sided | – | Between the display's back and the carrier board | 2 |
| Kapton tape | – | Over trimmed leads on the display side, under the battery | – |
| Filament: PETG or PLA, ~55 g | – | Black/dark grey looks best around e-paper | 2 |

**Total:** about €55–60 plus the display you already have.

## Tools
Soldering iron with a fine tip (the microSD socket has 1.1 mm pitch), flux, flush cutters,
calipers (measure the display module before printing), heat-set insert tip (or the soldering
iron at ~220 °C), 3D printer.

## Without the carrier board
For a breadboard test or a hand-wired build, use an Adafruit microSD breakout (product 4682,
SDIO-capable) and wire everything as in [wiring.md](wiring.md). The pin assignment is the same.
