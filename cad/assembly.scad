// Assembled / exploded view with stand-ins for the electronics.
// Set explode > 0 in the customizer to pull the parts apart.
// Set check = true to render only the intersections of printed parts with
// the electronics stand-ins: an empty result means nothing collides.
use <bezel.scad>
use <chassis.scad>
use <back.scad>
use <buttons.scad>
include <common.scad>

explode = 0;   // mm
check = false;
section = false;   // cut the model in half at the Feather's centre line
// Build-guide pictures (make steps): 1..7 show assembly step N with the new
// printed parts in orange and lifted, earlier parts in their case colours; 8 is the
// finished reader. 6 shows the back shell on its own, inside up.
step = 0;
lift = 14;   // how far the parts of the current step hover above their place

module printed() {
    color("dimgray") bezel();
    translate([0, 0, explode]) color("lightgray", 0.9) chassis();
    translate([0, 0, 2 * explode]) color("dimgray", 0.6) back_shell();
}

module electronics() {
    display_dummy();
    translate([0, 0, explode]) { feather_dummy(); battery_dummy(); }
    translate([0, 0, 2 * explode]) sd_dummy();
    buttons_dummy();
}

module view() { printed(); electronics(); }

// Button caps in their holes (flange behind the front face / side wall).
module caps_placed() {
    handed() {
        for (x = front_btn_x) translate([x, btn_y, front_t + cap_flange_t]) mirror([0, 0, 1]) front_cap();
        for (y = side_btn_y) translate([case_w - wall - cap_flange_t, y, side_btn_z]) rotate([0, 90, 0]) side_cap();
    }
}

// Printed parts of the current step are orange, all others in their case colour.
function pc(n, c) = n == step ? "darkorange" : c;

// Parts added in assembly step n, in assembly position.
module step_parts(n) {
    if (n == 1) { color(pc(1, "dimgray")) bezel(); inserts_dummy(); }
    if (n == 2) color(pc(2, "dimgray")) caps_placed();
    if (n == 3) buttons_dummy();
    if (n == 4) display_dummy();
    if (n == 5) { color(pc(5, "lightgray")) chassis(); feather_dummy(); battery_dummy(); }
    if (n == 6) { color(pc(6, "dimgray")) back_shell(); sd_dummy(); slide_dummy(); }
    if (n == 7) screws_dummy();
}

module step_view(n) {
    if (n == 6) {
        // back shell sub-assembly, turned over (like a page) so its inside faces the camera
        translate([case_w, 0, case_d]) rotate([0, 180, 0]) step_parts(6);
    } else if (n >= 7) {
        for (i = [1 : 6]) step_parts(i);
        translate([0, 0, n == 7 ? lift : 0]) step_parts(7);
    } else {
        for (i = [1 : n - 1]) step_parts(i);
        translate([0, 0, n == 1 ? 0 : lift]) step_parts(n);
    }
}

if (check) {
    // each pair of solids that must not overlap
    intersection() { bezel(); union() { display_dummy(); feather_dummy(); sd_dummy(); battery_dummy(); buttons_dummy(); } }
    intersection() { chassis(); union() { display_dummy(); feather_dummy(); sd_dummy(); battery_dummy(); buttons_dummy(); } }
    intersection() { back_shell(); union() { display_dummy(); feather_dummy(); sd_dummy(); battery_dummy(); buttons_dummy(); } }
    intersection() { bezel(); chassis(); }
    intersection() { back_shell(); chassis(); }
} else if (section) {
    difference() { view(); translate([hx(feather_x0 + feather_w / 2), -1, -1]) cube([200, 200, 100]); }
} else if (step > 0) step_view(step);
else view();
