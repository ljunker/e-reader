// =====================================================================
//  E-reader case: every dimension in one place.
//  Coordinates (portrait, looking at the BACK of the reader, the way it lies
//  face-down on the bench during assembly):
//    X → right, Y → up (0 = bottom edge), Z → towards you (0 = front face).
//  Seen from the screen side, X therefore points to the LEFT.
//  All values in mm.
//
//  !!! MEASURE YOUR DISPLAY MODULE WITH CALIPERS AND FIX THE "disp_*" VALUES !!!
//  Defaults come from Waveshare's drawing (driver board 103.0 × 78.5,
//  raw panel 90.1 × 77.0 × 1.18, active area 84.8 × 63.6).
// =====================================================================

$fn = 48;

// ---------- printer ----------
tol        = 0.20;   // clearance for parts that slide into each other
hole_tol   = 0.25;   // extra on holes for button caps

// ---------- display module (portrait orientation) ----------
mod_w        = 78.5;  // board width  (landscape height)
mod_h        = 103.0; // board height (landscape width)
mod_pcb_t    = 1.6;
panel_w      = 77.0;
panel_h      = 90.1;
panel_t      = 1.2;
// Panel position on the board, from the board's bottom-left corner.
// On the Waveshare module the extra ~13 mm of board (driver chip, FPC, header)
// is on one short side; we put that side at the TOP of the reader.
panel_off_x  = (mod_w - panel_w) / 2;
panel_off_y  = 0.5;
// Active (visible pixel) area, from the board's bottom-left corner.
active_w     = 63.6;
active_h     = 84.8;
active_off_x = (mod_w - active_w) / 2;
active_off_y = panel_off_y + 2.6;
// Height of the tallest part on the back of the display board (connector
// housing, capacitors). Pin header should be removed or bent flat.
mod_back_clear = 3.0;

window_margin = 0.6;  // extra opening around the active area
window_chamfer = 1.2; // 45° chamfer on the front edge of the window

// ---------- handedness ----------
// true: grip and PREV/NEXT on the LEFT edge (seen from the screen), for holding
// the reader in the left hand. The layout below is written for this version
// (grip at high X = left from the front); false mirrors everything at
// X = case_w / 2 (see handed() in common.scad), only the engraved label is kept
// readable. The firmware needs no change: PREV stays the upper side button,
// NEXT the lower one.
left_handed = true;

// ---------- label engraved on the back ----------
// Any text; "" for none. Also settable at build time: make LABEL="My reader".
// Keep it short enough to fit the back (≈ 90 mm wide at size 7).
label       = "e-reader";
label_size  = 7;      // cap height in mm
label_font  = "Liberation Sans";
label_depth = 0.4;    // engraving depth

// ---------- case ----------
wall         = 2.0;   // side wall
front_t      = 1.6;   // front face thickness
back_t       = 1.6;   // back face thickness
gap          = 0.4;   // gap around the display module
chin         = 22;    // space below the display for the button row
top_margin   = 7;     // space above the display (room for the screw bosses)
spine        = 12;    // extra width on the grip side: side page-turn buttons
case_r       = 6;     // corner radius (plan view)
edge_r       = 1.2;   // rounding of the front edge

case_w = wall + gap + mod_w + gap + spine + wall;
case_h = wall + chin + gap + mod_h + gap + top_margin + wall;
case_d = 17;

split_z = 9;          // bezel is z ∈ [0, split_z], back shell is z ∈ [split_z, case_d]
lip_h   = 2.0;        // alignment lip of the back shell reaching into the bezel
lip_t   = 1.0;

// display module position (bottom-left of the board)
mod_x0 = wall + gap;
mod_y0 = wall + chin + gap;
mod_z0 = front_t + panel_t;          // front of the display board
mod_z1 = mod_z0 + mod_pcb_t;         // back of the display board

// ---------- screws (M2 + heat-set inserts in the bezel) ----------
insert_d    = 3.2;
insert_l    = 4.0;
boss_d      = 6.0;
screw_d     = 2.3;
screw_head_d = 4.2;
screw_head_h = 1.4;
screw_inset = wall + boss_d / 2 - 0.5;
screw_pos = [
    [screw_inset,           screw_inset],
    [case_w - screw_inset,  screw_inset],
    [screw_inset,           case_h - screw_inset],
    [case_w - screw_inset,  case_h - screw_inset],
];

// ---------- buttons ----------
sw_body     = 6.0;   // 6×6 tactile switch
sw_h        = 5.0;   // total height incl. actuator
perf_t      = 1.6;
cap_d       = 7.0;   // visible part of a front button
cap_flange_d = 10.0;
cap_flange_t = 1.0;
cap_travel  = 0.3;   // pre-load gap between cap and switch
btn_pitch   = 16;
btn_y       = wall + chin / 2;
btn_cx      = case_w / 2 + 8;    // towards the grip, leaves room for the Feather
// Left to right seen from the screen: MENU, OK, BACK (X points left from the front).
// The right-handed (mirrored) case swaps them; wire by position, see wiring.md.
front_btn_x = [btn_cx + btn_pitch, btn_cx, btn_cx - btn_pitch];   // MENU, OK, BACK
// front button board (perfboard, switches face forward)
fbb_z0 = front_t + cap_flange_t + cap_travel + sw_h;   // front of the perfboard
fbb_w  = 2 * btn_pitch + 12;
fbb_h  = 14;

// side page-turn buttons on the grip edge at high X (switches face +X)
side_btn_y   = [mod_y0 + 0.62 * mod_h, mod_y0 + 0.38 * mod_h];  // PREV, NEXT
side_btn_z   = 5.5;
side_cap_w   = 5.0;   // oval caps: width (Z) × length (Y)
side_cap_l   = 14.0;
sbb_x1 = case_w - wall - cap_flange_t - cap_travel - sw_h;   // front of the side board
sbb_h  = 0.38 * mod_h + 16;   // board length along Y

// ---------- chassis plate (sits behind the display) ----------
plate_t   = 1.5;
plate_z0  = mod_z1 + mod_back_clear;
plate_z1  = plate_z0 + plate_t;

// ---------- electronics ----------
// Unexpected Maker FeatherS3[D], from UM's STEP model (series_d/3d models):
// 51.3 mm long (52.3 mm with the USB-C receptacle), 2.5 mm holes 2.54 mm in
// at both USB-end corners, one more at the far end on the side of the
// 16-pin header (RST … TX), which faces +x with the components up.
feather_w = 22.86;
feather_l = 51.26;
feather_t = 1.6;
feather_holes = [[2.54, 2.54], [feather_w - 2.54, 2.54], [feather_w - 2.54, feather_l - 2.54]];
feather_standoff_h = 1.5;
feather_x0 = screw_inset + boss_d / 2 + 0.6;   // clear of the bottom-left screw boss
feather_y0 = wall + tol;          // USB-C at the bottom edge
feather_z0 = plate_z1 + feather_standoff_h;   // underside of the Feather PCB
usbc_w = 9.4;  usbc_h = 3.4;      // receptacle on top of the Feather, centred on its short edge
usbc_overhang = 1.1;              // how far the receptacle sticks out past the board edge
usbc_z = feather_z0 + feather_t + usbc_h / 2;

// Adafruit 4682 microSD breakout (measure!). Mounted upside-down on standoffs
// of the back shell, socket facing the front, above the front button board.
sd_w = 25.5;  sd_l = 18.0;  sd_t = 1.6;
sd_standoff_h = 1.0;
sd_x0 = 58;
sd_y0 = wall + tol;                // card slot at the bottom edge
sd_z1 = case_d - back_t - sd_standoff_h;   // back of the SD PCB
sd_z0 = sd_z1 - sd_t;                      // front of the SD PCB (socket side)
sd_slot_w = 14.0;  sd_slot_h = 2.6;
sd_slot_z = sd_z0 - 1.0;

// LiPo
bat_w = 50;  bat_l = 60;  bat_t = 6;
bat_x0 = mod_x0 + (mod_w - bat_w) / 2;
bat_y0 = feather_y0 + feather_l + 2;   // above the Feather

// slide switch SS12D00 on the TOP edge (bottom edge is full: USB-C, microSD)
slide_x  = case_w - wall - spine - 12;
slide_body = [8.8, 4.6, 4.6];   // x, y, z
slide_slot_w = 5.0;  slide_slot_h = 2.4;
slide_z  = case_d - back_t - slide_body[2] / 2 - 0.2;

// ---------- chassis details ----------
// Opening in the plate over the display's connector / driver area (measure!)
conn_cut = [mod_w - 16, 18];      // w, h; placed at the top of the module
// side board vertical extent
sbb_z0 = front_t + 0.2;
sbb_z1 = split_z - 0.2;
