#include "PageCache.h"

namespace pagecache {

static uint32_t distance(uint32_t a, uint32_t b) { return a > b ? a - b : b - a; }

void Slots::clear() {
    for (bool& u : used_) u = false;
}

int Slots::find(uint32_t page) const {
    for (int i = 0; i < kSlots; i++)
        if (used_[i] && page_[i] == page) return i;
    return -1;
}

int Slots::claim(uint32_t page, uint32_t current) {
    int s = find(page);
    if (s < 0) {
        for (int i = 0; i < kSlots && s < 0; i++)
            if (!used_[i]) s = i;
    }
    if (s < 0) {
        s = 0;
        for (int i = 1; i < kSlots; i++)
            if (distance(page_[i], current) > distance(page_[s], current)) s = i;
    }
    used_[s] = true;
    page_[s] = page;
    return s;
}

void Slots::release(int slot) {
    if (slot >= 0 && slot < kSlots) used_[slot] = false;
}

bool Slots::wanted(uint32_t current, uint32_t pageCount, uint32_t& page) const {
    if (current + 1 < pageCount && find(current + 1) < 0) {
        page = current + 1;
        return true;
    }
    if (current > 0 && current - 1 < pageCount && find(current - 1) < 0) {
        page = current - 1;
        return true;
    }
    return false;
}

}  // namespace pagecache
