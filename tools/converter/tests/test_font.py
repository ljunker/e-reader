from pathlib import Path

import pytest

from xtconvert.cli import main as cli_main
from xtconvert.font import CHARSET, Xff, build, pack_levels, parse_faces, quantize, render_face

FONTS = Path(__file__).parent.parent / "fonts"
ROOT = Path(__file__).parent.parent.parent.parent
LITERATA = FONTS / "literata" / "Literata[opsz,wght].ttf"
LITERATA_I = FONTS / "literata" / "Literata-Italic[opsz,wght].ttf"
INTER = FONTS / "inter" / "Inter[opsz,wght].ttf"

# How the committed files are built (README, sdcard/fonts).
CARD_ARGS = [str(LITERATA), "--italic", str(LITERATA_I)]
BUILTIN_ARGS = [str(INTER), "--faces", "regular:11,regular:14,bold:16", "--name", "Inter", "--symbol", "kBuiltinFont"]


def test_quantize():
    assert [quantize(v, 1.0) for v in (0, 42, 43, 128, 212, 213, 255)] == [0, 0, 1, 2, 2, 3, 3]
    assert quantize(100, 1.5) > quantize(100, 1.0)


def test_pack_levels():
    assert pack_levels(bytes([3, 2, 1, 0, 3]), 5, 1) == bytes([0b11100100, 0b11000000])
    assert pack_levels(bytes([1, 2]), 1, 2) == bytes([0x40, 0x80])


def test_parse_faces():
    assert parse_faces("regular:13,bold-italic:9") == [(0, 13), (3, 9)]


def test_roundtrip():
    faces = [render_face(LITERATA, 1, 16, 700, 1.3, list("Hab?")), render_face(LITERATA, 0, 16, 400, 1.3, list("Hab?"))]
    x = Xff(build("Test", faces))
    assert x.name == "Test"
    assert [(f.style, f.size) for f in x.faces] == [(0, 16), (1, 16)]   # sorted
    for f in faces:
        g = x.face(f.style, f.size)
        assert [(a.cp, a.advance, a.left, a.top, a.width, a.height, a.levels) for a in g.glyphs] == \
               [(a.cp, a.advance, a.left, a.top, a.width, a.height, a.levels) for a in sorted(f.glyphs, key=lambda a: a.cp)]
    # bold is wider than regular
    assert x.width(x.face(1, 16), "Hab") > x.width(x.face(0, 16), "Hab")


def test_charset_coverage():
    f = render_face(LITERATA, 0, 17, 400, 1.3)
    have = {g.cp for g in f.glyphs}
    assert {ord(c) for c in CHARSET} == have       # Literata has every character
    space = next(g for g in f.glyphs if g.cp == 32)
    assert space.width == 0 and space.advance > 0
    assert all(max(g.levels) == 3 for g in f.glyphs if chr(g.cp).isalnum())   # letters reach full ink


def test_header_output(tmp_path):
    out = tmp_path / "f.h"
    assert cli_main(["font", str(INTER), "--faces", "regular:12", "-o", str(out), "--symbol", "kTest"]) == 0
    text = out.read_text()
    assert "alignas(4) static const uint8_t kTest[" in text and text.rstrip().endswith("};")


def test_preview(tmp_path):
    out = tmp_path / "f.xff"
    assert cli_main(["font", str(INTER), "--faces", "bold:12", "-o", str(out), "--preview", str(tmp_path / "p")]) == 0
    assert {p.name for p in (tmp_path / "p").iterdir()} == {"bold-12.png", "bold-12-bw.png"}


@pytest.mark.parametrize("args,committed", [
    (CARD_ARGS, ROOT / "sdcard" / "fonts" / "literata.xff"),
    (BUILTIN_ARGS, ROOT / "firmware" / "src" / "BuiltinFont.h"),
])
def test_committed_files_are_current(tmp_path, args, committed):
    out = tmp_path / committed.name
    assert cli_main(["font", *args, "-o", str(out)]) == 0
    old, new = committed.read_bytes(), out.read_bytes()
    if committed.suffix == ".h":   # the comment names the source file only
        old, new = old.split(b"\n", 2)[2], new.split(b"\n", 2)[2]
    assert old == new, "rebuild with xtconvert font (see tests/test_font.py)"
