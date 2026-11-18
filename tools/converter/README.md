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

Tests: `uv run pytest`
