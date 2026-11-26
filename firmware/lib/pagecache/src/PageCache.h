#pragma once
// Which page each prepared frame holds, so a page turn can show a frame drawn
// in advance instead of laying the page out first. Platform independent: the
// frames themselves live in the firmware (Display::Frame in PSRAM).

#include <stdint.h>

namespace pagecache {

class Slots {
public:
    // The page on screen, the next one and the previous one.
    static constexpr int kSlots = 3;

    // Forget every page (another book, font or layout).
    void clear();
    // Slot that holds `page`, or -1.
    int find(uint32_t page) const;
    // Slot to keep `page` in while `current` is on screen: the slot that
    // already has it, else a free one, else the one farthest from `current`.
    int claim(uint32_t page, uint32_t current);
    // The slot's frame could not be kept after all.
    void release(int slot);
    // Page worth preparing now: the next one, then the previous one. False
    // when both are ready or do not exist.
    bool wanted(uint32_t current, uint32_t pageCount, uint32_t& page) const;

private:
    bool used_[kSlots] = {};
    uint32_t page_[kSlots] = {};
};

}  // namespace pagecache
