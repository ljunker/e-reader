# xtconvert

Converts PDFs, comics (CBZ), image folders — anything with fixed pages — into
`.xtp` files the e-reader shows page by page. EPUBs don't need this: the reader
lays them out itself.

```bash
cd tools/converter
uv run xtconvert book.pdf                       # fit whole pages, writes book.xtp next to it
uv run xtconvert paper.pdf --mode width         # zoom to text width, 2–3 screens per A4 page
uv run xtconvert slides.pdf --mode landscape    # rotated screen, half a page per screen
uv run xtconvert comic.cbz --no-crop --preview out/
```

Options: `--gamma 1.3` darkens thin text, `--no-dither` gives crisper text but worse
photos, `--preview DIR` writes PNGs of every screen so you can check the result
without the device.

Copy the `.xtp` file to the SD card (or upload it over Wi‑Fi / USB).
The format is documented in [`src/xtconvert/xtp.py`](src/xtconvert/xtp.py).

## Hyphenation patterns

`xtconvert hyph` turns TeX hyphenation patterns (hyph-utf8 `.tex` files, kept in
`patterns/`) into the `.hyp` files the reader loads from `/hyph/` on the card:

```bash
uv run xtconvert hyph patterns/hyph-de-1996.tex -o ../../sdcard/hyph/de.hyp
uv run xtconvert hyph patterns/hyph-en-us.tex -o ../../sdcard/hyph/en.hyp
```

Another language: add its `.tex` file and name the output after its two-letter code;
the firmware maps `dc:language` to `de` and `en` today. The format is documented in
[`src/xtconvert/hyph.py`](src/xtconvert/hyph.py).

## Fonts

`xtconvert font` renders a TrueType/OpenType family into an anti-aliased `.xff` bitmap font
(4 gray levels, all four styles in several pixel sizes) for `/fonts/` on the card. A variable font
gives bold through its weight axis; separate files go in with `--bold`, `--italic` and `--bold-italic`.

```bash
uv run xtconvert font "fonts/literata/Literata[opsz,wght].ttf" \
    --italic "fonts/literata/Literata-Italic[opsz,wght].ttf" -o ../../sdcard/fonts/literata.xff
uv run xtconvert font ~/Downloads/MyFont-Regular.ttf --bold ~/Downloads/MyFont-Bold.ttf \
    --italic ~/Downloads/MyFont-Italic.ttf --bold-italic ~/Downloads/MyFont-BoldItalic.ttf \
    -o /Volumes/SD/fonts/myfont.xff --preview out/
```

Options: `--sizes 15,17,20,24,29` (pixel sizes; the reader's three reading sizes use the first
three, headings the larger ones), `--gamma 1.3` (higher darkens thin strokes), `--preview DIR`
(sample line of every face, in gray and as the black-and-white panel shows it).

The firmware's built-in fallback font (`firmware/src/BuiltinFont.h`, used for menus and when
the card has no fonts) is built the same way:

```bash
uv run xtconvert font "fonts/inter/Inter[opsz,wght].ttf" --faces regular:11,regular:14,bold:16 \
    --name Inter --symbol kBuiltinFont -o ../../firmware/src/BuiltinFont.h
```

`fonts/` holds Literata and Inter (SIL Open Font License, see the `OFL.txt` next to each).
The format is documented in [`src/xtconvert/font.py`](src/xtconvert/font.py).

Tests: `uv run pytest`
