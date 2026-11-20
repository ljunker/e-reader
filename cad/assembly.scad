// Assembled / exploded view with stand-ins for the display, carrier PCB and parts.
// Set explode > 0 in the customizer to pull the parts apart.
// Collision checks live in check.py.
use <bezel.scad>
use <back.scad>
include <common.scad>

explode = 0;      // mm
section = false;  // cut the model in half at the Feather's centre line

module view() {
    color("dimgray") bezel();
    translate([0, 0, explode]) electronics();
    translate([0, 0, 2 * explode]) color("dimgray", 0.6) back_shell();
}

if (section) {
    difference() { view(); translate([feather_x0 + feather_w / 2, -1, -1]) cube([200, 200, 100]); }
} else view();
