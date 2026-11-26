#pragma once
// Register maths for the MAX17048 fuel gauge on the FeatherS3[D]. No I/O
// here, so it is tested on the host; the bus access is in src/Power.cpp.
#include <stddef.h>
#include <stdint.h>

namespace fuelgauge {

constexpr uint8_t kMax17048Addr = 0x36;

// MAX17048 (registers are big-endian)
constexpr uint8_t kMaxRegVcell = 0x02;   // 78.125 µV per bit
constexpr uint8_t kMaxRegSoc = 0x04;     // 1/256 % per bit

inline int max17048Millivolts(uint16_t vcell) {
    return int((uint64_t(vcell) * 78125u + 500000u) / 1000000u);
}

// Rounded to whole percent; the chip may report a little over 100 when full.
inline int max17048Percent(uint16_t soc) {
    int p = (int(soc) + 128) >> 8;
    return p > 100 ? 100 : p;
}

}  // namespace fuelgauge
