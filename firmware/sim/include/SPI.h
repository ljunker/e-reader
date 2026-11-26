#pragma once
#include <Arduino.h>

#define SPI_MODE0 0x00
#define SPI_MODE1 0x01
#define SPI_MODE2 0x02
#define SPI_MODE3 0x03

class SPISettings {
public:
    SPISettings() {}
    SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) : clock_(clock) { (void)bitOrder; (void)dataMode; }
    uint32_t clock_ = 1000000;
};

// One SPI bus; bytes go to the simulated device whose CS line is low.
class SPIClass {
public:
    void begin(int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1, int8_t ss = -1) { (void)sck; (void)miso; (void)mosi; (void)ss; }
    void end() {}
    void beginTransaction(SPISettings) {}
    void endTransaction() {}
    uint8_t transfer(uint8_t data);
    void transfer(void* data, uint32_t size);
    uint16_t transfer16(uint16_t data) { return uint16_t(transfer(uint8_t(data >> 8)) << 8 | transfer(uint8_t(data))); }
    void writeBytes(const uint8_t* data, uint32_t size) { for (uint32_t i = 0; i < size; i++) transfer(data[i]); }
};
extern SPIClass SPI;
