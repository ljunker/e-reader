"""Command line: xtconvert book.pdf [-o book.xtp] [--mode fit|width|landscape] ...

xtconvert hyph patterns.tex -o de.hyp builds hyphenation patterns for the SD card (see hyph.py).
xtconvert font Font.ttf -o font.xff builds a bitmap font for the SD card (see font.py)."""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from .render import Options, iter_source, process, quantize, screen_size, to_preview
from .xtp import XtpFile


def convert(src: Path, dst: Path, opts: Options, title: str | None = None, preview: Path | None = None,
            max_pages: int | None = None, quiet: bool = False) -> XtpFile:
    w, h = screen_size(opts.mode)
    out = XtpFile(w, h, title or src.stem)
    if preview:
        preview.mkdir(parents=True, exist_ok=True)
    for n, page in enumerate(iter_source(src, opts), start=1):
        if max_pages and n > max_pages:
            break
        for img in process(page, opts):
            px = quantize(img, opts.dither)
            out.add_page(px)
            if preview:
                to_preview(px, w, h).save(preview / f"{len(out.pages):04d}.png")
        if not quiet:
            print(f"\r{src.name}: page {n} → {len(out.pages)} screens", end="", file=sys.stderr)
    if not quiet:
        print(file=sys.stderr)
    if not out.pages:
        raise ValueError(f"{src.name}: no pages found")
    dst.write_bytes(out.to_bytes(rle=True))
    return out


def main(argv: list[str] | None = None) -> int:
    argv = sys.argv[1:] if argv is None else argv
    if argv[:1] == ["hyph"]:
        from . import hyph
        return hyph.main(argv[1:])
    if argv[:1] == ["font"]:
        from . import font
        return font.main(argv[1:])
    ap = argparse.ArgumentParser(prog="xtconvert", description=__doc__)
    ap.add_argument("input", type=Path, nargs="+", help="PDF, CBZ, image, or folder of images")
    ap.add_argument("-o", "--output", type=Path, help="output file (single input) or folder")
    ap.add_argument("--mode", choices=["fit", "width", "landscape"], default="fit",
                    help="fit: whole page; width: zoom to width and split into slices; landscape: half pages on the rotated screen")
    ap.add_argument("--no-crop", action="store_true", help="keep white margins")
    ap.add_argument("--gamma", type=float, default=1.0, help="darken mid-tones, e.g. 1.3 for thin text")
    ap.add_argument("--no-dither", action="store_true", help="hard 4-level quantization (sharper text, worse photos)")
    ap.add_argument("--dpi", type=int, default=200)
    ap.add_argument("--title")
    ap.add_argument("--preview", type=Path, help="also write each screen as a PNG into this folder")
    ap.add_argument("--max-pages", type=int)
    args = ap.parse_args(argv)

    opts = Options(mode=args.mode, crop=not args.no_crop, gamma=args.gamma, dither=not args.no_dither, dpi=args.dpi)
    for src in args.input:
        if args.output and len(args.input) == 1 and args.output.suffix == ".xtp":
            dst = args.output
        else:
            folder = args.output or src.parent
            folder.mkdir(parents=True, exist_ok=True)
            dst = folder / (src.stem + ".xtp")
        try:
            f = convert(src, dst, opts, args.title if len(args.input) == 1 else None, args.preview, args.max_pages)
        except (ValueError, OSError) as e:
            print(f"xtconvert: {e}", file=sys.stderr)
            return 2
        print(f"{dst}  ({len(f.pages)} screens, {dst.stat().st_size // 1024} KB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
