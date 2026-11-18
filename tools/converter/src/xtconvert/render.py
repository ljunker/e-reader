"""Turn source pages into screen-sized 4-gray images."""
from __future__ import annotations

import io
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator

from PIL import Image, ImageOps

# Screen in reading (portrait) orientation.
SCREEN_W, SCREEN_H = 300, 400
GRAY_LEVELS = (0, 85, 170, 255)   # index 0..3 → value written to the .xtp

IMAGE_EXT = {".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp", ".tif", ".tiff"}


@dataclass
class Options:
    mode: str = "fit"          # fit | width | landscape
    crop: bool = True          # trim white margins
    gamma: float = 1.0         # >1 darkens mid-tones (thin PDF text looks better)
    dither: bool = True
    dpi: int = 200             # PDF render resolution before scaling down
    overlap: int = 20          # px of overlap between slices in "width" mode


def screen_size(mode: str) -> tuple[int, int]:
    return (SCREEN_H, SCREEN_W) if mode == "landscape" else (SCREEN_W, SCREEN_H)


# ---------- sources ----------

def iter_pdf(path: Path, dpi: int) -> Iterator[Image.Image]:
    import pymupdf

    with pymupdf.open(path) as doc:
        for page in doc:
            pix = page.get_pixmap(dpi=dpi, colorspace=pymupdf.csGRAY, alpha=False)
            yield Image.frombytes("L", (pix.width, pix.height), pix.samples)


def iter_zip(path: Path) -> Iterator[Image.Image]:
    with zipfile.ZipFile(path) as z:
        names = sorted(n for n in z.namelist() if Path(n).suffix.lower() in IMAGE_EXT and not n.startswith("__MACOSX"))
        for n in names:
            with z.open(n) as f:
                yield Image.open(io.BytesIO(f.read())).convert("L")


def iter_images(paths: list[Path]) -> Iterator[Image.Image]:
    for p in paths:
        yield Image.open(p).convert("L")


def iter_source(path: Path, opts: Options) -> Iterator[Image.Image]:
    if path.is_dir():
        yield from iter_images(sorted(p for p in path.iterdir() if p.suffix.lower() in IMAGE_EXT))
        return
    ext = path.suffix.lower()
    if ext == ".pdf":
        yield from iter_pdf(path, opts.dpi)
    elif ext in (".cbz", ".zip"):
        yield from iter_zip(path)
    elif ext in IMAGE_EXT:
        yield from iter_images([path])
    elif ext in (".epub", ".xps", ".cbr", ".fb2", ".mobi"):
        # PyMuPDF opens these too (EPUBs get laid out as fixed pages)
        yield from iter_pdf(path, opts.dpi)
    else:
        raise ValueError(f"unsupported input: {path.name}")


# ---------- processing ----------

def autocrop(img: Image.Image, threshold: int = 235, pad: int = 8) -> Image.Image:
    """Trim near-white margins."""
    mask = img.point(lambda v: 255 if v < threshold else 0)
    bbox = mask.getbbox()
    if not bbox:
        return img
    l, t, r, b = bbox
    return img.crop((max(l - pad, 0), max(t - pad, 0), min(r + pad, img.width), min(b + pad, img.height)))


def apply_gamma(img: Image.Image, gamma: float) -> Image.Image:
    if gamma == 1.0:
        return img
    lut = [round(255 * (v / 255) ** gamma) for v in range(256)]
    return img.point(lut)


def fit_into(img: Image.Image, w: int, h: int) -> Image.Image:
    """Scale to fit inside w×h keeping the aspect ratio, centred on white."""
    scaled = ImageOps.contain(img, (w, h), Image.Resampling.LANCZOS)
    canvas = Image.new("L", (w, h), 255)
    canvas.paste(scaled, ((w - scaled.width) // 2, (h - scaled.height) // 2))
    return canvas


def slice_by_width(img: Image.Image, w: int, h: int, overlap: int) -> list[Image.Image]:
    """Scale to the screen width and cut into screen-high slices."""
    scale = w / img.width
    tall = img.resize((w, max(1, round(img.height * scale))), Image.Resampling.LANCZOS)
    if tall.height <= h:
        return [fit_into(tall, w, h)]
    slices = []
    step = h - overlap
    y = 0
    while True:
        if y + h >= tall.height:
            y = tall.height - h       # last slice aligned to the bottom
            slices.append(tall.crop((0, y, w, y + h)))
            break
        slices.append(tall.crop((0, y, w, y + h)))
        y += step
    return slices


def quantize(img: Image.Image, dither: bool) -> bytes:
    """Return one byte per pixel with values 0..3."""
    pal = Image.new("P", (1, 1))
    pal.putpalette(sum(([g, g, g] for g in GRAY_LEVELS), []) + [0, 0, 0] * 252)
    q = img.convert("RGB").quantize(palette=pal, dither=Image.Dither.FLOYDSTEINBERG if dither else Image.Dither.NONE)
    return q.tobytes()


def process(img: Image.Image, opts: Options) -> list[Image.Image]:
    """One source page → one or more screen-sized grayscale images."""
    if opts.crop:
        img = autocrop(img)
    img = apply_gamma(img, opts.gamma)
    w, h = screen_size(opts.mode)
    if opts.mode == "width":
        return slice_by_width(img, w, h, opts.overlap)
    if opts.mode == "landscape":
        # top and bottom half of the page, each fitted to the 400×300 screen
        if img.height > img.width:
            half = img.height // 2
            ov = min(opts.overlap * 2, half // 4)
            parts = [img.crop((0, 0, img.width, half + ov)), img.crop((0, half - ov, img.width, img.height))]
            return [fit_into(p, w, h) for p in parts]
        return [fit_into(img, w, h)]
    return [fit_into(img, w, h)]


def to_preview(pixels: bytes, w: int, h: int) -> Image.Image:
    return Image.frombytes("L", (w, h), bytes(GRAY_LEVELS[v] for v in pixels))


def quantized_image(img: Image.Image, opts: Options) -> tuple[bytes, Image.Image]:
    px = quantize(img, opts.dither)
    return px, to_preview(px, img.width, img.height)


