#pragma once
// Register maths for the battery monitor on the Adafruit ESP32-S3 Feather.
// Newer boards carry a MAX17048, older ones an LC709203F; the device code in
// src/Power.cpp probes for both. No I/O here, so it is tested on the host.
#include <stddef.h>
#include <stdint.h>

namespace fuelgauge {

constexpr uint8_t kMax17048Addr = 0x36;
constexpr uint8_t kLc709203fAddr = 0x0B;

// MAX17048 (registers are big-endian)
constexpr uint8_t kMaxRegVcell = 0x02;   // 78.125 µV per bit
constexpr uint8_t kMaxRegSoc = 0x04;     // 1/256 % per bit

// LC709203F (registers are little-endian, every transfer carries a CRC-8)
constexpr uint8_t kLcRegCellVoltage = 0x09;   // mV
constexpr uint8_t kLcRegPowerMode = 0x15;     // 1 = operational, 2 = sleep

inline int max17048Millivolts(uint16_t vcell) {
    return int((uint64_t(vcell) * 78125u + 500000u) / 1000000u);
}

// Rounded to whole percent; the chip may report a little over 100 when full.
inline int max17048Percent(uint16_t soc) {
    int p = (int(soc) + 128) >> 8;
    return p > 100 ? 100 : p;
}

// CRC-8, polynomial x^8 + x^2 + x + 1, initial value 0 (CRC-8/SMBUS), as the
// LC709203F expects over the bus bytes including the address bytes.
inline uint8_t crc8(const uint8_t* d, size_t n) {
    uint8_t crc = 0;
    for (size_t i = 0; i < n; i++) {
        crc ^= d[i];
        for (int b = 0; b < 8; b++) crc = (crc & 0x80) ? uint8_t((crc << 1) ^ 0x07) : uint8_t(crc << 1);
    }
    return crc;
}

// Rough charge from the cell voltage, linear between empty and full, 0..100.
inline int linearPercent(int mv, int emptyMv, int fullMv) {
    int p = (mv - emptyMv) * 100 / (fullMv - emptyMv);
    return p < 0 ? 0 : (p > 100 ? 100 : p);
}

}  // namespace fuelgauge
