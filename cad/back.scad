// Back shell: z ∈ [split_z, case_d] plus an alignment lip reaching into the bezel.
// Cut-outs: USB-C and microSD on the bottom edge, power switch on the top edge.
// Print outside face down (the Makefile flips it), no supports needed.
include <common.scad>

module port_cut(x, z, w, h, depth = wall + 2) {
    // rounded slot through the bottom wall (y = 0)
    translate([x, -1, z]) rotate([-90, 0, 0])
        linear_extrude(depth) offset(r = h / 2 - 0.01) offset(delta = -(h / 2 - 0.01))
            square([w, h], center = true);
}

module lip() {
    difference() {
        rbox_inset(wall + tol, split_z - lip_h, split_z + eps);
        rbox_inset(wall + tol + lip_t, split_z - lip_h - 1, split_z + 1);
        for (p = screw_pos) translate([p[0], p[1], 0]) cylinder(d = boss_d + 1.5, h = case_d);
        // gap for the Feather's corner standoffs at the bottom edge
        translate([feather_x0 - 1, -1, 0]) cube([feather_w + 2, wall + lip_t + 2, case_d]);
    }
}

module screw_tubes() {
    for (p = screw_pos) translate([p[0], p[1], split_z]) cylinder(d = boss_d, h = case_d - split_z - eps);
}

module screw_holes() {
    for (p = screw_pos) {
        translate([p[0], p[1], 0]) cylinder(d = screw_d, h = case_d + 1);
        // counterbore for the head, from the back
        translate([p[0], p[1], case_d - screw_head_h - 0.8]) cylinder(d = screw_head_d + 0.4, h = 5);
    }
}

// Pegs that press the chassis plate (and with it the display) forward.
peg_pos = [[mod_x0 + 12, mod_y0 + mod_h - 6], [mod_x0 + mod_w - 12, mod_y0 + mod_h - 6],
           [mod_x0 + mod_w - 14, bat_y0 - 10], [feather_x0 + feather_w + 8, bat_y0 - 10]];
module pegs() {
    for (p = peg_pos) translate([p[0], p[1], plate_z1 + tol])
        cylinder(d = 5, h = case_d - back_t - plate_z1 - tol + eps);
}

module sd_mount() {
    // four short posts; fix the breakout with double-sided tape or M2 screws
    for (dx = [1.5, sd_w - 1.5]) for (dy = [sd_l - 3])
        translate([sd_x0 + dx - 1.5, sd_y0 + dy, sd_z1 - eps]) cube([3, 3, sd_standoff_h + 2 * eps]);
    // back stop so the board cannot move when a card is pushed in
    translate([sd_x0 + 2, sd_y0 + sd_l + tol, sd_z0 - 1]) cube([sd_w - 4, 1.2, case_d - back_t - sd_z0 + 1]);
}

module switch_holder() {
    b = slide_body;
    y1 = case_h - wall;            // inner face of the top wall
    translate([slide_x - b[0] / 2 - 1.2, y1 - b[1] - tol - 1.2, slide_z - b[2] / 2])
        difference() {
            cube([b[0] + 2.4 + 2 * tol, b[1] + 1.2 + tol, case_d - back_t - slide_z + b[2] / 2 + eps]);
            translate([1.2, 1.2, -1]) cube([b[0] + 2 * tol, b[1] + tol + 1, b[2] + 1]);
        }
}

module back_shell() {
    difference() {
        handed() union() {
            difference() {
                case_body(split_z, case_d, round_front = false);
                rbox_inset(wall, split_z - eps, case_d - back_t);
                // USB-C
                port_cut(feather_x0 + feather_w / 2, usbc_z, usbc_w + 2.0, usbc_h + 1.6);
                // microSD
                port_cut(sd_x0 + sd_w / 2, sd_slot_z, sd_slot_w, sd_slot_h);
                // finger notch to push/pull the card
                translate([sd_x0 + sd_w / 2, -1, sd_slot_z]) rotate([-90, 0, 0]) cylinder(d = 8, h = wall / 2 + 1);
                // power switch on the top edge
                translate([slide_x - slide_slot_w / 2, case_h - wall - 1, slide_z - slide_slot_h / 2])
                    cube([slide_slot_w, wall + 2, slide_slot_h]);
            }
            lip();
            screw_tubes();
            pegs();
            sd_mount();
            switch_holder();
        }
        screw_holes();
        // engraved label on the back (outside handed() so it stays readable)
        if (label != "")
            translate([case_w / 2, case_h / 2, case_d - label_depth])
                linear_extrude(label_depth + 1)
                    text(label, size = label_size, font = label_font, halign = "center", valign = "center");
    }
}

print = false;
// Turned over about the Y axis (a rotation, not a mirror: mirroring would print
// the mirror image of the shell, label and port cut-outs included).
if (print) translate([case_w, 0, case_d]) rotate([0, 180, 0]) back_shell();
else back_shell();
