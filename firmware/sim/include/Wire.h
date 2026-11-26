#pragma once
#include <Arduino.h>

// I2C bus with the simulated battery monitor (MAX17048) on it.
class TwoWire : public Stream {
public:
    bool begin(int sda = -1, int scl = -1, uint32_t frequency = 0) { (void)sda; (void)scl; (void)frequency; return true; }
    bool end() { return true; }
    void beginTransmission(uint8_t address);
    uint8_t endTransmission(bool sendStop = true);
    uint8_t requestFrom(uint8_t address, size_t len, bool stopBit = true);
    size_t write(uint8_t data) override;
    size_t write(const uint8_t* data, size_t n) override;
    int available() override;
    int read() override;
    int peek() override;
    void flush() override {}

private:
    uint8_t addr_ = 0;
    uint8_t tx_[32];
    size_t txLen_ = 0;
    uint8_t rx_[32];
    size_t rxLen_ = 0, rxPos_ = 0;
};
extern TwoWire Wire;
