#include "UsbDrive.h"

#include <SD_MMC.h>
#include <USB.h>
#include <USBMSC.h>

#include "Storage.h"

// Global so its constructor registers the mass-storage interface before the
// core starts USB (Serial.begin() runs before setup() with CDC on boot).
static USBMSC msc;

// Survives deep sleep: "not now" for the computer that is still plugged in.
RTC_DATA_ATTR static bool rtcDeclined = false;

namespace usbdrive {

static bool ready = false;
static volatile bool active_ = false;
static volatile bool ejected_ = false;
static volatile bool suspended = false;
static bool announced = false;   // takeHostArrived() fired for this connection
static uint16_t sectorSize = 512;

static int32_t onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    if (!active_ || offset != 0 || bufsize % sectorSize) return -1;
    uint8_t* p = static_cast<uint8_t*>(buffer);
    for (uint32_t i = 0; i < bufsize / sectorSize; i++)
        if (!SD_MMC.readRAW(p + i * sectorSize, lba + i)) return -1;
    return int32_t(bufsize);
}

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    if (!active_ || offset != 0 || bufsize % sectorSize) return -1;
    for (uint32_t i = 0; i < bufsize / sectorSize; i++)
        if (!SD_MMC.writeRAW(buffer + i * sectorSize, lba + i)) return -1;
    return int32_t(bufsize);
}

static bool onStartStop(uint8_t, bool start, bool loadEject) {
    if (loadEject && !start) ejected_ = true;
    return true;
}

static void onUsbEvent(void*, esp_event_base_t base, int32_t id, void*) {
    if (base != ARDUINO_USB_EVENTS) return;
    switch (id) {
        case ARDUINO_USB_STARTED_EVENT:
        case ARDUINO_USB_RESUME_EVENT:
            suspended = false;
            break;
        case ARDUINO_USB_SUSPEND_EVENT:   // cable pulled (or the computer sleeps)
        case ARDUINO_USB_STOPPED_EVENT:
            suspended = true;
            break;
        default: break;
    }
}

void begin() {
    USB.onEvent(onUsbEvent);
    if (!storage::mounted()) return;
    sectorSize = uint16_t(SD_MMC.sectorSize());
    uint32_t sectors = uint32_t(SD_MMC.numSectors());
    if (!sectorSize || !sectors) return;
    msc.vendorID("DIY");
    msc.productID("E-Reader");
    msc.productRevision("1.0");
    msc.onRead(onRead);
    msc.onWrite(onWrite);
    msc.onStartStop(onStartStop);
    msc.mediaPresent(false);
    ready = msc.begin(sectors, sectorSize);
}

bool available() { return ready; }

bool hostConnected() { return bool(USB) && !suspended; }

bool takeHostArrived() {
    if (!hostConnected()) {
        announced = false;
        rtcDeclined = false;   // cable out: ask again next time
        return false;
    }
    if (!ready || announced || rtcDeclined) return false;
    announced = true;
    return true;
}

void decline() { rtcDeclined = true; }

void enable() {
    storage::suspend();
    ejected_ = false;
    active_ = true;
    msc.mediaPresent(true);
}

bool disable(String& error) {
    msc.mediaPresent(false);
    active_ = false;
    ejected_ = false;
    rtcDeclined = true;   // don't offer the drive again for this connection
    return storage::remount(error);
}

bool active() { return active_; }
bool ejected() { return ejected_; }

}  // namespace usbdrive
