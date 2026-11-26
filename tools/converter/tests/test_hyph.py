from pathlib import Path

import pytest

from xtconvert.cli import main as cli_main
from xtconvert.hyph import Hyp, build, exception_pattern, parse_tex, split_pattern

PATTERNS = Path(__file__).parent.parent / "patterns"
SDCARD = Path(__file__).parent.parent.parent.parent / "sdcard" / "hyph"


def test_split_pattern():
    assert split_pattern(".ab1c") == (".abc", [0, 0, 0, 1, 0])
    assert split_pattern("2b3") == ("b", [2, 3])


def test_exception_pattern():
    assert exception_pattern("ta-ble") == (".table.", [0, 8, 8, 9, 8, 8, 8, 0])


def test_parse_tex_reads_blocks_and_mins():
    text = "% hyphenmins:\n%     typesetting:\n%         left: 1\n%         right: 4\n" \
           "\\patterns{ % comment\n a1b\n .c2\n}\n\\hyphenation{\n ex-am-ple\n}\n"
    pats, exc, mins = parse_tex(text)
    assert pats == ["a1b", ".c2"]
    assert exc == ["ex-am-ple"]
    assert mins == (1, 4)


def test_roundtrip_and_liang():
    blob = build(["1ba", "a1b", "b2b"], ["ab-ab"], (1, 1))
    h = Hyp(blob)
    assert (h.left, h.right) == (1, 1)
    assert h.hyphenate("abba") == "a-bba"       # b2b beats 1ba
    assert h.hyphenate("aba") == "a-ba"
    assert h.hyphenate("abab") == "ab-ab"       # exception wins


def test_non_latin1_patterns_are_dropped():
    h = Hyp(build(["a1b", "ł1a"], [], (1, 1)))
    assert b"\xc5\x82a" not in h.table and len(h.table) == 1


@pytest.mark.parametrize("word,expected", [
    ("Silbentrennung", "Sil-ben-tren-nung"),
    ("Übersetzung", "Über-set-zung"),
    ("Schifffahrt", "Schiff-fahrt"),
])
def test_german(tmp_path, word, expected):
    out = tmp_path / "de.hyp"
    assert cli_main(["hyph", str(PATTERNS / "hyph-de-1996.tex"), "-o", str(out)]) == 0
    assert Hyp(out.read_bytes()).hyphenate(word) == expected


def test_english_with_exceptions(tmp_path):
    out = tmp_path / "en.hyp"
    assert cli_main(["hyph", str(PATTERNS / "hyph-en-us.tex"), "-o", str(out)]) == 0
    h = Hyp(out.read_bytes())
    assert (h.left, h.right) == (2, 3)
    assert h.hyphenate("hyphenation") == "hy-phen-ation"
    assert h.hyphenate("table") == "ta-ble"
    assert h.hyphenate("project") == "project"


@pytest.mark.parametrize("name,src", [("de.hyp", "hyph-de-1996.tex"), ("en.hyp", "hyph-en-us.tex")])
def test_committed_files_are_current(tmp_path, name, src):
    out = tmp_path / name
    cli_main(["hyph", str(PATTERNS / src), "-o", str(out)])
    assert (SDCARD / name).read_bytes() == out.read_bytes(), "rebuild sdcard/hyph with xtconvert hyph"
