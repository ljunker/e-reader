// =====================================================================
//  E-reader case: every dimension in one place.
//  Coordinates (portrait, looking at the screen):
//    X → right, Y → up (0 = bottom edge), Z → into the device (0 = front face).
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

// ---------- case ----------
wall         = 2.0;   // side wall
front_t      = 1.6;   // front face thickness
back_t       = 1.6;   // back face thickness
gap          = 0.4;   // gap around the display module
chin         = 22;    // space below the display for the button row
top_margin   = 7;     // space above the display (room for the screw bosses)
spine        = 12;    // extra width on the right: grip + side page-turn buttons
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
btn_cx      = case_w / 2 + 8;    // right of centre, leaves room for the Feather on the left
front_btn_x = [btn_cx - btn_pitch, btn_cx, btn_cx + btn_pitch];   // MENU, OK, BACK
// front button board (perfboard, switches face forward)
fbb_z0 = front_t + cap_flange_t + cap_travel + sw_h;   // front of the perfboard
fbb_w  = 2 * btn_pitch + 12;
fbb_h  = 14;

// side page-turn buttons on the right edge (switches face +X)
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
// Unexpected Maker FeatherS3 (Feather form factor)
feather_w = 22.9;
feather_l = 50.8;
feather_t = 1.6;
feather_holes = [[2.54, 2.54], [22.9 - 2.54, 2.54], [2.54, 50.8 - 2.54], [22.9 - 2.54, 50.8 - 2.54]];
feather_standoff_h = 1.5;
feather_x0 = screw_inset + boss_d / 2 + 0.6;   // clear of the bottom-left screw boss
feather_y0 = wall + tol;          // USB-C at the bottom edge
feather_z0 = plate_z1 + feather_standoff_h;   // underside of the Feather PCB
usbc_w = 9.4;  usbc_h = 3.4;      // receptacle on top of the Feather, centred on its short edge
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
