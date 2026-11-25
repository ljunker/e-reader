// Assembled / exploded view with stand-ins for the electronics.
// Set explode > 0 in the customizer to pull the parts apart.
// Set check = true to render only the intersections of printed parts with
// the electronics stand-ins: an empty result means nothing collides.
use <bezel.scad>
use <chassis.scad>
use <back.scad>
include <common.scad>

explode = 0;   // mm
check = false;
section = false;   // cut the model in half at the Feather's centre line

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

if (check) {
    // each pair of solids that must not overlap
    intersection() { bezel(); union() { display_dummy(); feather_dummy(); sd_dummy(); battery_dummy(); buttons_dummy(); } }
    intersection() { chassis(); union() { display_dummy(); feather_dummy(); sd_dummy(); battery_dummy(); buttons_dummy(); } }
    intersection() { back_shell(); union() { display_dummy(); feather_dummy(); sd_dummy(); battery_dummy(); buttons_dummy(); } }
    intersection() { bezel(); chassis(); }
    intersection() { back_shell(); chassis(); }
} else if (section) {
    difference() { view(); translate([hx(feather_x0 + feather_w / 2), -1, -1]) cube([200, 200, 100]); }
} else view();
