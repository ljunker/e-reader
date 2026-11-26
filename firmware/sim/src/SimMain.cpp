// Entry point: the Arduino core's loop task, minus FreeRTOS. delay() inside
// loop() hands control back to the browser (ASYNCIFY).
#include <Arduino.h>

void setup();
void loop();
void simRestoreRtc();

int main() {
    simRestoreRtc();
    setup();
    for (;;) loop();
}
