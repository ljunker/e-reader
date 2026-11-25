"""Generates the binary fixtures for the native firmware tests.

    cd firmware && uv run --project ../tools/converter python test/fixtures/make_fixtures.py

Writes next to this script:
    sample.epub         EPUB 3 with nav, CSS, styles, cover (JPEG) and a PNG
    sample2.epub        EPUB 2 with an NCX table of contents, no nav, no cover
    drm.epub            EPUB with an encryption.xml that is not font obfuscation
    images/*.png|jpg    images for the decoder tests
    deflate_vectors.h   raw DEFLATE streams (stored, fixed, dynamic Huffman)
"""
from __future__ import annotations

import io
import zipfile
import zlib
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent


def gradient(w: int, h: int) -> Image.Image:
    img = Image.new("RGB", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            px[x, y] = (x * 255 // max(1, w - 1), y * 255 // max(1, h - 1), 128)
    return img


def jpeg_bytes(img: Image.Image, progressive: bool = False) -> bytes:
    b = io.BytesIO()
    img.save(b, "JPEG", quality=85, progressive=progressive)
    return b.getvalue()


def png_bytes(img: Image.Image) -> bytes:
    b = io.BytesIO()
    img.save(b, "PNG")
    return b.getvalue()


def write_epub(path: Path, files: dict[str, bytes | str]) -> None:
    with zipfile.ZipFile(path, "w") as z:
        z.writestr(zipfile.ZipInfo("mimetype"), "application/epub+zip", compress_type=zipfile.ZIP_STORED)
        for name, data in files.items():
            info = zipfile.ZipInfo(name, date_time=(2020, 1, 1, 0, 0, 0))   # reproducible output
            z.writestr(info, data, compress_type=zipfile.ZIP_DEFLATED)


CONTAINER = """<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>"""

OPF3 = """<?xml version="1.0" encoding="utf-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:identifier id="id">test-1</dc:identifier>
    <dc:title>Das Testbuch &amp; mehr</dc:title>
    <dc:language>de</dc:language>
  </metadata>
  <manifest>
    <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
    <item id="css" href="style.css" media-type="text/css"/>
    <item id="cover" href="images/cover.jpg" media-type="image/jpeg" properties="cover-image"/>
    <item id="pic" href="images/pic%20one.png" media-type="image/png"/>
    <item id="c1" href="text/ch1.xhtml" media-type="application/xhtml+xml"/>
    <item id="c2" href="text/ch2.xhtml" media-type="application/xhtml+xml"/>
  </manifest>
  <spine><itemref idref="c1"/><itemref idref="c2"/></spine>
</package>"""

NAV = """<?xml version="1.0" encoding="utf-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
<head><title>Inhalt</title></head>
<body>
  <nav epub:type="landmarks"><ol><li><a href="text/ch2.xhtml">Nicht das hier</a></li></ol></nav>
  <nav epub:type="toc"><h1>Inhalt</h1>
    <ol>
      <li><a href="text/ch1.xhtml">Kapitel 1</a>
        <ol><li><a href="text/ch1.xhtml#sec">Ein Abschnitt</a></li></ol>
      </li>
      <li><a href="text/ch2.xhtml"><span>Kapitel</span> 2</a></li>
    </ol>
  </nav>
</body></html>"""

CSS = """
/* test stylesheet */
.center { text-align: center }
.hidden { display: none; }
p.noindent { text-indent: 0; margin-top: 1em }
.b { font-weight: bold }
span.it, .also-it { font-style: italic }
h2 { page-break-before: always; text-align: center }
div p { font-weight: bold }
@media print { p { display: none } }
"""

CH1 = """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml">
<head><title>Kapitel 1</title><link rel="stylesheet" type="text/css" href="../style.css"/>
<style>.red { font-style: italic }</style></head>
<body>
  <h1>Kapitel 1</h1>
  <p>Ein <em>kursives</em> und <strong>fettes</strong> Wort &amp; &#8222;Zitat&#8220; &#x2014; 10&nbsp;km.</p>
  <p class="center">Mitte</p>
  <!-- a comment <p>not text</p> -->
  <div class="hidden"><p>Unsichtbar</p></div>
  <h2 id="sec">Abschnitt</h2>
  <p class="noindent">Bild folgt:</p>
  <p><img src="../images/pic%20one.png" alt="Bild"/></p>
  <ul><li>Erstens</li><li>Zweitens</li></ul>
  <p><span class="it">schräg</span><span class="b">dick</span>, <span style="font-weight: bold">inline</span> Łódź …</p>
  <script>var x = "<p>no</p>";</script>
</body></html>"""

CH2 = """<?xml version="1.0" encoding="utf-8"?>
<html xmlns="http://www.w3.org/1999/xhtml"><head><title>K2</title></head>
<body><h1>Kapitel 2</h1>
<p>Zeile eins<br/>Zeile zwei</p>
""" + "\n".join(
    f"<p>Absatz {i}: Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor.</p>"
    for i in range(40)
) + "\n</body></html>"

OPF2 = """<?xml version="1.0" encoding="utf-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="id">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Zwei</dc:title></metadata>
  <manifest>
    <item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>
    <item id="a" href="a.html" media-type="application/xhtml+xml"/>
    <item id="b" href="sub/b.html" media-type="application/xhtml+xml"/>
  </manifest>
  <spine toc="ncx"><itemref idref="a"/><itemref idref="b"/></spine>
</package>"""

NCX = """<?xml version="1.0" encoding="utf-8"?>
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">
  <navMap>
    <navPoint id="n1" playOrder="1"><navLabel><text>Erster Teil</text></navLabel><content src="a.html"/>
      <navPoint id="n2" playOrder="2"><navLabel><text>Unterpunkt</text></navLabel><content src="sub/b.html#x"/></navPoint>
    </navPoint>
  </navMap>
</ncx>"""

A_HTML = """<html><body><p>Erster Text.</p></body></html>"""
B_HTML = """<html><body><p>Vorher.</p><p id="x">Ziel.</p></body></html>"""

ENCRYPTION = """<?xml version="1.0"?>
<encryption xmlns="urn:oasis:names:tc:opendocument:xmlns:container" xmlns:enc="http://www.w3.org/2001/04/xmlenc#">
  <enc:EncryptedData><enc:EncryptionMethod Algorithm="http://www.w3.org/2001/04/xmlenc#aes128-cbc"/>
    <enc:CipherData><enc:CipherReference URI="OEBPS/text/ch1.xhtml"/></enc:CipherData></enc:EncryptedData>
</encryption>"""


def c_array(name: str, data: bytes) -> str:
    body = ",".join(str(b) for b in data)
    return f"static const uint8_t {name}[] = {{{body}}};\nstatic const size_t {name}_len = {len(data)};\n"


def raw_deflate(data: bytes, level: int) -> bytes:
    c = zlib.compressobj(level, zlib.DEFLATED, -15)
    return c.compress(data) + c.flush()


def main() -> None:
    cover = jpeg_bytes(gradient(600, 800))
    pic = Image.new("RGBA", (100, 50), (255, 255, 255, 0))
    for x in range(100):
        for y in range(10, 40):
            pic.putpixel((x, y), (0, 0, 0, 255))
    write_epub(HERE / "sample.epub", {
        "META-INF/container.xml": CONTAINER,
        "OEBPS/content.opf": OPF3,
        "OEBPS/nav.xhtml": NAV,
        "OEBPS/style.css": CSS,
        "OEBPS/images/cover.jpg": cover,
        "OEBPS/images/pic one.png": png_bytes(pic),
        "OEBPS/text/ch1.xhtml": CH1,
        "OEBPS/text/ch2.xhtml": CH2,
    })
    write_epub(HERE / "sample2.epub", {
        "META-INF/container.xml": CONTAINER,
        "OEBPS/content.opf": OPF2,
        "OEBPS/toc.ncx": NCX,
        "OEBPS/a.html": A_HTML,
        "OEBPS/sub/b.html": B_HTML,
    })
    write_epub(HERE / "drm.epub", {
        "META-INF/container.xml": CONTAINER,
        "META-INF/encryption.xml": ENCRYPTION,
        "OEBPS/content.opf": OPF2,
        "OEBPS/a.html": A_HTML,
    })

    img = HERE / "images"
    img.mkdir(exist_ok=True)
    (img / "photo.jpg").write_bytes(jpeg_bytes(gradient(640, 480)))
    (img / "progressive.jpg").write_bytes(jpeg_bytes(gradient(64, 48), progressive=True))
    gray = Image.new("L", (40, 20), 255)
    for x in range(20):
        for y in range(20):
            gray.putpixel((x, y), 0)
    (img / "gray.png").write_bytes(png_bytes(gray))
    (img / "gray16.png").write_bytes(png_bytes(Image.eval(gray.convert("I"), lambda v: v * 257).convert("I;16")))
    (img / "rgba.png").write_bytes(png_bytes(pic))
    (img / "palette.png").write_bytes(png_bytes(gray.convert("RGB").quantize(4)))
    rgb = Image.new("RGB", (8, 8), (255, 0, 0))   # red -> luma 76
    (img / "rgb.png").write_bytes(png_bytes(rgb))

    text = b"hello hello hello hello, deflate!"
    long_text = b"".join(b"Line %d: the quick brown fox jumps over the lazy dog.\n" % i for i in range(300))
    vectors = (
        "// Generated by make_fixtures.py\n#pragma once\n#include <stddef.h>\n#include <stdint.h>\n"
        + c_array("kShortText", text)
        + c_array("kStored", raw_deflate(text, 0))
        + c_array("kFixed", raw_deflate(text, 9))
        + c_array("kLongText", long_text)
        + c_array("kDynamic", raw_deflate(long_text, 9))
        + c_array("kZlib", zlib.compress(long_text, 6))
    )
    (HERE / "deflate_vectors.h").write_text(vectors)


if __name__ == "__main__":
    main()
