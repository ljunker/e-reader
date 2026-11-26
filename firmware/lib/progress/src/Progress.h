#pragma once
// Reading progress for the status line and the library. Platform independent.

#include <stdint.h>

#include <vector>

namespace progress {

// Percent read: 0 on the first page, 100 on the last.
uint8_t percent(uint32_t page, uint32_t pageCount);

struct Chapter {
    int index = -1;           // chapter the page belongs to, -1 before the first one
    uint32_t pagesLeft = 0;   // pages after this one until the next chapter (or the end)
};

// `starts` holds the first page of each chapter in book order. Several
// chapters may start on the same page; the last of them wins.
Chapter locate(const std::vector<uint32_t>& starts, uint32_t page, uint32_t pageCount);

}  // namespace progress
