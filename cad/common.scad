include <params.scad>

eps = 0.01;

// Rounded rectangle in XY, extruded from z0 to z1.
module rbox(w, h, z0, z1, r = case_r) {
    translate([0, 0, z0])
        linear_extrude(z1 - z0)
            offset(r = r) offset(delta = -r) square([w, h]);
}

// Case outline inset by d on every side (used for cavities, lips and the PCB).
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

// ---------- stand-ins used by assembly.scad / check.py for collision checks ----------
module display_dummy() {
    color("darkgreen") translate([mod_x0, mod_y0, mod_z0]) cube([mod_w, mod_h, mod_pcb_t]);
    color("whitesmoke") translate([mod_x0 + panel_off_x, mod_y0 + panel_off_y, front_t]) cube([panel_w, panel_h, panel_t]);
    color("gray") translate([mod_x0 + 4, mod_y0 + mod_h - 18, mod_z1]) cube([mod_w - 8, 14, mod_back_clear - 0.3]);
}

// carrier PCB (hardware/kicad/carrier): outline, cable opening and screw holes
module pcb_dummy() {
    color("darkolivegreen") difference() {
        rbox_inset(pcb_inset, pcb_z0, pcb_z1);
        translate([mod_x0 + (mod_w - conn_cut[0]) / 2, conn_cut_y, pcb_z0 - 1]) cube([conn_cut[0], conn_cut[1], pcb_t + 2]);
        for (p = screw_pos) translate([p[0], p[1], pcb_z0 - 1]) cylinder(d = 2.2, h = pcb_t + 2);
    }
}

module feather_dummy() {
    color("royalblue") translate([feather_x0, feather_y0, feather_z0]) cube([feather_w, feather_l, feather_t]);
    color("silver") translate([feather_x0 + feather_w / 2 - usbc_w / 2, feather_y0 - 0.5, feather_z0 + feather_t]) cube([usbc_w, 7.5, usbc_h]);
    // header spacer
    color("black") for (dx = [1.3, 21.6]) translate([feather_x0 + dx - 1.27, feather_y0 + 6.35 - 1.27, pcb_z1]) cube([2.54, 40.6, feather_spacer]);
}

module sd_dummy() {
    color("silver") translate([case_w - pcb_inset - sd_sock[1], sd_cy - sd_sock[0] / 2, pcb_z1]) cube([sd_sock[1], sd_sock[0], sd_sock[2]]);
}

module battery_dummy() {
    color("gold") translate([bat_x0, bat_y0, pcb_z1]) cube([bat_w, bat_l, bat_t]);
}

module buttons_dummy() {
    // front: 6×6 switches on the PCB's front side
    for (x = front_btn_x) color("black")
        translate([x - sw_body / 2, btn_y - sw_body / 2, pcb_z0 - sw_h]) cube([sw_body, sw_body, sw_h]);
    // side: right-angle switches on the PCB's back side, actuator tip at side_act_x
    for (y = side_btn_y) color("black")
        translate([side_act_x - side_body[0], y - side_body[1] / 2, pcb_z1]) cube([side_body[0], side_body[1], side_body[2]]);
}

module electronics() {
    display_dummy();
    pcb_dummy();
    feather_dummy();
    sd_dummy();
    battery_dummy();
    buttons_dummy();
}
