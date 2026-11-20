"""Carrier board for the e-reader: single source of truth for parts, nets and
placement. generate_pcb.py and generate_sch.py both read this file.

Positions are in case coordinates (mm, looking at the device from the FRONT,
x right, y up, origin bottom-left) and come from cad/params.scad.
PCB layer F (top) faces the BACK of the device; layer B faces the display.
"""
from __future__ import annotations

import ast
import os
import re
import shutil
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
OPENSCAD = shutil.which("openscad") or "/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD"


def case_params() -> dict:
    """Run OpenSCAD on cad/export_params.scad and parse the echoed values."""
    out = os.path.join(HERE, ".params.echo")
    subprocess.run([OPENSCAD, "-o", out, os.path.join(ROOT, "cad", "export_params.scad")],
                   check=True, capture_output=True)
    text = open(out).read()
    os.remove(out)
    m = re.search(r"ECHO: P = (\[.*\])", text)
    return dict(ast.literal_eval(m.group(1)))


# FeatherS3 pads in KiCad's Module:Adafruit_Feather footprint (generic Feather
# names -> FeatherS3 GPIO). Pad 1 (RST) is at the USB end.
#  1 RST   2 3V3   3 (AREF, unused)  4 GND   5 IO17  6 IO18  7 IO14  8 IO12
#  9 IO6  10 IO5  11 IO36 SCK  12 IO35 MOSI  13 IO37 MISO  14 IO44 RX  15 IO43 TX
# 16 (spare, unused)  17 IO8 SDA  18 IO9 SCL  19 IO33  20 IO38  21 IO1  22 IO3
# 23 IO7  24 IO10  25 IO11  26 VBUS  27 EN  28 VBAT
FEATHER = {
    2: "+3V3", 4: "GND", 28: "VBAT",
    # display
    12: "EPD_DIN", 11: "EPD_CLK", 20: "EPD_CS", 19: "EPD_DC", 15: "EPD_RST", 14: "EPD_BUSY", 6: "EPD_PWR",
    # microSD (SDMMC 4-bit)
    8: "SD_CLK", 25: "SD_CMD", 24: "SD_D0", 18: "SD_D1", 5: "SD_D2", 17: "SD_D3", 22: "SD_DET",
    # buttons
    7: "BTN_PREV", 21: "BTN_NEXT", 10: "BTN_MENU", 9: "BTN_OK", 23: "BTN_BACK",
}
FEATHER_GPIO = {5: 17, 6: 18, 7: 14, 8: 12, 9: 6, 10: 5, 11: 36, 12: 35, 13: 37, 14: 44, 15: 43, 17: 8, 18: 9,
                19: 33, 20: 38, 21: 1, 22: 3, 23: 7, 24: 10, 25: 11}

# Waveshare 4.2" V2 Rev2.2 cable order
EPD_HEADER = ["+3V3", "GND", "EPD_DIN", "EPD_CLK", "EPD_CS", "EPD_DC", "EPD_RST", "EPD_BUSY", "EPD_PWR"]
EPD_LABELS = ["VCC", "GND", "DIN", "CLK", "CS", "DC", "RST", "BUSY", "PWR"]

FP = "/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints"
SYMLIB = "/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols"


def lib_symbol(lib_id):
    """Return (s-expression block renamed to lib_id, {pin number: (x, y, angle, name)})."""
    lib, name = lib_id.split(":")
    s = open(os.path.join(SYMLIB, lib + ".kicad_sym")).read()
    i = s.index(f'(symbol "{name}"')
    depth = 0
    for j in range(i, len(s)):
        if s[j] == "(":
            depth += 1
        elif s[j] == ")":
            depth -= 1
            if depth == 0:
                block = s[i:j + 1]
                break
    block = block.replace(f'(symbol "{name}"', f'(symbol "{lib_id}"', 1)
    pins = {}
    for m in re.finditer(r'\(pin \w+ \w+\s*\(at ([-\d.]+) ([-\d.]+) (\d+)\).*?\(name "([^"]*)".*?\(number "([^"]*)"',
                         block, re.S):
        pins[m.group(5)] = (float(m.group(1)), float(m.group(2)), int(m.group(3)), m.group(4))
    return block, pins


def unconnected_net(ref, pin_number, pin_name):
    """The net name KiCad gives an unconnected symbol pin."""
    if pin_name and pin_name != "~":
        return f"unconnected-({ref}-{pin_name}-Pad{pin_number})"
    return f"unconnected-({ref}-Pad{pin_number})"


class Part:
    def __init__(self, ref, value, footprint, symbol, pins, pos=None, rot=0, side="F", anchor="center",
                 mpn="", note=""):
        self.ref, self.value, self.footprint, self.symbol = ref, value, footprint, symbol
        self.pins = pins            # pad number (str) -> net
        self.pos, self.rot, self.side, self.anchor = pos, rot, side, anchor
        self.mpn, self.note = mpn, note


def parts(P: dict) -> list[Part]:
    pp = []
    fx = P["feather_x0"] + P["feather_w"] / 2
    fy = P["feather_y0"] + P["feather_l"] / 2
    pp.append(Part("U1", "FeatherS3", "Module:Adafruit_Feather", "MCU_Module:Adafruit_Feather_Generic",
                   {str(k): v for k, v in FEATHER.items()}, (fx, fy), 180, "F", "board",
                   mpn="Unexpected Maker FeatherS3 + 2× pin header (16 + 12)",
                   note="soldered on plain pin headers, plastic spacer 2.5 mm"))

    # microSD at the right edge, card inserted through the right side wall
    right = P["case_w"] - P["pcb_inset"]
    pp.append(Part("J1", "microSD", "Connector_Card:microSD_HC_Molex_104031-0811", "Connector:Micro_SD_Card_Det2",
                   {"1": "SD_D2", "2": "SD_D3", "3": "SD_CMD", "4": "+3V3", "5": "SD_CLK", "6": "GND",
                    "7": "SD_D0", "8": "SD_D1", "9": "GND", "10": "SD_DET", "SH": "GND"},
                   (right, P["sd_cy"]), None, "F", "card_right", mpn="Molex 104031-0811"))
    # SD pull-ups and decoupling in a column left of the socket
    col_x = right - 13.1 - 2.5
    for i, net in enumerate(["SD_CMD", "SD_D0", "SD_D1", "SD_D2", "SD_D3"]):
        pp.append(Part(f"R{i + 1}", "10k", "Resistor_SMD:R_0805_2012Metric", "Device:R",
                       {"1": "+3V3", "2": net}, (col_x, P["sd_cy"] + 8.4 - 2.8 * i), 0, "F",
                       mpn="0805 10 kΩ 1 %"))
    pp.append(Part("C1", "10u", "Capacitor_SMD:C_0805_2012Metric", "Device:C", {"1": "+3V3", "2": "GND"},
                   (col_x, P["sd_cy"] + 8.4 - 2.8 * 5), 0, "F", mpn="0805 10 µF 10 V X5R"))
    pp.append(Part("C2", "100n", "Capacitor_SMD:C_0805_2012Metric", "Device:C", {"1": "+3V3", "2": "GND"},
                   (col_x, P["sd_cy"] + 8.4 - 2.8 * 6), 0, "F", mpn="0805 100 nF 50 V X7R"))

    # front buttons (on B, facing the display side / front of the device)
    for ref, x, net in zip(["SW1", "SW2", "SW3"], P["front_btn_x"], ["BTN_MENU", "BTN_OK", "BTN_BACK"]):
        pp.append(Part(ref, net[4:], "Button_Switch_THT:SW_PUSH_6mm_H4.3mm", "Switch:SW_Push",
                       {"1": net, "2": "GND"}, (x, P["btn_y"]), 0, "B", mpn="6×6 mm tactile, 4.3 mm (e.g. PHAP3301)"))
    # side page-turn buttons at the right edge on the back side (more room there),
    # actuators pointing out of the right side
    for ref, y, net in zip(["SW4", "SW5"], P["side_btn_y"], ["BTN_PREV", "BTN_NEXT"]):
        pp.append(Part(ref, net[4:], "Button_Switch_THT:SW_Tactile_SPST_Angled_PTS645Vx31-2LFS", "Switch:SW_Push",
                       {"1": net, "2": "GND"}, (P["side_act_x"], y), None, "F", "actuator",
                       mpn="C&K PTS645VL31-2 LFS (right angle)"))
    # power switch in the battery line, lever out of the top edge
    top = P["case_h"] - P["pcb_inset"]
    pp.append(Part("SW6", "POWER", "Button_Switch_THT:SW_Slide_SPDT_Angled_CK_OS102011MA1Q", "Switch:SW_SPDT",
                   {"1": "VBAT", "2": "BAT_IN", "3": ""}, (P["slide_x"], top), 180, "F", "edge_top",
                   mpn="C&K OS102011MA1QN1"))
    # battery connector, plug from below (towards the battery)
    pp.append(Part("J2", "BATTERY", "Connector_JST:JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal",
                   "Connector_Generic:Conn_01x02", {"1": "BAT_IN", "2": "GND"},
                   (P["bat_x0"] + P["bat_w"] - 6, P["bat_y0"] + P["bat_l"] + 3.5), 0, "F",
                   mpn="JST S2B-PH-K-S"))
    # display cable header, pins pointing into the cable opening
    pp.append(Part("J3", "EPD", "Connector_PinHeader_2.54mm:PinHeader_1x09_P2.54mm_Horizontal",
                   "Connector_Generic:Conn_01x09", {str(i + 1): n for i, n in enumerate(EPD_HEADER)},
                   (P["mod_x0"] + P["mod_w"] / 2 - 4, P["conn_cut_y"] - 3.2), 90, "F", "pads",
                   mpn="1×9 right-angle pin header 2.54 mm (or solder the cable directly)"))
    for i, (x, y) in enumerate(P["screw_pos"]):
        pp.append(Part(f"H{i + 1}", "M2", "MountingHole:MountingHole_2.2mm_M2", "Mechanical:MountingHole", {},
                       (x, y), 0, "F"))
    return pp
