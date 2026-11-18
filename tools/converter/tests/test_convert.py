from pathlib import Path

import pymupdf
from PIL import Image

from xtconvert.cli import convert, main
from xtconvert.render import Options
from xtconvert.xtp import XtpFile, unpack_2bpp


def make_pdf(path: Path, pages: int = 3) -> Path:
    doc = pymupdf.open()
    for i in range(pages):
        page = doc.new_page(width=595, height=842)   # A4
        page.insert_text((72, 100), f"Page {i + 1}", fontsize=36)
        page.insert_textbox(pymupdf.Rect(72, 150, 523, 780), "Lorem ipsum dolor sit amet. " * 80, fontsize=11)
        page.insert_text((72, 790), "footer", fontsize=9)   # keeps the full page height after auto-crop
    doc.save(path)
    return path


def test_fit_mode(tmp_path):
    pdf = make_pdf(tmp_path / "a.pdf")
    out = tmp_path / "a.xtp"
    convert(pdf, out, Options(mode="fit"), preview=tmp_path / "prev", quiet=True)
    f = XtpFile.from_bytes(out.read_bytes())
    assert (f.width, f.height, len(f.pages)) == (300, 400, 3)
    assert f.title == "a"
    px = unpack_2bpp(f.pages[0])
    assert set(px) <= {0, 1, 2, 3}
    assert 0 in px and 3 in px                     # has ink and paper
    assert len(list((tmp_path / "prev").glob("*.png"))) == 3


def test_width_mode_splits_pages(tmp_path):
    pdf = make_pdf(tmp_path / "b.pdf", pages=2)
    out = tmp_path / "b.xtp"
    convert(pdf, out, Options(mode="width"), quiet=True)
    f = XtpFile.from_bytes(out.read_bytes())
    assert f.width == 300 and len(f.pages) > 2


def test_landscape_mode(tmp_path):
    pdf = make_pdf(tmp_path / "c.pdf", pages=1)
    out = tmp_path / "c.xtp"
    convert(pdf, out, Options(mode="landscape"), quiet=True)
    f = XtpFile.from_bytes(out.read_bytes())
    assert (f.width, f.height, len(f.pages)) == (400, 300, 2)


def test_image_folder(tmp_path):
    folder = tmp_path / "comic"
    folder.mkdir()
    for i in range(2):
        Image.linear_gradient("L").resize((600, 900)).save(folder / f"{i}.png")
    out = tmp_path / "comic.xtp"
    convert(folder, out, Options(crop=False), quiet=True)
    assert len(XtpFile.from_bytes(out.read_bytes()).pages) == 2


def test_cli(tmp_path, capsys):
    pdf = make_pdf(tmp_path / "d.pdf", pages=1)
    assert main([str(pdf), "-o", str(tmp_path / "out"), "--gamma", "1.3"]) == 0
    assert (tmp_path / "out" / "d.xtp").exists()
