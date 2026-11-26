# Feature ideas

Firmware budget (measured 2026-11-26, `pio run -e feather_s3`): the app uses 1.83 MB of the 2.88 MB slot,
so about **1 MB is free**. The 22 U8g2 fonts take 118 KB. Wi-Fi OTA would need two app slots of at most
1.4 MB each, so updates stay with UF2 over USB.

Rule of thumb: **data goes on the microSD card, only code goes into flash.**

## Nearly free (< 5 KB flash)

| Feature | What it does | Flash | Data | Status |
|---|---|---|---|---|
| Cover as sleep screen | The panel shows the open book's cover while the reader sleeps (MENU → Sleep screen) | together ~12 KB code, +18.6 KB flash | `/.reader/<hash>.cov` | done |
| Cover thumbnails in the library | Small dithered cover next to each book, made once when it first scrolls into view | ↑ | `/.reader/<hash>.cov` | done |
| Better status line | Chapter title, "4 left · 37 / 280", battery symbol, progress bar with chapter ticks | ↑ | – | done |
| Sort the library | Recently read first or by title (MENU → Sort), percent read and "finished" per book | ↑ | NVS | done |
| Pictures from `/sleep/` | Sleep screen from your own pictures instead of the cover | ~1 KB | `/sleep/` on SD | |
| Bookmarks | Long press OK sets one, list in the menu | ~3 KB | SD, per book | |
| Reading time left | Average pages per minute → "~25 min left in chapter" | ~1 KB | NVS | |
| Refresh settings | Full refresh every N pages, inverted night mode | < 1 KB | NVS | |
| Landscape for `.xtp` | Comics and PDFs sideways, page split if needed | ~1 KB | – | |

## Medium (5–30 KB flash)

| Feature | What it does | Flash | Data | Notes |
|---|---|---|---|---|
| Hyphenation (Liang) | Better justified text at 300 px width | +12.7 KB (measured) | `/hyph/de.hyp` 219 KB, `en.hyp` 29 KB, in PSRAM while a book is open | **done**; language from `dc:language`, guessed for TXT |
| Footnote popups | EPUB note references open an overlay | ~5–8 KB | – | XHTML parser exists |
| Reading statistics | Minutes per day, pages per week, streak; chart on the upload page | ~5 KB | log on SD | time from NTP when Wi-Fi is on |
| Highlights + export | Mark sentence by sentence, download as Markdown over Wi-Fi | ~6–10 KB | SD | sentences are easier than words with 5 buttons |
| Dictionary | Look up a word; sorted index on SD, binary search | ~8 KB | 10–50 MB on SD | built by `xtconvert`; word selection UX is the hard part |

## Expensive, weigh carefully

| Feature | What it does | Flash | Notes |
|---|---|---|---|
| Fonts from SD + gray anti-aliasing | Fonts as files loaded into PSRAM, own TTFs via `xtconvert` | **saves ~118 KB** | 4-gray mode on the SSD1683 needs a full refresh per page |
| OPDS / Calibre content server | Browse and download books on the device | ~15–25 KB (HTTP) | HTTPS pulls in mbedTLS: +100 KB or more |
| KOReader progress sync | Share reading position with other devices | ~10 KB (HTTP) | same HTTPS caveat |

## No firmware cost (computer side)

| Feature | What it does |
|---|---|
| Web articles → EPUB | `xtconvert <url>` with Readability extraction |
| RSS digest | Daily EPUB from feeds, sent over Wi-Fi |

## Freeing flash

| Change | Saves |
|---|---|
| TXT books use Lucida like EPUB, drop the `ncen` fonts | ~14 KB |
| `CORE_DEBUG_LEVEL=0` in release builds (less printf/log code) | some of ~30 KB |
