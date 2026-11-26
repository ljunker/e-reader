#pragma once
#include "USB.h"

typedef int32_t (*msc_read_cb)(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize);
typedef int32_t (*msc_write_cb)(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize);
typedef bool (*msc_start_stop_cb)(uint8_t power_condition, bool start, bool load_eject);

// The computer side works on the card's files directly (the page's file
// panel), so sector reads and writes never reach these callbacks; eject does.
class USBMSC {
public:
    bool begin(uint32_t block_count, uint16_t block_size);
    void end() {}
    void vendorID(const char* vid) { (void)vid; }
    void productID(const char* pid) { (void)pid; }
    void productRevision(const char* ver) { (void)ver; }
    void onStartStop(msc_start_stop_cb cb);
    void onRead(msc_read_cb cb) { (void)cb; }
    void onWrite(msc_write_cb cb) { (void)cb; }
    void mediaPresent(bool media_present);
};
