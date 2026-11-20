// Back shell: z ∈ [split_z, case_d] plus an alignment lip reaching into the bezel.
// Its screw tubes clamp the carrier PCB onto the bezel's bosses.
// Cut-outs: USB-C (bottom), power switch (top), microSD and page-turn buttons (right).
// Print outside face down (the Makefile flips it), no supports needed.
include <common.scad>

// rounded slot through a wall; `axis` = "y" (bottom/top wall) or "x" (right wall)
module slot(x, y, z, w, h, axis = "y", depth = wall + 2) {
    translate([x, y, z]) rotate(axis == "y" ? [-90, 0, 0] : [0, 90, 0])
        linear_extrude(depth, center = true) offset(r = h / 2 - 0.01) offset(delta = -(h / 2 - 0.01))
            square(axis == "y" ? [w, h] : [h, w], center = true);
}

module lip() {
    difference() {
        rbox_inset(wall + tol, split_z - lip_h, split_z + eps);
        rbox_inset(wall + tol + lip_t, split_z - lip_h - 1, split_z + 1);
        for (p = screw_pos) translate([p[0], p[1], 0]) cylinder(d = boss_d + 1.5, h = case_d);
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

module back_shell() {
    difference() {
        union() {
            difference() {
                case_body(split_z, case_d, round_front = false);
                rbox_inset(wall, split_z - eps, case_d - back_t);
                // USB-C, bottom edge
                slot(feather_x0 + feather_w / 2, wall / 2, usbc_z, usbc_w + 2.0, usbc_h + 1.6);
                // power switch lever, top edge
                slot(slide_x, case_h - wall / 2, slide_z, slide_slot_w, slide_slot_h);
                // microSD, right edge, plus a finger notch to pull the card
                slot(case_w - wall / 2, sd_cy, sd_slot_z, sd_slot_w, sd_slot_h, "x");
                translate([case_w + 1, sd_cy, sd_slot_z]) rotate([0, -90, 0]) cylinder(d = 7, h = wall / 2 + 1);
                // page-turn buttons, right edge
                for (y = side_btn_y)
                    translate([case_w - wall - eps, y, side_btn_z]) rotate([0, 90, 0])
                        oval(side_cap_l + hole_tol, side_cap_w + hole_tol, wall + 2 * eps);
            }
            lip();
            screw_tubes();
        }
        screw_holes();
        // engraved label on the back
        translate([case_w / 2, case_h / 2, case_d - 0.4])
            linear_extrude(1) text("e-reader", size = 7, halign = "center", valign = "center");
    }
}

print = false;
if (print) translate([0, 0, case_d]) mirror([0, 0, 1]) back_shell();
else back_shell();
