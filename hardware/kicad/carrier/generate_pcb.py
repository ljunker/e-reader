"""Builds carrier.kicad_pcb from design.py. Run with KiCad's Python:

    /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 generate_pcb.py

Places footprints, draws the outline from cad/params.scad, adds GND pours,
net classes and silkscreen. Routing is done afterwards by route.sh.
"""
import math
import os
import sys
import uuid

import pcbnew

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import design  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "carrier.kicad_pcb")
OX, OY = 50.0, 40.0   # sheet offset of the case origin

P = design.case_params()
W, H = P["case_w"], P["case_h"]


def k(x, y):
    """Case coordinates (front view, y up) -> KiCad board coordinates (back view, y down)."""
    return OX + (W - x), OY + (H - y)


def v(x, y):
    return pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y))


def mm(p):
    return pcbnew.ToMM(p.x), pcbnew.ToMM(p.y)


def sym_uuid(ref):
    # deterministic, shared with generate_sch.py so the schematic links to the PCB
    return str(uuid.uuid5(uuid.NAMESPACE_URL, "e-reader-carrier/" + ref))


# ---------------------------------------------------------------- helpers
LIB_LOCAL = {}   # ref -> pad positions as in the library (before any flip)


def affine_from_pads(fp):
    """Map library footprint coordinates to board coordinates using its pads."""
    pts = []
    for (lx, ly), pad in zip(LIB_LOCAL[fp.GetReference()], fp.Pads()):
        bx, by = mm(pad.GetPosition())
        pts.append((lx, ly, bx, by))
    # pick three pads that are not collinear
    for i in range(len(pts)):
        for j in range(i + 1, len(pts)):
            for q in range(j + 1, len(pts)):
                (x1, y1, X1, Y1), (x2, y2, X2, Y2), (x3, y3, X3, Y3) = pts[i], pts[j], pts[q]
                det = (x2 - x1) * (y3 - y1) - (x3 - x1) * (y2 - y1)
                if abs(det) < 1e-6:
                    continue
                def solve(a1, a2, a3):
                    a = ((a2 - a1) * (y3 - y1) - (a3 - a1) * (y2 - y1)) / det
                    b = ((x2 - x1) * (a3 - a1) - (x3 - x1) * (a2 - a1)) / det
                    return a, b, a1 - a * x1 - b * y1
                ax, bx_, cx = solve(X1, X2, X3)
                ay, by_, cy = solve(Y1, Y2, Y3)
                return lambda lx, ly: (ax * lx + bx_ * ly + cx, ay * lx + by_ * ly + cy)
    # all pads in a row: use two of them plus the handedness (mirrored on layer B)
    (x1, y1, X1, Y1), (x2, y2, X2, Y2) = pts[0], max(pts[1:], key=lambda p: abs(p[0] - pts[0][0]) + abs(p[1] - pts[0][1]))
    L = math.hypot(x2 - x1, y2 - y1)
    ux, uy = (x2 - x1) / L, (y2 - y1) / L          # local unit vector along the row
    Ex, Ey = (X2 - X1) / L, (Y2 - Y1) / L          # its image on the board
    # image of the local perpendicular (-uy, ux): rotate by +90° (proper) or -90° (mirrored)
    Px, Py = (-Ey, Ex) if not fp.IsFlipped() else (Ey, -Ex)

    def f(lx, ly):
        s_ = (lx - x1) * ux + (ly - y1) * uy
        t_ = (lx - x1) * -uy + (ly - y1) * ux
        return X1 + s_ * Ex + t_ * Px, Y1 + s_ * Ey + t_ * Py
    return f


def courtyard_center(fp):
    layer = pcbnew.B_CrtYd if fp.IsFlipped() else pcbnew.F_CrtYd
    box = fp.GetCourtyard(layer).BBox()
    c = box.Centre()
    return pcbnew.ToMM(c.x), pcbnew.ToMM(c.y)


def place(board, part, nets):
    lib, name = part.footprint.split(":")
    fp = pcbnew.FootprintLoad(os.path.join(design.FP, lib + ".pretty"), name)
    if fp is None:
        raise SystemExit(f"footprint not found: {part.footprint}")
    fp.SetFPID(pcbnew.LIB_ID(lib, name))
    fp.SetReference(part.ref)
    fp.SetValue(part.value)
    fp.SetPath(pcbnew.KIID_PATH("/" + sym_uuid(part.ref)))
    for field, val in (("MPN", part.mpn), ("Note", part.note)):
        if val:
            fp.SetField(field, val)
    for f in fp.GetFields():
        if f.GetName() in ("MPN", "Note"):
            f.SetVisible(False)
    # small parts: reference designators only on the fab drawing, not on the silkscreen
    if part.ref[0] in "RC" or part.ref in ("SW4", "SW5", "J1", "J2"):
        fp.Reference().SetLayer(pcbnew.F_Fab)
    board.Add(fp)
    fp.SetPosition(v(0, 0))
    LIB_LOCAL[part.ref] = [mm(p.GetFPRelativePosition()) for p in fp.Pads()]
    if part.side == "B":
        fp.Flip(fp.GetPosition(), pcbnew.FLIP_DIRECTION_LEFT_RIGHT)

    target = k(*part.pos)

    if part.anchor == "actuator":
        # right-angle switch: actuator must point out of the device's right edge (= KiCad -x)
        for rot in (0, 90, 180, 270):
            fp.SetOrientationDegrees(rot)
            f = affine_from_pads(fp)
            pads = f(2.25, 0.0)
            tip = f(2.25, -3.15)
            dx, dy = tip[0] - pads[0], tip[1] - pads[1]
            if dx < -1 and abs(dy) < 0.1:
                break
        else:
            raise SystemExit("could not orient " + part.ref)
        cur = tip
    elif part.anchor == "card_right":
        # microSD: card enters along the footprint's +y; it must come from the device's right (= KiCad -x)
        for rot in (0, 90, 180, 270):
            fp.SetOrientationDegrees(rot)
            f = affine_from_pads(fp)
            a0, a1 = f(0.0, 0.0), f(0.0, 6.55)
            if a1[0] - a0[0] < -1 and abs(a1[1] - a0[1]) < 0.1:
                break
        cx, cy = courtyard_center(fp)
        front = f(0.0, 6.55)
        cur = (front[0], cy)     # front edge of the socket, centred along the edge
    else:
        fp.SetOrientationDegrees(part.rot)
        if part.anchor == "edge_top":          # slide switch: body front (local y = 2.3) on the edge
            cur = affine_from_pads(fp)(2.0, 2.3)
        elif part.anchor == "pads":            # pin header: centre of the pad row
            pads = [mm(p.GetPosition()) for p in fp.Pads()]
            cur = (sum(p[0] for p in pads) / len(pads), sum(p[1] for p in pads) / len(pads))
        else:                                  # courtyard centre
            cur = courtyard_center(fp)
    fp.Move(v(target[0] - cur[0], target[1] - cur[1]))

    for pad in fp.Pads():
        net = part.pins.get(pad.GetNumber(), "")
        if net:
            pad.SetNet(nets[net])
    return fp


def add_line(board, a, b, layer=pcbnew.Edge_Cuts, width=0.1):
    s = pcbnew.PCB_SHAPE(board)
    s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(v(*a))
    s.SetEnd(v(*b))
    s.SetLayer(layer)
    s.SetWidth(pcbnew.FromMM(width))
    board.Add(s)


def add_arc(board, center, start, angle_deg, layer=pcbnew.Edge_Cuts, width=0.1):
    s = pcbnew.PCB_SHAPE(board)
    s.SetShape(pcbnew.SHAPE_T_ARC)
    s.SetCenter(v(*center))
    s.SetStart(v(*start))
    s.SetArcAngleAndEnd(pcbnew.EDA_ANGLE(angle_deg, pcbnew.DEGREES_T), True)
    s.SetLayer(layer)
    s.SetWidth(pcbnew.FromMM(width))
    board.Add(s)


def rounded_rect_points(x0, y0, x1, y1, r, steps=8):
    pts = []
    corners = [(x1 - r, y0 + r, -90), (x1 - r, y1 - r, 0), (x0 + r, y1 - r, 90), (x0 + r, y0 + r, 180)]
    for cx, cy, a0 in corners:
        for i in range(steps + 1):
            a = math.radians(a0 + 90 * i / steps)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def add_text(board, text, pos, layer, size=1.0, mirror=False, angle=0):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(text)
    t.SetPosition(v(*pos))
    t.SetLayer(layer)
    t.SetTextSize(v(size, size))
    t.SetTextThickness(pcbnew.FromMM(size * 0.15))
    t.SetMirrored(mirror)
    t.SetTextAngleDegrees(angle)
    board.Add(t)


# ---------------------------------------------------------------- build
def main():
    board = pcbnew.CreateEmptyBoard()
    board.SetCopperLayerCount(2)

    parts = design.parts(P)
    # pins the schematic leaves open get KiCad's "unconnected-(...)" nets, so the
    # schematic-parity check matches
    for p in parts:
        _, pins = design.lib_symbol(p.symbol)
        for num, (_x, _y, _a, pname) in pins.items():
            if not p.pins.get(num):
                p.pins[num] = design.unconnected_net(p.ref, num, pname)
    netnames = sorted({n for p in parts for n in p.pins.values() if n})
    nets = {}
    for name in netnames:
        n = pcbnew.NETINFO_ITEM(board, name)
        board.Add(n)
        nets[name] = n

    # net classes and design rules live in carrier.kicad_pro (KiCad keeps them in the project file)

    # outline: case inner outline inset by pcb_inset, rounded corners
    i = P["pcb_inset"]
    r = P["pcb_r"]
    x0, y0 = k(W - i, H - i)   # top-left in KiCad coordinates
    x1, y1 = k(i, i)
    add_line(board, (x0 + r, y0), (x1 - r, y0))
    add_line(board, (x1, y0 + r), (x1, y1 - r))
    add_line(board, (x1 - r, y1), (x0 + r, y1))
    add_line(board, (x0, y1 - r), (x0, y0 + r))
    add_arc(board, (x0 + r, y0 + r), (x0, y0 + r), 90)
    add_arc(board, (x1 - r, y0 + r), (x1 - r, y0), 90)
    add_arc(board, (x1 - r, y1 - r), (x1, y1 - r), 90)
    add_arc(board, (x0 + r, y1 - r), (x0 + r, y1), 90)

    # cable opening for the display connector
    cw, ch = P["conn_cut"]
    cx0 = P["mod_x0"] + (P["mod_w"] - cw) / 2
    a = k(cx0 + cw, P["conn_cut_y"] + ch)
    b = k(cx0, P["conn_cut_y"])
    cr = 1.5
    add_line(board, (a[0] + cr, a[1]), (b[0] - cr, a[1]))
    add_line(board, (b[0], a[1] + cr), (b[0], b[1] - cr))
    add_line(board, (b[0] - cr, b[1]), (a[0] + cr, b[1]))
    add_line(board, (a[0], b[1] - cr), (a[0], a[1] + cr))
    add_arc(board, (a[0] + cr, a[1] + cr), (a[0], a[1] + cr), 90)
    add_arc(board, (b[0] - cr, a[1] + cr), (b[0] - cr, a[1]), 90)
    add_arc(board, (b[0] - cr, b[1] - cr), (b[0], b[1] - cr), 90)
    add_arc(board, (a[0] + cr, b[1] - cr), (a[0] + cr, b[1]), 90)

    fps = {p.ref: place(board, p, nets) for p in parts}

    # keep tracks 0.4 mm away from the board edge and the cable opening
    # (the autorouter does not know KiCad's edge-clearance rule)
    def keepout(pts_outer, pts_hole=None):
        z = pcbnew.ZONE(board)
        z.SetIsRuleArea(True)
        z.SetDoNotAllowTracks(True)
        z.SetDoNotAllowVias(True)
        z.SetDoNotAllowZoneFills(False)
        z.SetDoNotAllowPads(False)
        z.SetDoNotAllowFootprints(False)
        z.SetLayerSet(pcbnew.LSET.AllCuMask())
        poly = z.Outline()
        poly.NewOutline()
        for px, py in pts_outer:
            poly.Append(pcbnew.FromMM(px), pcbnew.FromMM(py))
        if pts_hole:
            poly.NewHole()
            for px, py in pts_hole:
                poly.Append(pcbnew.FromMM(px), pcbnew.FromMM(py))
        board.Add(z)

    m = 0.4
    rect = lambda ax, ay, bx, by: [(ax, ay), (bx, ay), (bx, by), (ax, by)]
    c = r + m   # strips cover the rounded corners too
    keepout(rect(x0 - 1, y0 - 1, x1 + 1, y0 + m))      # top
    keepout(rect(x0 - 1, y1 - m, x1 + 1, y1 + 1))      # bottom
    keepout(rect(x0 - 1, y0 - 1, x0 + m, y1 + 1))      # left
    keepout(rect(x1 - m, y0 - 1, x1 + 1, y1 + 1))      # right
    for cx_, cy_ in ((x0, y0), (x1 - c, y0), (x0, y1 - c), (x1 - c, y1 - c)):   # corner squares
        keepout(rect(cx_ - (1 if cx_ == x0 else 0), cy_ - (1 if cy_ == y0 else 0),
                     cx_ + c + (1 if cx_ != x0 else 0), cy_ + c + (1 if cy_ != y0 else 0)))
    keepout(rounded_rect_points(a[0] - m, a[1] - m, b[0] + m, b[1] + m, cr + m))

    # GND pour on both layers
    outline = rounded_rect_points(x0 + 0.3, y0 + 0.3, x1 - 0.3, y1 - 0.3, max(r - 0.3, 0.5))
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNet(nets["GND"])
        z.SetIsRuleArea(False)
        z.SetLocalClearance(pcbnew.FromMM(0.3))
        z.SetMinThickness(pcbnew.FromMM(0.25))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THT_THERMAL)   # solid for SMD pads
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        z.SetThermalReliefGap(pcbnew.FromMM(0.4))
        z.SetThermalReliefSpokeWidth(pcbnew.FromMM(0.4))
        poly = z.Outline()
        poly.NewOutline()
        for px, py in outline:
            poly.Append(pcbnew.FromMM(px), pcbnew.FromMM(py))
        board.Add(z)

    # silkscreen
    cxk, _ = k(W / 2, 0)
    add_text(board, "e-reader carrier v1", k(55, 47), pcbnew.F_SilkS, 1.5)
    add_text(board, "github.com/ljunker/e-reader", k(55, 44), pcbnew.F_SilkS, 1.0)
    # display header pin names next to J3's pads
    j3 = sorted(fps["J3"].Pads(), key=lambda p: int(p.GetNumber()))
    for pad, label in zip(j3, design.EPD_LABELS):
        px, py = mm(pad.GetPosition())
        add_text(board, label, (px, py + 2.6), pcbnew.F_SilkS, 0.8, angle=90)
    # button names on the display side (B, mirrored text)
    for ref in ("SW1", "SW2", "SW3"):
        cx, cy = courtyard_center(fps[ref])
        add_text(board, fps[ref].GetValue(), (cx, cy - 5.2), pcbnew.B_SilkS, 1.0, mirror=True)
    jx, jy = mm(fps["J2"].GetPosition())
    add_text(board, "+  -", (jx + 1, jy - 2.4), pcbnew.F_SilkS, 0.9)

    pcbnew.SaveBoard(OUT, board)
    print("wrote", OUT, "with", len(fps), "footprints and", len(nets), "nets")


if __name__ == "__main__":
    main()
