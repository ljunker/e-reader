// Front bezel: front face with the display window, side walls up to split_z,
// locating ribs for the display module, screw bosses with heat-set inserts that
// the carrier PCB rests on, and support posts behind the front buttons.
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

// Posts between the front switches carry the button force into the front face,
// so the PCB does not flex when a button is pressed.
module button_posts() {
    xs = [front_btn_x[0] - btn_pitch / 2, (front_btn_x[0] + front_btn_x[1]) / 2,
          (front_btn_x[1] + front_btn_x[2]) / 2, front_btn_x[2] + btn_pitch / 2];
    for (x = xs) translate([x - 1.5, btn_y - 1.5, front_t - eps])
        cube([3, 3, pcb_z0 - front_t + eps]);
}

// The PCB rests on these; heat-set inserts take the M2 screws from the back.
module bosses() {
    for (p = screw_pos) translate([p[0], p[1], front_t - eps])
        cylinder(d = boss_d, h = pcb_z0 - front_t + eps);
}

module bezel() {
    difference() {
        union() {
            difference() {
                case_body(0, split_z);
                rbox_inset(wall, front_t, split_z + eps);
                window_cut();
                front_button_holes();
            }
            display_ribs();
            button_posts();
            bosses();
        }
        for (p = screw_pos) translate([p[0], p[1], pcb_z0 - insert_l]) cylinder(d = insert_d, h = insert_l + 1);
    }
}

bezel();
