#pragma once
#include <Arduino.h>

// The microSD card as a USB drive on the Feather's USB-C port.
// The drive is always announced to the computer, but shows "no media" until
// enable(); while it is enabled the reader keeps its hands off the card.
namespace usbdrive {
void begin();             // after storage::begin(); does nothing without a card
bool available();         // the card is known to the USB stack
bool hostConnected();     // a computer (not a charger) enumerated the device
// True once each time a computer connects, unless the user said no to this
// connection already (remembered through deep sleep until the cable comes out).
bool takeHostArrived();
void decline();
void enable();
// Hides the drive again and mounts the card for the reader.
bool disable(String& error);
bool active();
bool ejected();           // the computer ejected the drive
}  // namespace usbdrive
