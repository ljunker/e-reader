// Prints the values the PCB generator needs (hardware/kicad/carrier/generate.py).
include <params.scad>
echo(P = [
    ["case_w", case_w], ["case_h", case_h], ["case_r", case_r], ["pcb_inset", pcb_inset], ["pcb_r", pcb_r],
    ["pcb_t", pcb_t], ["screw_pos", screw_pos], ["mod_x0", mod_x0], ["mod_y0", mod_y0], ["mod_w", mod_w], ["mod_h", mod_h],
    ["conn_cut", conn_cut], ["conn_cut_y", conn_cut_y],
    ["front_btn_x", front_btn_x], ["btn_y", btn_y], ["side_btn_y", side_btn_y], ["side_act_x", side_act_x],
    ["feather_x0", feather_x0], ["feather_y0", feather_y0], ["feather_w", feather_w], ["feather_l", feather_l],
    ["sd_cy", sd_cy], ["slide_x", slide_x], ["bat_x0", bat_x0], ["bat_y0", bat_y0], ["bat_w", bat_w], ["bat_l", bat_l],
]);
