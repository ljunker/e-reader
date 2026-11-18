#!/usr/bin/env python3
"""Collision check for the case: renders the intersection of every printed part
with every electronics stand-in (and with each other) and reports overlaps that
have real volume. Faces that merely touch are ignored.

Usage: python3 check.py [path/to/openscad]
"""
import itertools
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
OPENSCAD = (sys.argv[1] if len(sys.argv) > 1 else None) or shutil.which("openscad") \
    or "/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD"
PARTS = ["bezel", "chassis", "back_shell"]
DUMMIES = ["display_dummy", "feather_dummy", "sd_dummy", "battery_dummy", "buttons_dummy"]
MIN_DEPTH = 0.05  # mm; thinner overlaps are touching faces / rounding noise

HEADER = "".join(f"use <{HERE}/{f}>\n" for f in ("bezel.scad", "chassis.scad", "back.scad")) \
    + f"include <{HERE}/common.scad>\n"


def overlap(a, b, tmp):
    scad = os.path.join(tmp, "t.scad")
    stl = os.path.join(tmp, "t.stl")
    with open(scad, "w") as f:
        f.write(HEADER + f"intersection() {{ {a}(); {b}(); }}\n")
    if os.path.exists(stl):
        os.remove(stl)
    r = subprocess.run([OPENSCAD, "--backend=manifold", "-o", stl, scad], capture_output=True, text=True)
    if "empty" in r.stderr or not os.path.exists(stl):
        return None
    pts = [tuple(map(float, line.split()[1:])) for line in open(stl) if line.strip().startswith("vertex")]
    if not pts:
        return None
    lo = [min(p[i] for p in pts) for i in range(3)]
    hi = [max(p[i] for p in pts) for i in range(3)]
    if min(h - l for l, h in zip(lo, hi)) < MIN_DEPTH:
        return None
    return lo, hi


def main():
    pairs = [(p, d) for p in PARTS for d in DUMMIES] + list(itertools.combinations(PARTS, 2))
    bad = 0
    with tempfile.TemporaryDirectory() as tmp:
        for a, b in pairs:
            box = overlap(a, b, tmp)
            if box:
                bad += 1
                lo, hi = box
                print(f"COLLISION {a} × {b}: x {lo[0]:.2f}..{hi[0]:.2f}  y {lo[1]:.2f}..{hi[1]:.2f}  z {lo[2]:.2f}..{hi[2]:.2f}")
    print("check: no collisions" if not bad else f"check: {bad} collision(s)")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
