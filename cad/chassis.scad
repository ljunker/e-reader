// Chassis plate: sits behind the display and presses it into the bezel through
// small feet; carries the FeatherS3 and the battery on its back side.
// The back shell's pegs push the plate forward, so it needs no screws.
// Print flat (back side up, as modelled after the final rotate), no supports.
include <common.scad>

foot = 6;            // square feet that rest on the display board's edge
wire_slot = [10, 4];

module plate_outline() {
    // stay inside the back shell's alignment lip
    c = wall + tol + lip_t + tol;
    intersection() {
        translate([c, c]) offset(r = case_r - c) offset(delta = -(case_r - c)) square([case_w - 2 * c, case_h - 2 * c]);
        plate_raw();
    }
}

module plate_raw() {
    union() {
        // over the display module
        translate([mod_x0, mod_y0]) square([mod_w, mod_h]);
        // tongue under the Feather, reaching into the chin
        translate([feather_x0 - 1, feather_y0]) square([feather_w + 1, mod_y0 - feather_y0 + eps]);
    }
}

module plate() {
    difference() {
        translate([0, 0, plate_z0]) linear_extrude(plate_t) plate_outline();
        // clear the screw bosses / tubes
        for (p = screw_pos) translate([p[0], p[1], -1]) cylinder(d = boss_d + 1, h = case_d + 2);
        // opening over the display connector and driver chip
        translate([mod_x0 + (mod_w - conn_cut[0]) / 2, mod_y0 + mod_h - conn_cut[1] - 2, -1])
            cube([conn_cut[0], conn_cut[1], case_d]);
        // wire pass-throughs: buttons → Feather, side board → Feather
        translate([feather_x0 + feather_w + 2, mod_y0 + 2, -1]) cube([wire_slot[0], wire_slot[1], case_d]);
        translate([mod_x0 + mod_w - wire_slot[1] - 2, side_btn_y[1] - 4, -1]) cube([wire_slot[1], wire_slot[0], case_d]);
        // weight / material saving windows under the battery
        for (dx = [0, 1]) for (dy = [0, 1])
            translate([bat_x0 + 5 + dx * (bat_w / 2), bat_y0 + 5 + dy * (bat_l / 2), -1])
                cube([bat_w / 2 - 10, bat_l / 2 - 10, case_d]);
    }
}

// Feet pressing on the display board (near its edges, away from components).
module feet() {
    for (p = [[0.08, 0.06], [0.92, 0.06], [0.08, 0.7], [0.92, 0.7], [0.5, 0.06]])
        translate([mod_x0 + p[0] * mod_w - foot / 2, mod_y0 + p[1] * mod_h - foot / 2, mod_z1 + tol])
            cube([foot, foot, plate_z0 - mod_z1 - tol + eps]);
    // the tongue gets a foot that rests on the bezel's front face
    translate([feather_x0 + feather_w / 2 - foot / 2, feather_y0 + 3, front_t + tol])
        cube([foot, foot, plate_z0 - front_t - tol + eps]);
}

module feather_standoffs() {
    for (h = feather_holes) translate([feather_x0 + h[0], feather_y0 + h[1], plate_z1 - eps])
        difference() {
            cylinder(d = 4.5, h = feather_standoff_h + eps);
            translate([0, 0, -plate_t - 1]) cylinder(d = 1.8, h = 10);   // M2 self-tapping
        }
}

module battery_pocket() {
    t = 1.0; h = bat_t / 2;
    translate([bat_x0 - t - tol, bat_y0 - t - tol, plate_z1 - eps])
        difference() {
            cube([bat_w + 2 * (t + tol), bat_l + 2 * (t + tol), h]);
            translate([t, t, -1]) cube([bat_w + 2 * tol, bat_l + 2 * tol, h + 2]);
            // opening for the battery leads (towards the Feather)
            translate([feather_x0 + feather_w / 2 - bat_x0 - 6, -1, -1]) cube([12, t + 2, h + 2]);
        }
}

module chassis() {
    difference() {
        union() { plate(); feet(); feather_standoffs(); battery_pocket(); }
        for (h = feather_holes) translate([feather_x0 + h[0], feather_y0 + h[1], 0]) cylinder(d = 1.8, h = case_d);
    }
}

// In assembly coordinates; flip for printing in the Makefile (-D print=true).
print = false;
if (print) translate([0, 0, plate_z1]) mirror([0, 0, 1]) chassis();
else chassis();
