#include "Progress.h"

namespace progress {

uint8_t percent(uint32_t page, uint32_t pageCount) {
    if (pageCount <= 1 || page + 1 >= pageCount) return 100;
    return uint8_t(uint64_t(page) * 100 / (pageCount - 1));
}

Chapter locate(const std::vector<uint32_t>& starts, uint32_t page, uint32_t pageCount) {
    Chapter c;
    uint32_t next = pageCount;
    for (size_t i = 0; i < starts.size(); i++) {
        if (starts[i] <= page) {
            c.index = int(i);
        } else if (starts[i] < next) {
            next = starts[i];
            break;   // in book order, the first later start is the next chapter
        }
    }
    c.pagesLeft = next > page ? next - page - 1 : 0;
    return c;
}

}  // namespace progress
