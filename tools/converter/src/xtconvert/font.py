"""TrueType / OpenType fonts → .xff bitmap font files for the e-reader.

    xtconvert font "fonts/literata/Literata[opsz,wght].ttf" --italic "fonts/literata/Literata-Italic[opsz,wght].ttf" \
        -o ../../sdcard/fonts/literata.xff

Each glyph is rendered anti-aliased by FreeType (through Pillow) and stored
with 4 ink levels (2 bits per pixel). The reader shows them in 4 grays, or in
black and white by treating levels 2 and 3 as black.

Format (all integers little-endian; the reader keeps the file in PSRAM or
flash and reads it in place):

    "XFF1"  u8 faceCount, u8 0, u16 0, char name[24] (NUL-padded UTF-8)
    faceCount × face (16 bytes):
        u8 style (0 regular, 1 bold, 2 italic, 3 bold italic), u8 sizePx,
        u8 ascent, u8 descent (pixels above / below the baseline, over all glyphs),
        u16 glyphCount, u16 0, u32 glyphsOffset, u32 bitmapOffset
    per face, at glyphsOffset: glyphCount × glyph (12 bytes), sorted by code point:
        u16 codePoint, u16 advance (1/16 pixel), i8 left (bearing), i8 top (rows above the baseline),
        u8 width, u8 height, u32 offset of the bitmap from bitmapOffset
    bitmaps: rows of ceil(width / 4) bytes, 4 pixels per byte, leftmost pixel in
        the top two bits; 0 = paper .. 3 = full ink

Faces are sorted by style, then size.
"""
from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

MAGIC = b"XFF1"
STYLES = ("regular", "bold", "italic", "bold italic")
GLYPH = struct.Struct("<HHbbBBI")
FACE = struct.Struct("<BBBBHHII")

# Latin-1 plus the typographic punctuation books use.
EXTRA = "‐‑–—‘’‚“”„†‡•…‹›€™�"
CHARSET = [chr(c) for c in range(0x20, 0x7F)] + [chr(c) for c in range(0xA0, 0x100)] + list(EXTRA)


@dataclass
class Glyph:
    cp: int
    advance: int        # 1/16 pixel
    left: int
    top: int
    width: int
    height: int
    levels: bytes       # width * height values 0..3


@dataclass
class Face:
    style: int
    size: int
    ascent: int
    descent: int
    glyphs: list[Glyph]


def _set_axes(font: ImageFont.FreeTypeFont, size: int, weight: int) -> None:
    try:
        axes = font.get_variation_axes()
    except (OSError, AttributeError):
        return   # not a variable font
    values = []
    for a in axes:
        name = a["name"].decode() if isinstance(a["name"], bytes) else a["name"]
        if name.lower().startswith("weight"):
            v = weight
        elif name.lower().startswith("optical"):
            v = size * 0.75   # pixels at 96 dpi → points
        else:
            v = a["default"]
        values.append(min(max(v, a["minimum"]), a["maximum"]))
    font.set_variation_by_axes(values)


def quantize(coverage: int, gamma: float) -> int:
    """FreeType coverage 0..255 → ink level 0..3; gamma > 1 darkens thin strokes."""
    c = (coverage / 255) ** (1 / gamma)
    return min(3, int(c * 3 + 0.5))


def render_face(path: Path, style: int, size: int, weight: int, gamma: float, charset=CHARSET) -> Face:
    font = ImageFont.truetype(str(path), size)
    _set_axes(font, size, weight)
    glyphs = []
    for ch in charset:
        if ch == chr(0xAD):   # soft hyphen: invisible inside a word
            glyphs.append(Glyph(0xAD, 0, 0, 0, 0, 0, b""))
            continue
        if ch not in (" ", chr(0xA0)) and font.getmask(ch).getbbox() is None:
            continue   # not in the font
        advance = round(font.getlength(ch) * 16)
        x0, y0, x1, y1 = font.getbbox(ch, anchor="ls")
        w, h = max(0, x1 - x0), max(0, y1 - y0)
        levels = b""
        if w and h:
            img = Image.new("L", (w, h), 0)
            ImageDraw.Draw(img).text((-x0, -y0), ch, font=font, fill=255, anchor="ls")
            levels = bytes(quantize(v, gamma) for v in img.tobytes())
            # trim rows and columns without ink
            rows = [r for r in range(h) if any(levels[r * w:(r + 1) * w])]
            cols = [c for c in range(w) if any(levels[r * w + c] for r in range(h))]
            if not rows:
                w = h = 0
                levels = b""
            else:
                r0, r1, c0, c1 = rows[0], rows[-1] + 1, cols[0], cols[-1] + 1
                levels = bytes(levels[r * w + c] for r in range(r0, r1) for c in range(c0, c1))
                x0, y0 = x0 + c0, y0 + r0
                w, h = c1 - c0, r1 - r0
        if w > 255 or h > 255 or not -128 <= x0 <= 127 or not -128 <= -y0 <= 127:
            raise ValueError(f"glyph {ch!r} at {size} px is too large")
        if not (w and h):
            x0 = y0 = w = h = 0
        glyphs.append(Glyph(ord(ch), advance, x0, -y0, w, h, levels))
    inked = [g for g in glyphs if g.height]
    ascent = max(g.top for g in inked)
    descent = max(0, max(g.height - g.top for g in inked))
    return Face(style, size, ascent, descent, glyphs)


def pack_levels(levels: bytes, w: int, h: int) -> bytes:
    out = bytearray()
    for r in range(h):
        row = levels[r * w:(r + 1) * w]
        for i in range(0, w, 4):
            b = 0
            for k in range(4):
                v = row[i + k] if i + k < w else 0
                b |= v << (6 - 2 * k)
            out.append(b)
    return bytes(out)


def build(name: str, faces: list[Face]) -> bytes:
    faces = sorted(faces, key=lambda f: (f.style, f.size))
    head = MAGIC + struct.pack("<BBH", len(faces), 0, 0) + name.encode()[:23].ljust(24, b"\0")
    base = len(head) + FACE.size * len(faces)
    data = bytearray()
    dirs = []
    for f in faces:
        glyphs = sorted(f.glyphs, key=lambda g: g.cp)
        bitmaps = bytearray()
        offsets = []
        for g in glyphs:
            offsets.append(len(bitmaps))
            bitmaps += pack_levels(g.levels, g.width, g.height)
        glyph_off = base + len(data)
        for g, o in zip(glyphs, offsets):
            data += GLYPH.pack(g.cp, g.advance, g.left, g.top, g.width, g.height, o)
        bitmap_off = base + len(data)
        data += bitmaps
        while len(data) % 4:
            data.append(0)
        dirs.append(FACE.pack(f.style, f.size, f.ascent, f.descent, len(glyphs), 0, glyph_off, bitmap_off))
    return head + b"".join(dirs) + bytes(data)


class Xff:
    """Reads a .xff file (reference for tests and previews)."""

    def __init__(self, blob: bytes):
        if blob[:4] != MAGIC:
            raise ValueError("not a .xff file")
        count = blob[4]
        self.name = blob[8:32].rstrip(b"\0").decode()
        self.faces: list[Face] = []
        for i in range(count):
            style, size, asc, desc, n, _, goff, boff = FACE.unpack_from(blob, 32 + FACE.size * i)
            glyphs = []
            for k in range(n):
                cp, adv, left, top, w, h, off = GLYPH.unpack_from(blob, goff + GLYPH.size * k)
                stride = (w + 3) // 4
                levels = bytes((blob[boff + off + r * stride + c // 4] >> (6 - 2 * (c % 4))) & 3
                               for r in range(h) for c in range(w))
                glyphs.append(Glyph(cp, adv, left, top, w, h, levels))
            self.faces.append(Face(style, size, asc, desc, glyphs))

    def face(self, style: int, size: int) -> Face:
        return next(f for f in self.faces if f.style == style and f.size == size)

    def width(self, face: Face, text: str) -> int:
        by_cp = {g.cp: g for g in face.glyphs}
        return (sum(by_cp.get(ord(c), by_cp[ord("?")]).advance for c in text) + 8) >> 4

    def render(self, face: Face, text: str, black_white: bool = False) -> Image.Image:
        """One line of text as a grayscale picture, as the reader draws it."""
        by_cp = {g.cp: g for g in face.glyphs}
        img = Image.new("L", (self.width(face, text) + 4, face.ascent + face.descent + 4), 255)
        px = img.load()
        pen = 0
        shades = (255, 255, 0, 0) if black_white else (255, 170, 85, 0)
        for c in text:
            g = by_cp.get(ord(c), by_cp[ord("?")])
            gx = 2 + ((pen + 8) >> 4) + g.left
            gy = 2 + face.ascent - g.top
            for r in range(g.height):
                for col in range(g.width):
                    v = g.levels[r * g.width + col]
                    if v:
                        px[gx + col, gy + r] = shades[v]
            pen += g.advance
        return img


def to_header(blob: bytes, symbol: str, source: str) -> str:
    lines = [f"#pragma once\n// Generated by 'xtconvert font {source}'. Do not edit.\n",
             "#include <stdint.h>\n",
             f"alignas(4) static const uint8_t {symbol}[{len(blob)}] = {{"]
    for i in range(0, len(blob), 20):
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in blob[i:i + 20]) + ",")
    lines.append("};\n")
    return "\n".join(lines)


def parse_faces(spec: str) -> list[tuple[int, int]]:
    """"regular:13,bold:15" → [(0, 13), (1, 15)]."""
    out = []
    for item in spec.split(","):
        style, _, size = item.partition(":")
        out.append((STYLES.index(style.replace("-", " ")), int(size)))
    return out


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="xtconvert font", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("regular", type=Path, help="regular TTF/OTF (a variable font gives bold through its weight axis)")
    ap.add_argument("--bold", type=Path, help="bold file (default: the regular one at --bold-weight)")
    ap.add_argument("--italic", type=Path, help="italic file (default: the regular one)")
    ap.add_argument("--bold-italic", type=Path, help="bold italic file (default: the italic one at --bold-weight)")
    ap.add_argument("--sizes", default="15,17,20,24,29", help="pixel sizes, smallest first; every style gets each")
    ap.add_argument("--faces", help="only these faces, e.g. regular:10,regular:13,bold:15 (instead of --sizes)")
    ap.add_argument("--weight", type=int, default=400)
    ap.add_argument("--bold-weight", type=int, default=700)
    ap.add_argument("--gamma", type=float, default=1.3, help=">1 darkens thin strokes")
    ap.add_argument("--name", help="family name shown on the reader (default: from the file)")
    ap.add_argument("-o", "--output", type=Path, required=True, help=".xff file, or .h for a C array")
    ap.add_argument("--symbol", default="kBuiltinFont", help="array name for -o *.h")
    ap.add_argument("--preview", type=Path, help="also write a sample PNG of every face into this folder")
    args = ap.parse_args(argv)

    files = {0: args.regular, 1: args.bold or args.regular, 2: args.italic or args.regular}
    files[3] = args.bold_italic or (args.italic if args.italic else files[1])
    weights = {0: args.weight, 1: args.bold_weight, 2: args.weight, 3: args.bold_weight}
    if args.faces:
        wanted = parse_faces(args.faces)
    else:
        sizes = [int(s) for s in args.sizes.split(",")]
        wanted = [(st, sz) for st in range(4) for sz in sizes]
    try:
        name = args.name or ImageFont.truetype(str(args.regular), 12).getname()[0]
        faces = [render_face(files[st], st, sz, weights[st], args.gamma) for st, sz in wanted]
        blob = build(name, faces)
    except (ValueError, OSError) as e:
        print(f"xtconvert font: {e}", file=sys.stderr)
        return 2
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.output.suffix == ".h":
        args.output.write_text(to_header(blob, args.symbol, args.regular.name))
    else:
        args.output.write_bytes(blob)
    if args.preview:
        args.preview.mkdir(parents=True, exist_ok=True)
        x = Xff(blob)
        sample = "Grüße aus Köln – „Typografie“ für 300 px … Hamburgefonstiv 0123"
        for f in x.faces:
            for bw in (False, True):
                tag = f"{STYLES[f.style].replace(' ', '-')}-{f.size}{'-bw' if bw else ''}"
                x.render(f, sample, bw).save(args.preview / f"{tag}.png")
    print(f"{args.output}  {name}: {len(faces)} faces, {len(blob) // 1024} KB")
    return 0
