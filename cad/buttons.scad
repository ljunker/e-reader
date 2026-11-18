// Button caps. Print flange-down (as modelled). 3 front caps + 2 side caps.
include <common.scad>

protrude = 0.8;   // how far a cap stands out of the case

module front_cap() {
    cylinder(d = cap_flange_d, h = cap_flange_t);
    cylinder(d = cap_d - hole_tol, h = cap_flange_t + front_t + protrude);
    // small nub on the flange's back that presses the switch actuator
    translate([0, 0, -cap_travel]) cylinder(d = 3, h = cap_travel + eps);
}

module side_cap() {
    // modelled lying flat: oval in XY, height along Z
    oval(side_cap_l + 4, side_cap_w + 2, cap_flange_t);
    oval(side_cap_l - hole_tol, side_cap_w - hole_tol, cap_flange_t + wall + protrude);
    translate([0, 0, -cap_travel]) cylinder(d = 3, h = cap_travel + eps);
}

module button_set() {
    for (i = [0 : 2]) translate([i * (cap_flange_d + 4), 0, cap_travel]) front_cap();
    for (i = [0 : 1]) translate([3 * (cap_flange_d + 4) + i * (side_cap_w + 8), 0, cap_travel]) side_cap();
}

button_set();
