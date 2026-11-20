// Button caps. Print flange-down (as modelled). 3 front caps + 2 side caps.
include <common.scad>

module front_cap() {
    cylinder(d = cap_flange_d, h = cap_flange_t);
    cylinder(d = cap_d - hole_tol, h = cap_flange_t + front_t + protrude);
    // nub on the flange's back that reaches down to the switch actuator
    translate([0, 0, -front_nub]) cylinder(d = 3, h = front_nub + eps);
}

module side_cap() {
    // modelled lying flat: oval in XY, height along Z. The flange only grows in
    // length, so the cap stays inside the back shell's height.
    oval(side_cap_l + 4, side_cap_w, cap_flange_t);
    oval(side_cap_l - hole_tol, side_cap_w - hole_tol, cap_flange_t + wall + protrude);
    translate([0, 0, -side_nub]) cylinder(d = 2.5, h = side_nub + eps);
}

module button_set() {
    for (i = [0 : 2]) translate([i * (cap_flange_d + 4), 0, front_nub]) front_cap();
    for (i = [0 : 1]) translate([3 * (cap_flange_d + 4) + i * (side_cap_w + 8), 0, side_nub]) side_cap();
}

button_set();
