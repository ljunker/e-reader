# Assembly

Time: about 2–3 hours plus printing.

## 1. Breadboard bring-up (before soldering anything permanently)
1. Wire the display to the FeatherS3 per [wiring.md](../hardware/wiring.md) with jumper wires.
2. `cd firmware && pio run -t upload && pio device monitor`
3. Without an SD card you should see **"Storage: No microSD card"** on the display. That proves SPI, `PWR`, `BUSY` and rotation.
   - Upside down? Change `DISPLAY_ROTATION_PORTRAIT` in `firmware/include/config.h` (1 ↔ 3).
   - Nothing happens and the log shows "Busy Timeout": check `BUSY` and `PWR` (Rev2.2 needs `PWR` high).
4. Add the SD breakout and the buttons, then put a card with `/books/*.txt` in it. The library should appear.
5. Measure the battery voltage with a multimeter and compare it with the % shown; adjust `VBAT_SCALE` in `config.h`.

## 2. Prepare the display module
- Remove (desolder or clip) the straight pin header on the back if it is fitted, and solder thin silicone wires directly to the pads. Keep the side-entry connector only if `mod_back_clear` in `params.scad` allows it.
- Stick 1 mm foam tape on the back of the board where the chassis feet press (near the corners).

## 3. Button boards
- **Front:** a strip of perfboard (≈ 44 × 14 mm) with 3 tactile switches 16 mm apart (MENU, OK, BACK), switches facing the front.
- **Side:** a strip (≈ 55 × 7 mm) with 2 switches (PREV upper, NEXT lower) at the distance printed by `echo(side_btn_y)` in OpenSCAD (default ≈ 24 mm).
- One common GND wire per board, one signal wire per button.

## 4. Case
1. Press the 4 heat-set inserts into the bezel.
2. Drop the button caps into their holes, then slide the button boards into their slotted posts.
3. Lay the display face-down into the bezel ribs.
4. Screw the FeatherS3 onto the chassis standoffs (M2 × 4 self-tapping), USB-C towards the bottom edge. Put the battery into its pocket (Kapton tape between battery and anything sharp).
5. Wire everything; route wires through the slots in the chassis plate. Put the slide switch into its holder in the back shell, in series with battery +.
6. Stick the SD breakout onto its posts in the back shell, slot towards the bottom edge.
7. Place the chassis onto the display, close with the back shell, and fit 4 × M2 × 8 screws.

## 5. First start
Slide the power switch on. The library appears. Long-press NEXT/PREV to jump 10 entries/pages.
After 3 minutes without input the reader sleeps; any button wakes it.
