# Assembly

Time: about 2 hours plus printing and PCB delivery.

## 1. Breadboard bring-up (before soldering anything permanently)
1. Wire the display to the FeatherS3 per [wiring.md](../hardware/wiring.md) with jumper wires.
2. `cd firmware && pio run -t upload && pio device monitor`
3. Without an SD card you should see **"Storage: No microSD card"** on the display. That proves SPI, `PWR`, `BUSY` and rotation.
   - Upside down? Change `DISPLAY_ROTATION_PORTRAIT` in `firmware/include/config.h` (1 ↔ 3).
   - Nothing happens and the log shows "Busy Timeout": check `BUSY` and `PWR` (Rev2.2 needs `PWR` high).
4. Add an SD breakout (Adafruit 4682) and the buttons, then put a card with `/books/*.txt` in it. The library should appear.
5. Measure the battery voltage with a multimeter and compare it with the % shown; adjust `VBAT_SCALE` in `config.h`.

## 2. Solder the carrier board
Follow the soldering order in [the carrier board README](../hardware/kicad/carrier/README.md#soldering-order).
Then plug the board onto the breadboard-tested setup: same pins, same firmware.

## 3. Prepare the display module
- Remove (desolder or clip) the straight pin header on the back if it is fitted. Either plug the
  Waveshare cable's Dupont ends onto J3 (VCC first, labels on the silkscreen) or solder the wires
  straight into J3's holes.
- Stick 3 mm foam pads on the back of the display board near its corners; the carrier board presses
  the display into the front frame through them.

## 4. Case
1. Press the 4 heat-set inserts into the front frame's corner bosses.
2. Drop the 3 front button caps into their holes, flange inside.
3. Lay the display face-down into the frame's ribs, cable towards the top.
4. Route the display cable through the carrier board's opening and connect it to J3.
5. Put the carrier board onto the 4 bosses (front buttons facing the display).
6. Plug in the battery (check polarity at J2), lay it crosswise above the Feather on a strip of Kapton tape,
   and fix it with double-sided tape.
7. Put the 2 side button caps into the back shell's right wall, flange inside, and close the case with
   4 × M2 × 12 screws. They go through the back shell and the carrier board into the inserts.

## 5. First start
Slide the power switch on (top edge). The library appears. Long-press NEXT/PREV to jump 10 entries/pages.
After 3 minutes without input the reader sleeps; any button wakes it.
The battery charges over USB-C only while the power switch is **on** (the switch sits in the battery line).
