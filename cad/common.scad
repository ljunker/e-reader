include <params.scad>

eps = 0.01;

// Mirrors its children for the left-handed case (see left_handed in params.scad).
// Every printed part and every stand-in goes through this, so the parts stay
// consistent with each other and with the collision check.
module handed() {
    if (left_handed) translate([case_w, 0, 0]) mirror([1, 0, 0]) children();
    else children();
}

// X coordinate after handed(), for things placed outside of it.
function hx(x) = left_handed ? case_w - x : x;

// Rounded rectangle in XY, extruded from z0 to z1.
module rbox(w, h, z0, z1, r = case_r) {
    translate([0, 0, z0])
        linear_extrude(z1 - z0)
            offset(r = r) offset(delta = -r) square([w, h]);
}

// Case outline inset by d on every side (used for cavities and lips).
module rbox_inset(d, z0, z1, r = case_r) {
    translate([d, d, 0]) rbox(case_w - 2 * d, case_h - 2 * d, z0, z1, max(r - d, 0.5));
}

// Outer body with a rounded front or back edge.
module case_body(z0, z1, round_front = true) {
    if (round_front) hull() {
        rbox_inset(edge_r, z0, z0 + eps);
        rbox(case_w, case_h, z0 + edge_r, z1);
    } else hull() {
        rbox(case_w, case_h, z0, z1 - edge_r);
        rbox_inset(edge_r, z1 - eps, z1);
    }
}

// Rounded-end slot, length along Y, extruded along Z (rotate as needed).
module oval(l, w, depth) {
    hull() for (dy = [-(l - w) / 2, (l - w) / 2])
        translate([0, dy, 0]) cylinder(d = w, h = depth);
}

// ---------- stand-ins used by assembly.scad for collision checks ----------
module display_dummy() {
    handed() {
        color("darkgreen") translate([mod_x0, mod_y0, mod_z0]) cube([mod_w, mod_h, mod_pcb_t]);
        color("whitesmoke") translate([mod_x0 + panel_off_x, mod_y0 + panel_off_y, front_t]) cube([panel_w, panel_h, panel_t]);
        color("gray") translate([mod_x0 + 4, mod_y0 + mod_h - 18, mod_z1]) cube([mod_w - 8, 14, mod_back_clear]);
    }
}

module feather_dummy() {
    handed() {
        color("royalblue") translate([feather_x0, feather_y0, feather_z0]) cube([feather_w, feather_l, feather_t]);
        color("silver") translate([feather_x0 + feather_w / 2 - usbc_w / 2, feather_y0 - 0.5, feather_z0 + feather_t]) cube([usbc_w, 7.5, usbc_h]);
    }
}

module sd_dummy() {
    handed() {
        color("purple") translate([sd_x0, sd_y0, sd_z0]) cube([sd_w, sd_l, sd_t]);
        color("silver") translate([sd_x0 + sd_w / 2 - 7, sd_y0, sd_z0 - 2]) cube([14, 15, 2]);
    }
}

module battery_dummy() {
    handed() {
        color("gold") translate([bat_x0, bat_y0, plate_z1]) cube([bat_w, bat_l, bat_t]);
    }
}

module buttons_dummy() {
    handed() {
        color("tan") translate([btn_cx - fbb_w / 2, btn_y - fbb_h / 2, fbb_z0]) cube([fbb_w, fbb_h, perf_t]);
        for (x = front_btn_x) color("black")
            translate([x - sw_body / 2, btn_y - sw_body / 2, fbb_z0 - sw_h]) cube([sw_body, sw_body, sw_h]);
        color("tan") translate([sbb_x1 - perf_t, side_btn_y[1] - 8, sbb_z0]) cube([perf_t, sbb_h, sbb_z1 - sbb_z0]);
        for (y = side_btn_y) color("black")
            translate([sbb_x1, y - sw_body / 2, side_btn_z - sw_body / 2]) cube([sw_h, sw_body, sw_body]);
    }
}
