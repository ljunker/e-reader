#include "Ssd1683.h"

#include <string.h>

namespace epdsim {

void Ssd1683::defaults() {
    cmd_ = 0;
    nArgs_ = 0;
    entryMode_ = 0x03;
    xStart_ = 0;
    xEnd_ = kStride - 1;
    yStart_ = 0;
    yEnd_ = kHeight - 1;
    xCount_ = yCount_ = 0;
    updateSeq_ = 0xff;
    updateCtrl_ = 0;
    lutLoaded_ = false;
}

void Ssd1683::powerLoss() {
    memset(bw_, 0xff, sizeof bw_);
    memset(red_, 0xff, sizeof red_);
    defaults();
    analogOn_ = false;
    sleeping_ = false;
    busyUntil_ = 0;
}

void Ssd1683::hardwareReset(uint32_t nowMs) {
    sleeping_ = false;
    analogOn_ = false;
    defaults();
    holdBusy(kResetMs, nowMs);
}

void Ssd1683::holdBusy(uint32_t ms, uint32_t nowMs) {
    uint32_t scaled = uint32_t(float(ms) * timeScale);
    busyUntil_ = nowMs + scaled;
}

void Ssd1683::write(bool isData, uint8_t byte, uint32_t nowMs) {
    if (sleeping_) return;   // only a hardware reset wakes the controller
    if (isData) data(byte, nowMs);
    else command(byte, nowMs);
}

void Ssd1683::command(uint8_t c, uint32_t nowMs) {
    cmd_ = c;
    nArgs_ = 0;
    switch (c) {
        case 0x12:   // software reset
            defaults();
            holdBusy(kResetMs, nowMs);
            break;
        case 0x20:   // master activation
            activate(nowMs);
            break;
        case 0x32:   // waveform (LUT) from the host instead of the OTP
            lutLoaded_ = true;
            break;
        case 0x01: case 0x03: case 0x04: case 0x0C: case 0x10: case 0x11:
        case 0x18: case 0x1A: case 0x21: case 0x22: case 0x24: case 0x26:
        case 0x2C: case 0x3C: case 0x3F: case 0x44: case 0x45: case 0x4E: case 0x4F:
            break;
        default:
            unknown_++;
            break;
    }
}

void Ssd1683::data(uint8_t d, uint32_t nowMs) {
    (void)nowMs;
    if (cmd_ == 0x24) { ramWrite(bw_, d); return; }
    if (cmd_ == 0x26) { ramWrite(red_, d); return; }
    if (nArgs_ < int(sizeof args_)) args_[nArgs_] = d;
    int n = ++nArgs_;
    switch (cmd_) {
        case 0x10:
            if (n == 1 && (d & 0x03)) sleeping_ = true;
            break;
        case 0x11:
            if (n == 1) entryMode_ = d & 0x07;
            break;
        case 0x21:
            if (n == 1) updateCtrl_ = d;
            break;
        case 0x22:
            if (n == 1) updateSeq_ = d;
            break;
        case 0x44:   // X window, in bytes
            if (n == 1) xStart_ = d & 0x3f;
            if (n == 2) xEnd_ = d & 0x3f;
            break;
        case 0x45:   // Y window, 9 bits each
            if (n == 2) yStart_ = (args_[0] | (args_[1] & 1) << 8);
            if (n == 4) yEnd_ = (args_[2] | (args_[3] & 1) << 8);
            break;
        case 0x4E:
            if (n == 1) xCount_ = d & 0x3f;
            break;
        case 0x4F:
            if (n == 2) yCount_ = (args_[0] | (args_[1] & 1) << 8);
            break;
        default:
            break;
    }
}

void Ssd1683::ramWrite(uint8_t* ram, uint8_t d) {
    if (xCount_ >= 0 && xCount_ < kStride && yCount_ >= 0 && yCount_ < kHeight)
        ram[yCount_ * kStride + xCount_] = d;
    advance();
}

// Moves the address counter per the data entry mode: bit 0 = X increments,
// bit 1 = Y increments, bit 2 = Y is the fast axis. A counter that runs past
// the window end wraps to the window start and steps the other axis.
void Ssd1683::advance() {
    bool xInc = entryMode_ & 1, yInc = entryMode_ & 2, yFirst = entryMode_ & 4;
    auto stepX = [&]() {
        xCount_ += xInc ? 1 : -1;
        if (xInc ? xCount_ > xEnd_ : xCount_ < xEnd_) { xCount_ = xStart_; return true; }
        return false;
    };
    auto stepY = [&]() {
        yCount_ += yInc ? 1 : -1;
        if (yInc ? yCount_ > yEnd_ : yCount_ < yEnd_) { yCount_ = yStart_; return true; }
        return false;
    };
    if (yFirst) { if (stepY()) stepX(); }
    else if (stepX()) stepY();
}

// 0x20 runs the sequence chosen with 0x22: bit 7 clock on, bit 6 analog on,
// bit 2 show the RAM (bit 3 = display mode 2, the fast partial refresh),
// bit 1 analog off, bit 0 clock off.
void Ssd1683::activate(uint32_t nowMs) {
    uint8_t s = updateSeq_;
    uint32_t ms = 0;
    if ((s & 0x40) && !analogOn_) ms += kPowerOnMs;
    if (s & 0x40) analogOn_ = true;
    if (s & 0x04) {
        Refresh kind = lutLoaded_ ? Refresh::Gray : (s & 0x08) ? Refresh::Partial : Refresh::Full;
        uint32_t t = kind == Refresh::Gray ? kGrayMs : kind == Refresh::Full ? kFullMs : kPartialMs;
        // a power-on in the same sequence is part of the refresh time GxEPD2 expects
        ms = t;
        refreshes_++;
        if (kind == Refresh::Gray) {
            for (int y = 0; y < kHeight; y++)
                for (int x = 0; x < kWidth; x++) glass_[y * kWidth + x] = level(x, y);
        } else if (kind == Refresh::Full) {
            for (int y = 0; y < kHeight; y++)
                for (int x = 0; x < kWidth; x++) glass_[y * kWidth + x] = white(x, y) ? 3 : 0;
        } else {   // only what changed against the previous picture, in the window
            for (int y = yStart_; y <= yEnd_ && y < kHeight; y++)
                for (int x = xStart_ * 8; x < (xEnd_ + 1) * 8 && x < kWidth; x++) {
                    uint8_t m = 0x80 >> (x & 7);
                    int i = y * kStride + x / 8;
                    if ((bw_[i] ^ red_[i]) & m) glass_[y * kWidth + x] = (bw_[i] & m) ? 3 : 0;
                }
        }
        if (onRefresh) onRefresh(glass_, kind, uint32_t(float(t) * timeScale));
    }
    if (s & 0x02) {
        if (!(s & 0x04)) ms += kPowerOffMs;
        analogOn_ = false;
    }
    holdBusy(ms, nowMs);
}

}  // namespace epdsim
