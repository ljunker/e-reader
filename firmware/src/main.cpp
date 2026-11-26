// DIY e-reader — Adafruit ESP32-S3 Feather + Waveshare 4.2" V2 e-paper.
#include <Arduino.h>
#include <esp_sleep.h>

#include "Buttons.h"
#include "Display.h"
#include "Power.h"
#include "Storage.h"
#include "Ui.h"
#include "config.h"

// Survives deep sleep: which book was open and where.
RTC_DATA_ATTR static char rtcPath[160];
RTC_DATA_ATTR static uint32_t rtcPos;

static Buttons buttons;
static uint32_t lastActivity;

static void goToSleep() {
    strlcpy(rtcPath, ui.openPath(), sizeof rtcPath);
    rtcPos = ui.openPosition();
    ui.prepareSleep();
    power::deepSleep();
}

void setup() {
    Serial.begin(115200);
    power::begin();
    buttons.begin();

    bool fromSleep = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1;
    Btn wakeButton = fromSleep ? Buttons::fromWakeMask(esp_sleep_get_ext1_wakeup_status()) : Btn::None;

    display.begin(!fromSleep);
    String err;
    storage::begin(err);

    if (fromSleep && rtcPath[0] && err.isEmpty() && ui.resume(rtcPath, rtcPos)) {
        // The panel still shows the page we fell asleep on. A page-turn button
        // turns the page right away; any other button just redraws it.
        if (wakeButton == Btn::Next || wakeButton == Btn::Prev) ui.handle({wakeButton, false});
        else ui.redraw();
    } else {
        ui.begin(err);
    }
    lastActivity = millis();
}

void loop() {
    ButtonEvent e = buttons.poll();
    if (e.button != Btn::None) {
        lastActivity = millis();
        ui.handle(e);
    }
    if (millis() - lastActivity > SLEEP_AFTER_MS && !buttons.anyDown()) goToSleep();
    delay(5);
}
