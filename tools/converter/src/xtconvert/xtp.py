"""The .xtp page-file format (mirrored by firmware/src/xtp/).

All integers little-endian.

Header, 64 bytes:
    0   4  magic  b"XTP1"
    4   2  version (1)
    6   2  width  in pixels (multiple of 4)
    8   2  height in pixels
   10   1  bits per pixel (2)
   11   1  flags (bit 0: pages are RLE-compressed)
   12   4  page count
   16   4  offset of the page index
   20  44  title, UTF-8, NUL-padded

Page index: page count × (u32 offset, u32 length) of each page's data.

Page data: rows top to bottom, 4 pixels per byte, first pixel in the two most
significant bits. Pixel values: 0 = black, 1 = dark gray, 2 = light gray,
3 = white (same order as the SSD1683 4-gray mode).

RLE (PackBits-like), one control byte c then:
    c < 128   copy the next c + 1 bytes literally   (1..128 bytes)
    c >= 128  repeat the next byte c - 126 times     (2..129 bytes)
"""
from __future__ import annotations

import struct
from dataclasses import dataclass, field

MAGIC = b"XTP1"
VERSION = 1
HEADER_FMT = "<4sHHHBBII44s"
HEADER_SIZE = struct.calcsize(HEADER_FMT)
assert HEADER_SIZE == 64
FLAG_RLE = 0x01


def rle_encode(data: bytes) -> bytes:
    out = bytearray()
    i, n = 0, len(data)
    literal = bytearray()

    def flush_literal():
        while literal:
            chunk = literal[:128]
            out.append(len(chunk) - 1)
            out.extend(chunk)
            del literal[:128]

    while i < n:
        run = 1
        while i + run < n and run < 129 and data[i + run] == data[i]:
            run += 1
        if run >= 2:
            flush_literal()
            out.append(run + 126)
            out.append(data[i])
            i += run
        else:
            literal.append(data[i])
            i += 1
    flush_literal()
    return bytes(out)


def rle_decode(data: bytes, expected: int | None = None) -> bytes:
    out = bytearray()
    i = 0
    while i < len(data):
        c = data[i]
        i += 1
        if c < 128:
            out.extend(data[i:i + c + 1])
            i += c + 1
        else:
            out.extend(bytes([data[i]]) * (c - 126))
            i += 1
    if expected is not None and len(out) != expected:
        raise ValueError(f"RLE page decodes to {len(out)} bytes, expected {expected}")
    return bytes(out)


def pack_2bpp(pixels: bytes, width: int, height: int) -> bytes:
    """pixels: one byte per pixel with values 0..3, row-major."""
    if width % 4:
        raise ValueError("width must be a multiple of 4")
    if len(pixels) != width * height:
        raise ValueError("pixel count does not match size")
    out = bytearray(width * height // 4)
    for j in range(len(out)):
        p = pixels[4 * j:4 * j + 4]
        out[j] = (p[0] << 6) | (p[1] << 4) | (p[2] << 2) | p[3]
    return bytes(out)


def unpack_2bpp(packed: bytes) -> bytes:
    out = bytearray(len(packed) * 4)
    for j, b in enumerate(packed):
        out[4 * j] = b >> 6
        out[4 * j + 1] = (b >> 4) & 3
        out[4 * j + 2] = (b >> 2) & 3
        out[4 * j + 3] = b & 3
    return bytes(out)


@dataclass
class XtpFile:
    width: int
    height: int
    title: str = ""
    pages: list[bytes] = field(default_factory=list)   # packed 2bpp, uncompressed

    def page_bytes(self) -> int:
        return self.width * self.height // 4

    def add_page(self, pixels: bytes) -> None:
        self.pages.append(pack_2bpp(pixels, self.width, self.height))

    def to_bytes(self, rle: bool = True) -> bytes:
        title = self.title.encode("utf-8")[:43]
        index_offset = HEADER_SIZE
        data_offset = index_offset + 8 * len(self.pages)
        blobs = [rle_encode(p) if rle else p for p in self.pages]
        header = struct.pack(HEADER_FMT, MAGIC, VERSION, self.width, self.height, 2,
                             FLAG_RLE if rle else 0, len(self.pages), index_offset, title)
        index = bytearray()
        off = data_offset
        for b in blobs:
            index += struct.pack("<II", off, len(b))
            off += len(b)
        return header + bytes(index) + b"".join(blobs)

    @classmethod
    def from_bytes(cls, data: bytes) -> "XtpFile":
        magic, version, w, h, bpp, flags, count, index_offset, title = struct.unpack_from(HEADER_FMT, data)
        if magic != MAGIC:
            raise ValueError("not an .xtp file")
        if version != VERSION or bpp != 2:
            raise ValueError(f"unsupported .xtp version {version} / {bpp} bpp")
        f = cls(w, h, title.rstrip(b"\0").decode("utf-8", "replace"))
        for k in range(count):
            off, length = struct.unpack_from("<II", data, index_offset + 8 * k)
            blob = data[off:off + length]
            f.pages.append(rle_decode(blob, f.page_bytes()) if flags & FLAG_RLE else blob)
        return f
