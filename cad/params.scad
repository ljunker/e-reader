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

lip_h   = 2.0;        // alignment lip of the back shell reaching into the bezel
lip_t   = 1.0;

// display module position (bottom-left of the board)
mod_x0 = wall + gap;
mod_y0 = wall + chin + gap;
mod_z0 = front_t + panel_t;          // front of the display board
mod_z1 = mod_z0 + mod_pcb_t;         // back of the display board

// ---------- carrier PCB (hardware/kicad/carrier) ----------
// Sits behind the display, clamped between the bezel's screw bosses and the
// back shell's screw tubes. Front side (towards the display): the 5 buttons.
// Back side: FeatherS3, microSD socket, battery connector, power switch,
// display header.
pcb_t     = 1.6;
pcb_z0    = mod_z1 + mod_back_clear;   // front face of the PCB
pcb_z1    = pcb_z0 + pcb_t;            // back face
split_z   = pcb_z1;                    // bezel is z ∈ [0, split_z], back shell above
pcb_inset = wall + tol + lip_t + tol;  // PCB edge stays inside the back shell's lip
pcb_r     = case_r - pcb_inset;
// Opening for the display cable (the module's connector is at its top, on the back).
conn_cut  = [mod_w - 16, 12];          // w, h
conn_cut_y = mod_y0 + mod_h - 2 - 12;  // bottom edge of the opening

// ---------- screws (M2 + heat-set inserts in the bezel) ----------
insert_d    = 3.2;
insert_l    = 4.0;
boss_d      = 5.6;
screw_d     = 2.3;
screw_head_d = 4.2;
screw_head_h = 1.4;
screw_inset = 6.0;   // PCB edge is at pcb_inset (3.4): leaves 1.5 mm of board around each M2 hole
screw_pos = [
    [screw_inset,           screw_inset],
    [case_w - screw_inset,  screw_inset],
    [screw_inset,           case_h - screw_inset],
    [case_w - screw_inset,  case_h - screw_inset],
];

// ---------- buttons (soldered to the PCB's front side) ----------
sw_body     = 6.0;   // 6×6 tactile switch
sw_h        = 4.3;   // front switches: 6×6 mm, 4.3 mm tall (e.g. PHAP3301)
cap_d       = 7.0;   // visible part of a front button
cap_flange_d = 10.0;
cap_flange_t = 1.0;
cap_travel  = 0.2;   // free play before the cap touches the switch
protrude    = 0.8;   // how far a cap stands out of the case
btn_pitch   = 16;
btn_y       = wall + chin / 2;
btn_cx      = case_w / 2 + 6;    // right of centre, leaves room for the Feather on the left
front_btn_x = [btn_cx - btn_pitch, btn_cx, btn_cx + btn_pitch];   // MENU, OK, BACK
// gap between the back of a cap's flange and the switch top -> length of its nub
front_nub   = pcb_z0 - sw_h - front_t - cap_flange_t - cap_travel;

// side page-turn buttons: right-angle 6×6 switches (C&K PTS645VL31) on the PCB's BACK
// side at its right edge (the display side has only ~5.8 mm, too little for them)
side_btn_y   = [mod_y0 + 0.62 * mod_h, mod_y0 + 0.38 * mod_h];  // PREV, NEXT
side_act_h   = 3.5;   // actuator axis above the PCB (check the switch datasheet)
side_body    = [7.0, 6.2, 6.6];   // x (incl. actuator), y, z: switch envelope
side_btn_z   = pcb_z1 + side_act_h;
side_cap_w   = 4.0;   // oval caps: width (Z) × length (Y)
side_cap_l   = 14.0;
side_act_x   = case_w - wall - cap_flange_t - cap_travel;   // where the actuator tip must end
side_nub     = 0.4;

// ---------- electronics (positions in case coordinates) ----------
// Unexpected Maker FeatherS3 on the PCB's back side, on a plain pin header
// (plastic spacer feather_spacer thick), USB-C at the bottom edge.
feather_w = 22.9;
feather_l = 50.8;
feather_t = 1.6;
feather_spacer = 2.5;
feather_x0 = screw_inset + boss_d / 2 + 0.6;   // clear of the bottom-left screw boss
feather_y0 = wall + tol;
feather_z0 = pcb_z1 + feather_spacer;   // underside of the Feather PCB
usbc_w = 9.4;  usbc_h = 3.4;            // receptacle on top of the Feather, centred on its short edge
usbc_z = feather_z0 + feather_t + usbc_h / 2;

case_d = ceil((usbc_z + usbc_h / 2 + 0.3 + back_t) * 2) / 2;   // 18.5 with the defaults

// microSD socket (Molex 104031-0811) on the back side at the RIGHT edge, below the
// page-turn buttons; the card goes in through the right side wall
sd_cy     = 38;                 // centre of the socket along Y
sd_sock   = [13.7, 13.1, 1.45]; // w (along Y), l (along X), h
sd_slot_w = 13.0;  sd_slot_h = 1.8;
sd_slot_z = pcb_z1 + 0.75;

// LiPo 6 × 50 × 60 mm lying crosswise above the Feather
bat_w = 60;  bat_l = 50;  bat_t = 6;
bat_x0 = mod_x0 + (mod_w - bat_w) / 2 + 4;
bat_y0 = feather_y0 + feather_l + 2;

// right-angle slide switch (C&K OS102011MA1Q) on the top edge
slide_x  = case_w - wall - spine - 12;
slide_slot_w = 5.0;  slide_slot_h = 2.8;
slide_z  = pcb_z1 + 2.0;
