"""Autoroutes carrier.kicad_pcb with Freerouting and refills the GND pours.
Run with KiCad's Python:

    $KICAD_PY route.py /path/to/freerouting-2.5.0.jar
"""
import math
import os
import subprocess
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
PCB = os.path.join(HERE, "carrier.kicad_pcb")
DSN = os.path.join(HERE, "carrier.dsn")
SES = os.path.join(HERE, "carrier.ses")


def add_stitching_vias(board, pitch=7.0, drill=0.4, dia=0.8):
    """Tie the two GND pours together with vias on a grid wherever there is room."""
    gnd = board.FindNet("GND")
    keep = pcbnew.FromMM(dia / 2 + 0.35)
    edge = board.GetBoardEdgesBoundingBox()
    outline = pcbnew.SHAPE_POLY_SET()
    board.GetBoardPolygonOutlines(outline, False)
    obstacles = list(board.GetTracks()) + [p for fp in board.GetFootprints() for p in fp.Pads()]
    count = 0
    x = edge.GetX() + pcbnew.FromMM(2)
    while x < edge.GetRight():
        y = edge.GetY() + pcbnew.FromMM(2)
        while y < edge.GetBottom():
            pos = pcbnew.VECTOR2I(int(x), int(y))
            inside = outline.Contains(pos) and not any(
                outline.Contains(pcbnew.VECTOR2I(pos.x + dx, pos.y + dy)) is False
                for dx, dy in ((keep * 2, 0), (-keep * 2, 0), (0, keep * 2), (0, -keep * 2)))
            in_rule_area = any(z.GetIsRuleArea() and z.Outline().Contains(pos) for z in board.Zones())
            if inside and not in_rule_area and not any(o.HitTest(pos, keep) for o in obstacles):
                # stay away from footprint bodies so nothing sits under a part
                if not any(fp.GetCourtyard(pcbnew.F_CrtYd if not fp.IsFlipped() else pcbnew.B_CrtYd)
                           .Contains(pos) for fp in board.GetFootprints()):
                    via = pcbnew.PCB_VIA(board)
                    via.SetPosition(pos)
                    via.SetDrill(pcbnew.FromMM(drill))
                    via.SetWidth(pcbnew.FromMM(dia))
                    via.SetNet(gnd)
                    board.Add(via)
                    obstacles.append(via)
                    count += 1
            y += pcbnew.FromMM(pitch)
        x += pcbnew.FromMM(pitch)
    return count


def ring(p, r, n=8):
    return [pcbnew.VECTOR2I(int(p.x + r * math.cos(2 * math.pi * i / n)), int(p.y + r * math.sin(2 * math.pi * i / n)))
            for i in range(n)]


def gnd_reachable(board):
    """UUIDs of everything KiCad sees connected to the Feather's GND pin."""
    board.BuildConnectivity()
    c = board.GetConnectivity()
    u1 = board.FindFootprintByReference("U1")
    anchor = [p for p in u1.Pads() if p.GetNumber() == "4"][0]
    return {i.m_Uuid.AsString() for i in c.GetConnectedItems(anchor)} | {anchor.m_Uuid.AsString()}


def fragments(board, net):
    out = []
    for z in board.Zones():
        if z.GetIsRuleArea() or z.GetNetCode() != net:
            continue
        for L in z.GetLayerSet().CuStack():
            f = z.GetFilledPolysList(L)
            out += [(L, f.Outline(i)) for i in range(f.OutlineCount())]
    return out


def connect_islands(board, dia=0.6, drill=0.3):
    """Use KiCad's connectivity to find GND pads and vias cut off from the main
    ground. Stray vias are removed; cut-off pads get a via into the main pour
    on the other layer."""
    gnd = board.FindNet("GND").GetNetCode()
    added = 0
    for _ in range(5):
        if kicad_unconnected(board) == 0:
            return added
        ok = gnd_reachable(board)
        vias = [t for t in board.GetTracks() if t.GetClass() == "PCB_VIA" and t.GetNetCode() == gnd]
        stray = [v for v in vias if v.m_Uuid.AsString() not in ok]
        for v in stray:
            board.Remove(v)
        pads = [p for fp in board.GetFootprints() for p in fp.Pads()
                if p.GetNetCode() == gnd and p.m_Uuid.AsString() not in ok]
        frags = fragments(board, gnd)
        good_vias = [v.GetPosition() for v in vias if v.m_Uuid.AsString() in ok]
        main = [(L, o) for L, o in frags if any(o.PointInside(q) for q in good_vias)]
        others = [t for t in board.GetTracks() if t.GetNetCode() != gnd]
        others += [p for fp in board.GetFootprints() for p in fp.Pads() if p.GetNetCode() != gnd]
        r = pcbnew.FromMM(dia / 2 + 0.3)
        new = 0
        for pad in pads:
            layer = pcbnew.F_Cu if pad.IsOnLayer(pcbnew.F_Cu) else pcbnew.B_Cu
            other = pcbnew.B_Cu if layer == pcbnew.F_Cu else pcbnew.F_Cu
            size = pad.GetSize(layer)
            probes = [pad.GetPosition()] + ring(pad.GetPosition(), max(size.x, size.y) // 2 + pcbnew.FromMM(0.6))
            own = [o for L, o in frags if L == layer and any(o.PointInside(q) for q in probes)]
            targets = [o for L, o in main if L == other]
            spot = None
            for o in own:
                bb = o.BBox()
                step = pcbnew.FromMM(0.2)
                y = bb.GetY()
                while y < bb.GetBottom() and spot is None:
                    x = bb.GetX()
                    while x < bb.GetRight() and spot is None:
                        pos = pcbnew.VECTOR2I(int(x), int(y))
                        pr = [pos] + ring(pos, r)
                        if all(o.PointInside(q) for q in pr) and any(all(t.PointInside(q) for q in pr) for t in targets) \
                                and not any(it.HitTest(pos, r) for it in others):
                            spot = pos
                        x += step
                    y += step
            if spot is not None:
                via = pcbnew.PCB_VIA(board)
                via.SetPosition(spot)
                via.SetDrill(pcbnew.FromMM(drill))
                via.SetWidth(pcbnew.FromMM(dia))
                via.SetNet(board.FindNet("GND"))
                board.Add(via)
                new += 1
        if not new and not stray:
            break
        added += new
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    left = kicad_unconnected(board)
    if left:
        print(f"warning: {left} connection(s) still open after bridging; check DRC")
    return added


def kicad_unconnected(board):
    board.BuildConnectivity()
    c = board.GetConnectivity()
    c.RecalculateRatsnest()
    return c.GetUnconnectedCount(False)


def route_once(jar):
    board = pcbnew.LoadBoard(PCB)
    # Export without the GND pours: otherwise Freerouting treats GND as a plane
    # and leaves it unrouted. GND gets real tracks; the pours are added on top.
    pours = [z for z in board.Zones() if not z.GetIsRuleArea()]
    for z in pours:
        board.Remove(z)
    if not pcbnew.ExportSpecctraDSN(board, DSN):
        raise SystemExit("DSN export failed")
    for z in pours:          # give them back before the board object goes away
        board.Add(z)
    del pours, board
    subprocess.run(["java", "-jar", jar, "-de", DSN, "-do", SES, "-mp", "40",
                    "--gui.enabled=false"], check=True, capture_output=True)
    board = pcbnew.LoadBoard(PCB)
    if not pcbnew.ImportSpecctraSES(board, SES):
        raise SystemExit("SES import failed")
    # Freerouting sometimes necks tracks down below what the fab can make
    min_w = pcbnew.FromMM(0.15)
    for t in board.GetTracks():
        if t.GetClass() == "PCB_TRACK" and t.GetWidth() < min_w:
            t.SetWidth(min_w)
    n = add_stitching_vias(board)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    n += connect_islands(board)
    return board, n


def main(attempts=5):
    """Freerouting's result varies from run to run; keep the first fully connected one."""
    jar = sys.argv[1]
    for attempt in range(1, attempts + 1):
        board, n = route_once(jar)
        left = kicad_unconnected(board)
        print(f"attempt {attempt}: {n} GND vias, {left} open connection(s)")
        if left == 0:
            break
    out = PCB
    pcbnew.SaveBoard(out, board)
    for f in (DSN, SES):
        if os.path.exists(f):
            os.remove(f)
    if left:
        raise SystemExit(f"still {left} open connection(s) after {attempts} attempts")
    print("routed", out)


if __name__ == "__main__":
    main()
