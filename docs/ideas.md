# Feature ideas

Firmware budget (measured 2026-11-27, `pio run -e feathers3d`): the app uses **1.78 MB of a 4 MB slot**, so about
**2.4 MB are free**. The FeatherS3[D] has 16 MB flash, laid out in `firmware/partitions-16MB.csv` as two 4 MB app
slots (the second one is for Wi-Fi OTA), TinyUF2 and a 7.7 MB FAT partition that nothing uses yet. **8 MB PSRAM**
(was 2 MB) holds fonts, hyphenation patterns, pictures and file data with room to spare.

Rule of thumb: **data the user changes goes on the microSD card, code goes into flash.** Fixed data (a default
font, hyphenation patterns, a dictionary) may now also live in the FAT partition, so the reader works without
those files on the card.

## Nearly free (< 5 KB flash)

| Feature | What it does | Flash | Data | Status |
|---|---|---|---|---|
| Cover as sleep screen | The panel shows the open book's cover while the reader sleeps (MENU → Sleep screen) | together ~12 KB code, +18.6 KB flash | `/.reader/<hash>.cov` | done |
| Cover thumbnails in the library | Small dithered cover next to each book, made once when it first scrolls into view | ↑ | `/.reader/<hash>.cov` | done |
| Better status line | Chapter title, "4 left · 37 / 280", battery symbol, progress bar with chapter ticks | ↑ | – | done |
| Sort the library | Recently read first or by title (MENU → Sort), percent read and "finished" per book | ↑ | NVS | done |
| USB power indicator | `+` after the battery percentage while USB-C supplies 5 V (VBUS sense on IO34) | < 0.1 KB | – | done |
| Card detect | DET wired to IO3: "No microSD card" instead of a mount error | – | – | done (wiring); rescan on insert is open |
| Pictures from `/sleep/` | Sleep screen from your own pictures instead of the cover | ~1 KB | `/sleep/` on SD | |
| Bookmarks | Long press OK sets one, list in the menu | ~3 KB | SD, per book | |
| Reading time left | Average pages per minute → "~25 min left in chapter" | ~1 KB | NVS | |
| Refresh settings | Full refresh every N pages, inverted night mode | < 1 KB | NVS | |
| Landscape for `.xtp` | Comics and PDFs sideways, page split if needed | ~1 KB | – | |
| Rescan on card insert | DET edge → remount and refresh the library; card pulled while reading → message instead of read errors | ~1 KB | – | needs DET (wired now) |
| Charging screen | While USB power is on and the reader would sleep: show "Charging 64 %" and wake every few minutes to update it | ~2 KB | – | VBUS sense + fuel gauge |
| Sleep sooner in the dark | Ambient light sensor (IO4) reads dark → reader is in a bag or sleeve, sleep after 30 s instead of 3 min | < 1 KB | – | calibrate the threshold on the device |
| Low-battery warning | MAX17048 alert or SOC < 5 %: show "Charge me" and sleep without wake-up on page turn | ~1 KB | – | gauge keeps counting in deep sleep (LDO1) |

## Medium (5–30 KB flash)

| Feature | What it does | Flash | Data | Notes |
|---|---|---|---|---|
| Hyphenation (Liang) | Better justified text at 300 px width | +12.7 KB (measured) | `/hyph/de.hyp` 219 KB, `en.hyp` 29 KB, in PSRAM while a book is open | **done**; language from `dc:language`, guessed for TXT |
| **Wi-Fi OTA updates** | Upload `firmware.bin` on the existing upload page; it goes into the other app slot, is checked and booted, the old one stays as fallback | ~10–20 KB (`Update`) | – | **new with 16 MB flash**: two 4 MB slots are already in the partition table |
| Footnote popups | EPUB note references open an overlay | ~5–8 KB | – | XHTML parser exists |
| Reading statistics | Minutes per day, pages per week, streak; chart on the upload page | ~5 KB | log on SD | time from NTP when Wi-Fi is on |
| Highlights + export | Mark sentence by sentence, download as Markdown over Wi-Fi | ~6–10 KB | SD | sentences are easier than words with 5 buttons |
| Dictionary | Look up a word; sorted index on SD or in the FAT partition, binary search | ~8 KB | 10–50 MB on SD, or up to ~7 MB in flash | built by `xtconvert`; word selection UX is the hard part. With 8 MB PSRAM the index can stay in RAM |
| Next page in advance | Lay out and draw the next page into a PSRAM frame while the reader reads, so NEXT only refreshes the panel | +1.8 KB (measured) | 3 frames × 15 KB in PSRAM (+30 KB each for `.xtp` gray pages) | **done**; the previous page is kept for PREV, the status line is drawn fresh on each turn |
| Built-in fonts and patterns in flash | Copy the default `.xff` fonts and `.hyp` files into the FAT partition, so a fresh card works without `/fonts` and `/hyph` | ~5 KB (FFat) | ~0.5–2 MB in flash | update them over Wi-Fi like books |
| Larger pictures | EPUB pictures up to 4 MP decoded (guards raised for 8 MB PSRAM) | – | – | **done**; PNGs up to ~1300 × 1300 in colour |

## Expensive, weigh carefully

| Feature | What it does | Flash | Notes |
|---|---|---|---|
| Fonts from SD + gray anti-aliasing | Fonts as files loaded into PSRAM, own TTFs via `xtconvert` | **saved 84 KB** (measured: U8g2 out, 20 KB built-in Inter in) | fonts from SD **done** (`/fonts/*.xff`, 2-bit glyphs, text shown black and white). A 4-gray refresh takes **5.5 s** on this panel, so only `.xtp` pages and the sleep cover use it (**done**, +2.8 KB). Gray text without the wait would need a custom LUT that darkens gray pixels after a fast b/w refresh (CrossPoint's trick); try after hardware bring-up |
| OPDS / Calibre content server | Browse and download books on the device | ~15–25 KB (HTTP), +100 KB or more with HTTPS (mbedTLS) | **affordable now**: HTTPS fits easily in the 2.4 MB left |
| KOReader progress sync | Share reading position with other devices | ~10 KB (HTTP), + HTTPS as above | same server code as OPDS |
| Bluetooth page-turn remote | Pair a cheap BLE HID clicker or a phone as remote; turns pages without touching the reader | ~150–250 KB (NimBLE) | **new**: did not fit next to Wi-Fi before. BLE costs battery while on, so only while a book is open and switchable in the menu |
| Wallabag / Instapaper / Pocket-style reading list | Fetch saved web articles as EPUB over HTTPS | ~30–50 KB + HTTPS | alternative to doing it on the computer with `xtconvert <url>` |

## No firmware cost (computer side)

| Feature | What it does |
|---|---|
| Web articles → EPUB | `xtconvert <url>` with Readability extraction |
| RSS digest | Daily EPUB from feeds, sent over Wi-Fi |

## Freeing flash

Not urgent any more with 2.4 MB free.

| Change | Saves |
|---|---|
| `CORE_DEBUG_LEVEL=0` in release builds (less printf/log code) | some of ~30 KB |
