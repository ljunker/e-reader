// Quick fit test (~20 min on an A1 mini): the bezel's front face, window and
// display ribs only. Drop the display module in and check window alignment
// and rib clearance before printing the full case.
use <bezel.scad>
include <common.scad>

intersection() {
    bezel();
    translate([-1, -1, -1]) cube([case_w + 2, case_h + 2, 1 + front_t + panel_t + mod_pcb_t]);
}
