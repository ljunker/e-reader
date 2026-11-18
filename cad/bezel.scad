// Front bezel: front face with the display window, side walls up to split_z,
// locating ribs for the display module, heat-set insert bosses, button holes.
// Print face-down (as modelled, z = 0 on the bed) on the textured PEI plate. No supports.
include <common.scad>

module window_cut() {
    x0 = mod_x0 + active_off_x - window_margin;
    y0 = mod_y0 + active_off_y - window_margin;
    w = active_w + 2 * window_margin;
    h = active_h + 2 * window_margin;
    translate([x0, y0, -eps]) cube([w, h, front_t + 2 * eps]);
    // 45° chamfer towards the viewer
    hull() {
        translate([x0, y0, window_chamfer]) cube([w, h, eps]);
        translate([x0 - window_chamfer, y0 - window_chamfer, -eps])
            cube([w + 2 * window_chamfer, h + 2 * window_chamfer, eps]);
    }
}

module front_button_holes() {
    for (x = front_btn_x) {
        translate([x, btn_y, -eps]) cylinder(d = cap_d + hole_tol, h = front_t + 2 * eps);
        // shallow dish so the buttons are easy to find by touch
        translate([x, btn_y, -eps]) cylinder(d1 = cap_d + 3, d2 = cap_d + hole_tol, h = 0.6);
    }
}

module side_button_holes() {
    for (y = side_btn_y)
        translate([case_w - wall - eps, y, side_btn_z]) rotate([0, 90, 0])
            oval(side_cap_l + hole_tol, side_cap_w + hole_tol, wall + 2 * eps);
}

// Ribs that locate the display module in X/Y; gaps keep it removable.
module display_ribs() {
    rib_t = 1.2;
    rib_h = panel_t + mod_pcb_t;
    z0 = front_t - eps;
    x0 = mod_x0 - gap;  x1 = mod_x0 + mod_w + gap;
    y0 = mod_y0 - gap;  y1 = mod_y0 + mod_h + gap;
    for (f = [0.15, 0.6]) {
        translate([x0 - rib_t, y0 + f * mod_h, z0]) cube([rib_t, 0.25 * mod_h, rib_h]);
        translate([x1, y0 + f * mod_h, z0]) cube([rib_t, 0.25 * mod_h, rib_h]);
    }
    for (f = [0.2, 0.6]) {
        translate([x0 + f * mod_w, y0 - rib_t, z0]) cube([0.2 * mod_w, rib_t, rib_h]);
        translate([x0 + f * mod_w, y1, z0]) cube([0.2 * mod_w, rib_t, rib_h]);
    }
}

// Slotted posts that hold the front button board at depth fbb_z0.
module front_board_guides() {
    post_h = fbb_z0 + perf_t + 0.8 - front_t;
    for (side = [-1, 1]) {
        x = btn_cx + side * (fbb_w / 2 + 0.6);
        translate([x - 1.4, btn_y - 4, front_t - eps])
            difference() {
                cube([2.8, 8, post_h]);
                // slot the board edge sits in
                translate([side < 0 ? 1.4 : -eps, -eps, fbb_z0 - front_t]) cube([1.4 + eps, 8 + 2 * eps, perf_t + tol]);
            }
    }
    // spacers between the switches take the button force
    for (i = [0 : 1]) translate([(front_btn_x[i] + front_btn_x[i + 1]) / 2 - 1.5, btn_y - 3, front_t - eps])
        cube([3, 6, fbb_z0 - front_t]);
}

// Slotted posts for the side button board (board in the YZ plane, switches face +X).
module side_board_guides() {
    y0 = side_btn_y[1] - 8;
    for (y = [y0 - 1.5, y0 + sbb_h])
        translate([sbb_x1 - perf_t - 1.2 - tol, y, front_t - eps])
            difference() {
                cube([perf_t + 2.4 + tol, 1.5, split_z - front_t]);
                translate([1.2, -eps, -eps]) cube([perf_t + tol, 1.5 + 2 * eps, split_z]);
            }
    // spacer between the two switches
    translate([sbb_x1, (side_btn_y[0] + side_btn_y[1]) / 2 - 2, front_t - eps])
        cube([sw_h - 0.5, 4, split_z - front_t - 1]);
}

module bosses() {
    for (p = screw_pos) translate([p[0], p[1], front_t - eps])
        cylinder(d = boss_d, h = split_z - front_t + eps);
}

module bezel() {
    difference() {
        union() {
            difference() {
                case_body(0, split_z);
                rbox_inset(wall, front_t, split_z + eps);
                window_cut();
                front_button_holes();
                side_button_holes();
            }
            display_ribs();
            front_board_guides();
            side_board_guides();
            bosses();
        }
        for (p = screw_pos) translate([p[0], p[1], split_z - insert_l]) cylinder(d = insert_d, h = insert_l + 1);
    }
}

bezel();
