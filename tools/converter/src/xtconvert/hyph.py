"""Liang hyphenation patterns (TeX hyph-utf8 .tex files) → .hyp files for the e-reader.

    xtconvert hyph patterns/hyph-de-1996.tex -o de.hyp

Format (all integers little-endian; the reader keeps the file in PSRAM and
searches it in place):

    "HYP1"  u8 leftMin, u8 rightMin, u8 maxLen, u8 0
    maxLen × (u32 count, u32 vectors, u32 letters, u32 index)   per pattern length L = 1..maxLen
    data

For each length L the data holds, at the given offsets:
    vectors  nvec × (L + 1) digit bytes: the distinct digit vectors of this length
    letters  count × L bytes: the letter strings, sorted (Latin-1, lowercase, '.' = word edge)
    index    count × u8: digit vector of each pattern

Exceptions (\\hyphenation{ta-ble}) become whole-word patterns with 9 at the
breaks and 8 everywhere else, which beats every TeX digit (at most 5).
"""
from __future__ import annotations

import argparse
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

MAGIC = b"HYP1"


def parse_tex(text: str) -> tuple[list[str], list[str], tuple[int, int]]:
    """Patterns, exceptions and (left, right) hyphenmins of a hyph-utf8 .tex file."""
    mins = [2, 3]
    m = re.search(r"%\s*typesetting:\s*\n%\s*left:\s*(\d+)\s*\n%\s*right:\s*(\d+)", text)
    if m:
        mins = [int(m.group(1)), int(m.group(2))]
    body = "\n".join(line.split("%", 1)[0] for line in text.splitlines())

    def block(name: str) -> list[str]:
        m = re.search(r"\\" + name + r"\s*\{([^}]*)\}", body)
        return m.group(1).split() if m else []

    return block("patterns"), block("hyphenation"), (mins[0], mins[1])


def split_pattern(p: str) -> tuple[str, list[int]]:
    letters, digits = [], [0]
    for ch in p:
        if ch.isdigit():
            digits[-1] = int(ch)
        else:
            letters.append(ch)
            digits.append(0)
    return "".join(letters), digits


def exception_pattern(word: str) -> tuple[str, list[int]]:
    letters = word.replace("-", "")
    digits = [8] * (len(letters) + 1)
    i = 0
    for ch in word:
        if ch == "-":
            digits[i] = 9
        else:
            i += 1
    return "." + letters + ".", [0] + digits + [0]


def build(patterns: list[str], exceptions: list[str], mins: tuple[int, int]) -> bytes:
    table: dict[str, list[int]] = {}
    for p in patterns:
        letters, digits = split_pattern(p.lower())
        table[letters] = digits
    for w in exceptions:
        letters, digits = exception_pattern(w.lower())
        table[letters] = digits
    by_len: dict[int, list[tuple[bytes, tuple[int, ...]]]] = defaultdict(list)
    for letters, digits in table.items():
        try:
            raw = letters.encode("latin-1")
        except UnicodeEncodeError:
            continue   # the reader's text is Latin-1 only
        by_len[len(raw)].append((raw, tuple(digits)))
    max_len = max(by_len)
    if max_len > 255:
        raise ValueError("pattern too long")

    head = MAGIC + struct.pack("<BBBB", mins[0], mins[1], max_len, 0)
    data = bytearray()
    base = len(head) + 16 * max_len
    dirs = []
    for length in range(1, max_len + 1):
        items = sorted(by_len.get(length, []))
        vecs = sorted({d for _, d in items})
        if len(vecs) > 256:
            raise ValueError(f"more than 256 digit vectors of length {length}")
        vec_no = {v: i for i, v in enumerate(vecs)}
        vec_off = base + len(data)
        for v in vecs:
            data += bytes(v)
        let_off = base + len(data)
        for raw, _ in items:
            data += raw
        idx_off = base + len(data)
        data += bytes(vec_no[d] for _, d in items)
        dirs.append(struct.pack("<IIII", len(items), vec_off, let_off, idx_off))
    return head + b"".join(dirs) + bytes(data)


class Hyp:
    """Reads a .hyp file and hyphenates (reference for tests and checks)."""

    def __init__(self, blob: bytes):
        if blob[:4] != MAGIC:
            raise ValueError("not a .hyp file")
        self.left, self.right, self.max_len = blob[4], blob[5], blob[6]
        self.table: dict[bytes, bytes] = {}
        for length in range(1, self.max_len + 1):
            count, vec_off, let_off, idx_off = struct.unpack_from("<IIII", blob, 8 + 16 * (length - 1))
            for k in range(count):
                letters = blob[let_off + k * length:let_off + (k + 1) * length]
                v = blob[idx_off + k]
                self.table[letters] = blob[vec_off + v * (length + 1):vec_off + (v + 1) * (length + 1)]

    def hyphenate(self, word: str) -> str:
        w = b"." + word.lower().encode("latin-1") + b"."
        pts = [0] * (len(w) + 1)
        for i in range(len(w)):
            for j in range(i + 1, min(len(w), i + self.max_len) + 1):
                d = self.table.get(w[i:j])
                if d:
                    for k, v in enumerate(d):
                        pts[i + k] = max(pts[i + k], v)
        out = []
        n = len(word)
        for i, ch in enumerate(word):
            if i >= self.left and n - i >= self.right and pts[i + 1] % 2:
                out.append("-")
            out.append(ch)
        return "".join(out)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="xtconvert hyph", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", type=Path, help="hyph-utf8 .tex pattern file")
    ap.add_argument("-o", "--output", type=Path, required=True)
    ap.add_argument("--min", type=int, nargs=2, metavar=("LEFT", "RIGHT"),
                    help="shortest piece before / after a break (default: from the file)")
    args = ap.parse_args(argv)
    try:
        patterns, exceptions, mins = parse_tex(args.input.read_text(encoding="utf-8"))
        if not patterns:
            raise ValueError(f"{args.input}: no \\patterns found")
        blob = build(patterns, exceptions, tuple(args.min) if args.min else mins)
    except (ValueError, OSError) as e:
        print(f"xtconvert hyph: {e}", file=sys.stderr)
        return 2
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(blob)
    print(f"{args.output}  ({len(patterns)} patterns, {len(exceptions)} exceptions, {len(blob) // 1024} KB)")
    return 0
