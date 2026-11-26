# DIY e-reader

A pocket e-reader built around a **Waveshare 4.2" e-Paper Module V2** (400 × 300, black/white + 4 grays),
an **ESP32-S3** (Adafruit ESP32-S3 Feather, 4 MB flash / 2 MB PSRAM), a microSD card and a 3D-printed case.
It reads books in portrait at 300 × 400, sleeps between page turns and runs for weeks on a 1500–2000 mAh LiPo.

![Assembly](cad/renders/assembly.png)

| | |
|---|---|
| **Formats** | `.txt`; **EPUB** laid out on the device (bold/italic, headings, pictures, table of contents); **PDF / comics** pre-rendered on your computer with [`xtconvert`](tools/converter) into `.xtp` |
| **Controls** | PREV / NEXT on the grip edge (left by default, `left_handed` in `cad/params.scad`), MENU / OK / BACK below the screen; long press jumps 10 pages |
| **Getting books on** | microSD card, Wi‑Fi upload page in the browser, or USB drive over USB‑C |
| **Case** | ~95 × 137 × 17 mm, parametric OpenSCAD, fits a Bambu A1 mini |

## Repository
| Folder | What's inside |
|---|---|
| [`hardware/`](hardware) | [Bill of materials](hardware/BOM.md), [wiring](hardware/wiring.md) |
| [`cad/`](cad) | OpenSCAD case: `params.scad` holds every dimension; `make` exports STLs and runs a collision check |
| [`firmware/`](firmware) | PlatformIO project (Arduino on ESP32-S3) |
| [`tools/converter/`](tools/converter) | `xtconvert`: PDF / CBZ / images → `.xtp` |
| [`docs/`](docs) | [Assembly](docs/assembly.md), [printing](docs/printing.md) |

## Quick start
1. Buy the parts in the [BOM](hardware/BOM.md) and wire them per [wiring.md](hardware/wiring.md) (breadboard first).
2. Flash the firmware:
   ```bash
   cd firmware && pio run -t upload
   ```
3. Format a microSD card as FAT32, make a `books/` folder and copy `.epub` and `.txt` files and converted PDFs into it:
   ```bash
   cd tools/converter && uv run xtconvert ~/Downloads/paper.pdf --mode width -o /Volumes/SD/books/
   ```
4. Measure your display module, set the values in `cad/params.scad`, print the fit test, then the case ([printing.md](docs/printing.md)).
5. Assemble ([assembly.md](docs/assembly.md)).

## Development
```bash
cd firmware && pio test -e native          # panel model, layout, EPUB, zip, image and .xtp unit tests on your computer
cd tools/converter && uv run pytest        # converter tests
make -C cad                                # STLs + collision check (needs OpenSCAD 2024+)
make -C cad png                            # preview renders
make -C cad steps                          # step-by-step pictures for docs/assembly.md
```

## Simulator
Try the firmware in the browser, without hardware:
```bash
brew install emscripten                 # once (or https://emscripten.org)
cd firmware && pio run -e sim -t serve  # then open http://localhost:8000/
```
`env:sim` compiles everything in `src/` unchanged, with the real GxEPD2, Adafruit GFX and U8g2 libraries
and String/FS from the arduino-esp32 core, to WebAssembly. Only the hardware underneath is simulated (`firmware/sim/`):
- **Display:** an SSD1683 model (`lib/epdsim`) receives GxEPD2's SPI commands. It holds BUSY for the real refresh time
  and shows full refreshes flashing, like the panel does. "Instant refresh" skips the wait.
- **Buttons:** click the caps or use the keys ← PREV, → NEXT, M MENU, Enter OK, Esc BACK. Holding a button gives a
  long press.
- **microSD and NVS:** kept in the browser's IndexedDB. On first start the card gets the test EPUBs, a short TXT and a
  `wifi.txt`. The side panel lists, adds, downloads and deletes files.
- **Deep sleep:** "Skip 3 min" runs out the idle timer. The panel keeps its picture, and a button reboots the
  firmware with its RTC memory, as on the chip. "Reset" is a cold start.
- **USB-C:** set the cable to Unplugged, Computer or Charger. While the drive is on, the file panel is the computer's
  view, and "Eject" ejects it.
- **Wi‑Fi:** the firmware's own upload page appears in the side panel. A service worker passes its requests to the
  firmware's web server.
- **Battery:** a simulated MAX17048 on I2C, set with the slider.

Not simulated: radio, real USB/sector access, RAM/PSRAM limits and the speed of the ESP32-S3 (EPUB conversion is
much faster in the browser).

## EPUB on the device
The first time a book is opened, the reader converts it once into cache files in `/.reader/` on the card
(this takes a few seconds to about a minute with many pictures; the panel shows the progress).
After that, opening, turning pages and changing the font size are as fast as with `.txt` files.
MENU → **Contents** jumps to a chapter.

**Hyphenation:** copy the [`sdcard/hyph/`](sdcard/hyph) folder to the top of the card (`/hyph/de.hyp`, `/hyph/en.hyp`).
Long words are then broken at line ends, so justified lines get even gaps. The language comes from the EPUB's
`dc:language`; for TXT files and EPUBs without it the reader guesses German or English from the text. Other
languages and a card without `/hyph/` still break after hyphens already in a word ("E-Mail-Adresse").
MENU → **Hyphenation** switches it off. Built with `xtconvert hyph` from the hyph-utf8 patterns in
`tools/converter/patterns/`. Updating to this firmware converts each EPUB once more on first open.

Supported: EPUB 2 and 3, chapters in reading order, headings, bold and italic, alignment, blockquotes,
lists, line breaks, page breaks from CSS, JPEG (baseline) and PNG pictures dithered to the panel,
and the table of contents from the nav document or the NCX.
Not supported: DRM-protected books (the reader says so), GIF/SVG and progressive JPEG pictures (left out),
characters outside Latin-1 (shown as the nearest Latin letter or `?` until the TTF fonts arrive), footnote pop-ups.

## Wi‑Fi upload & USB drive
**Wi‑Fi:** put a `wifi.txt` in the top folder of the card, one network per line:
```
# SSID = password   (lines starting with # are ignored)
Zuhause = geheim123
Büro WLAN = pass=word
Gartenhaus =
```
The line is split at the first `=`, so the password may contain `=` or `#`; leave it empty for an open network.
MENU → **Wi‑Fi upload** joins the strongest of these networks and shows the address.
Open `http://ereader.local/` (or the IP shown) in a browser on the same network to upload, replace or delete books;
the screen counts the files that arrive. BACK switches Wi‑Fi off, and so do 10 minutes without a request.

**USB:** plug the reader into a computer with the USB‑C cable. It asks whether to show the card as a USB drive
(OK) or to keep reading (BACK); MENU → **USB drive** offers it again. Copy books into `books/`, eject the drive,
and the reader returns to the library. A charger alone never triggers the question.

The firmware needs one large app partition for Wi‑Fi (`tinyuf2-partitions-4MB-noota.csv`); the first
`pio run -t upload` after updating rewrites the partition table. Saved reading positions stay.

## Status / roadmap
- [x] Hardware design, wiring, BOM
- [x] Parametric case with collision check
- [x] PDF/comic converter and `.xtp` format
- [x] Firmware: display, buttons, SD, deep sleep + wake on any button, library, TXT reader with justified text and cached pagination, `.xtp` viewer, font sizes, saved positions
- [ ] Bring-up on real hardware (pin check, Rev2.2 `PWR` pin, rotation, battery monitor)
- [ ] Anti-aliased fonts generated from TTFs
- [x] Hyphenation (Liang patterns, German and English)
- [x] EPUB (unzip, OPF/spine, XHTML + CSS subset, images, table of contents)
- [ ] True 4-gray refresh for PDF pages (currently 2×2 dithered B/W)
- [x] Wi‑Fi upload page, USB mass storage
- [ ] Optional KiCad carrier PCB (draft in tag `carrier-pcb-draft`)
