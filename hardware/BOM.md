# Bill of materials

Prices are rough (EUR, late 2026) and only for orientation.

| # | Part | Qty | Notes | ~€ |
|---|------|-----|-------|----|
| 1 | Waveshare 4.2" e-Paper Module V2 (Rev2.2) | 1 | Already owned. 400×300, SSD1683, B/W + 4 gray, SPI | – |
| 2 | **Unexpected Maker FeatherS3[D]** (FS3D-01) | 1 | 16 MB flash, 8 MB QSPI PSRAM, LiPo charger, MAX17048 fuel gauge, 5 V sense, native USB. E.g. Tinytronics (NL), ~21 €. Take the [D], the older FeatherS3 is end of life. | 21 |
| 3 | Adafruit microSD breakout, 3 V only (product 4682) | 1 | Supports SDIO 4-bit (needed for fast USB mass storage) | 4 |
| 4 | microSD card, 4–32 GB, FAT32 | 1 | exFAT also works if FatFs exFAT is enabled | 5 |
| 5 | LiPo, flat, 1500–2000 mAh, ≤ 6 × 50 × 60 mm, JST-PH 2.0 | 1 | **Check polarity** against the board's battery connector before plugging in. Must have a protection circuit. | 10 |
| 6 | Tactile switch 6×6 mm, 5 mm tall (THT) | 5 | PREV, NEXT, MENU, OK, BACK | 1 |
| 7 | Perfboard 2.54 mm, ~70 × 15 mm | 1 | Button strip; plus a small piece for the side buttons | 1 |
| 8 | Slide switch SS12D00 (SPDT, 2.54 mm) | 1 | In series with battery + → hard off | 0.5 |
| 9 | M2 heat-set inserts, 4 mm long, OD 3.2 mm | 4 | Bezel corner bosses | 2 |
| 10 | M2×8 pan-head screws (back shell → bezel) + 3 × M2×4 self-tapping (board → chassis) | 7 | | 1 |
| 11 | Silicone wire 28–30 AWG, several colours | – | | 3 |
| 12 | Foam tape 1 mm (single-sided) | – | Cushions the display against the bezel | 2 |
| 13 | Kapton tape | – | Insulate the battery from the board | – |
| 14 | Double-sided tape, thin | – | Holds the microSD breakout on its posts | – |
| 15 | Filament: PETG or PLA, ~60 g | – | Black/dark grey looks best around e-paper | 2 |

**Total:** about €51 plus the display you already have.

## Tools
Soldering iron, flush cutters, calipers (needed — measure the display module before printing), heat-set insert tip (or the soldering iron at ~220 °C), 3D printer.

## Alternatives
- **Board:** The Adafruit ESP32-S3 Feather 4 MB / 2 MB PSRAM (5477) also fits the case; it was used until the FeatherS3[D] (see git history for its pin map). With 4 MB flash there is no room for Wi-Fi OTA, and 2 MB PSRAM limits picture sizes. The Espressif ESP32-S3-DevKitC-1 does not fit: no LiPo charger, and on the N16R8 module the octal PSRAM takes IO35–37. Seeed XIAO ESP32-S3 has too few pins for SDIO + 5 buttons + display.
- **SD in SPI mode:** possible with any microSD breakout and frees two pins (e.g. for card detect), but USB mass storage drops to ~0.5 MB/s, and the firmware only does SDIO so far.
