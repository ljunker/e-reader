import random

import pytest

from xtconvert.xtp import XtpFile, pack_2bpp, rle_decode, rle_encode, unpack_2bpp


@pytest.mark.parametrize("data", [
    b"", b"\x00", b"\xff" * 1000, bytes(range(256)) * 3, b"ab" * 200 + b"\x00" * 300 + b"xyz",
])
def test_rle_roundtrip(data):
    assert rle_decode(rle_encode(data)) == data


def test_rle_random_roundtrip():
    rng = random.Random(1)
    for _ in range(200):
        data = bytes(rng.choice([0, 0, 0, 255, rng.randrange(256)]) for _ in range(rng.randrange(1, 2000)))
        assert rle_decode(rle_encode(data)) == data


def test_rle_compresses_blank_page():
    page = b"\xff" * (300 * 400 // 4)
    assert len(rle_encode(page)) < 500


def test_pack_unpack():
    px = bytes([0, 1, 2, 3] * 10)
    packed = pack_2bpp(px, 8, 5)
    assert packed[0] == 0b00011011
    assert unpack_2bpp(packed) == px


def test_file_roundtrip():
    f = XtpFile(8, 2, "Tëst")
    f.add_page(bytes([3] * 16))
    f.add_page(bytes([0, 1, 2, 3] * 4))
    g = XtpFile.from_bytes(f.to_bytes())
    assert (g.width, g.height, g.title) == (8, 2, "Tëst")
    assert g.pages == f.pages


def test_rejects_bad_magic():
    with pytest.raises(ValueError):
        XtpFile.from_bytes(b"NOPE" + bytes(60))
