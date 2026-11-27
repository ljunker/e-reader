// The hardware under the firmware: clock, GPIO, the SPI bus with the e-paper
// panel on it, the serial console, I2C with the battery monitor, deep sleep.
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <emscripten.h>
#include <esp_sleep.h>

#include "FuelGauge.h"
#include "Sim.h"
#include "Ssd1683.h"
#include "pins.h"

// ---------------------------------------------------------------- clock

static double bootAt = emscripten_get_now();
static double skipped = 0;        // "+3 min" in the page
static double lastYield = 0;

uint32_t sim::nowMs() { return uint32_t(emscripten_get_now() - bootAt + skipped); }

unsigned long millis() { return sim::nowMs(); }
unsigned long micros() { return (unsigned long)((emscripten_get_now() - bootAt + skipped) * 1000.0); }

void delay(uint32_t ms) {
    emscripten_sleep(ms);
    lastYield = emscripten_get_now();
    if (js_power_cut()) emscripten_force_exit(0);   // "Reset" in the page
}

void delayMicroseconds(uint32_t us) { (void)us; }

// Long loops call yield() to feed the watchdog; here it lets the page breathe.
void yield() {
    double now = emscripten_get_now();
    if (now - lastYield > 30) delay(0);
}

void* ps_malloc(size_t size) { return malloc(size); }

// newlib has these; musl does not (stdlib_noniso.c brings the long versions)
extern "C" char* ltoa(long value, char* result, int base);
extern "C" char* ultoa(unsigned long value, char* result, int base);
extern "C" char* itoa(int value, char* result, int base) { return ltoa(value, result, base); }
extern "C" char* utoa(unsigned value, char* result, int base) { return ultoa(value, result, base); }

const char* pathToFileName(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

// ---------------------------------------------------------------- panel

static epdsim::Ssd1683 panel;

// ---------------------------------------------------------------- GPIO

namespace {
struct Pin {
    uint8_t mode = INPUT;
    uint8_t out = LOW;
    int8_t ext = -1;   // driven from outside (a pressed button), -1 = not
};
Pin pins[49];

int level(int p) {
    if (p < 0 || p >= 49) return LOW;
    if (p == PIN_EPD_BUSY) return panel.busy(sim::nowMs()) ? HIGH : LOW;
    if (p == PIN_VBUS_SENSE) return sim::usbPowered() ? HIGH : LOW;
    const Pin& s = pins[p];
    if (s.mode == OUTPUT) return s.out;
    if (s.ext >= 0) return s.ext;
    if (s.mode & PULLUP) return HIGH;
    return LOW;
}

bool panelPowered() { return PIN_EPD_PWR < 0 || level(PIN_EPD_PWR) == HIGH; }

int lastRst = HIGH, lastPwr = LOW;

// Reacts to level changes on the lines that do something by themselves.
void pinsChanged() {
    if (PIN_EPD_PWR >= 0) {
        int pwr = level(PIN_EPD_PWR);
        if (pwr != lastPwr) {
            lastPwr = pwr;
            if (pwr == LOW) panel.powerLoss();
            js_led(pwr == HIGH);   // display power indicator in the page
        }
    }
    int rst = level(PIN_EPD_RST);
    if (rst != lastRst) {
        lastRst = rst;
        if (rst == LOW && panelPowered()) panel.hardwareReset(sim::nowMs());
    }
}
}  // namespace

void pinMode(uint8_t p, uint8_t mode) {
    if (p >= 49) return;
    pins[p].mode = mode;
    pinsChanged();
}

void digitalWrite(uint8_t p, uint8_t val) {
    if (p >= 49) return;
    pins[p].out = val ? HIGH : LOW;
    pinsChanged();
}

int digitalRead(uint8_t p) { return level(p); }

// level: 0 or 1 to drive the pin from outside, -1 to let go.
extern "C" EMSCRIPTEN_KEEPALIVE void sim_set_input(int p, int lvl) {
    if (p >= 0 && p < 49) pins[p].ext = int8_t(lvl);
}

// PREV, NEXT, MENU, OK, BACK
extern "C" EMSCRIPTEN_KEEPALIVE int sim_button_pin(int i) {
    static const int pinsOf[5] = {PIN_BTN_PREV, PIN_BTN_NEXT, PIN_BTN_MENU, PIN_BTN_OK, PIN_BTN_BACK};
    return i >= 0 && i < 5 ? pinsOf[i] : -1;
}

extern "C" EMSCRIPTEN_KEEPALIVE void sim_set_time_scale(float scale) { panel.timeScale = scale; }

extern "C" EMSCRIPTEN_KEEPALIVE void sim_time_skip(double ms) { skipped += ms; }

// ---------------------------------------------------------------- SPI

SPIClass SPI;

uint8_t SPIClass::transfer(uint8_t data) {
    if (level(PIN_EPD_CS) == LOW && panelPowered())
        panel.write(level(PIN_EPD_DC) == HIGH, data, sim::nowMs());
    return 0;
}

void SPIClass::transfer(void* data, uint32_t size) {
    uint8_t* p = static_cast<uint8_t*>(data);
    for (uint32_t i = 0; i < size; i++) p[i] = transfer(p[i]);
}

static struct PanelHook {
    PanelHook() {
        panel.onRefresh = [](const uint8_t* frame, epdsim::Refresh kind, uint32_t ms) {
            using P = epdsim::Ssd1683;
            js_panel_refresh(frame, P::kWidth * P::kHeight,
                             kind == epdsim::Refresh::Gray ? 2 : kind == epdsim::Refresh::Full ? 0 : 1, int(ms));
        };
    }
} panelHook;

// ---------------------------------------------------------------- serial

SimSerial Serial;
static char lineBuf[512];
static size_t lineLen = 0;

size_t SimSerial::write(uint8_t c) {
    if (c == '\r') return 1;
    if (c == '\n' || lineLen == sizeof lineBuf) {
        js_serial(lineBuf, int(lineLen));
        lineLen = 0;
        if (c == '\n') return 1;
    }
    lineBuf[lineLen++] = char(c);
    return 1;
}

size_t SimSerial::write(const uint8_t* buf, size_t n) {
    for (size_t i = 0; i < n; i++) write(buf[i]);
    return n;
}

void SimSerial::flush() {
    if (lineLen) {
        js_serial(lineBuf, int(lineLen));
        lineLen = 0;
    }
}

extern "C" void sim_log(char lvl, const char* fmt, ...) {
    char buf[400];
    int n = snprintf(buf, sizeof buf, "[%c] ", lvl);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf + n, sizeof buf - n, fmt, ap);
    va_end(ap);
    Serial.println(buf);
}

// ---------------------------------------------------------------- I2C + MAX17048

TwoWire Wire;
static bool gaugeFitted = true;
static int gaugePercent = 80;
static int gaugeMv = 3950;

extern "C" EMSCRIPTEN_KEEPALIVE void sim_set_battery(int fitted, int percent, int mv) {
    gaugeFitted = fitted;
    gaugePercent = percent;
    gaugeMv = mv;
}

// The gauge sits on the always-on LDO1 of the FeatherS3[D].
static bool gaugeAnswers(uint8_t addr) {
    return gaugeFitted && addr == fuelgauge::kMax17048Addr;
}

static uint16_t gaugeRegister(uint8_t reg) {
    switch (reg) {
        case fuelgauge::kMaxRegVcell: return uint16_t(double(gaugeMv) * 1000.0 / 78.125);
        case fuelgauge::kMaxRegSoc: return uint16_t(gaugePercent << 8);
        case 0x08: return 0x0012;   // VERSION
        default: return 0;
    }
}

void TwoWire::beginTransmission(uint8_t address) {
    addr_ = address;
    txLen_ = 0;
}

size_t TwoWire::write(uint8_t data) {
    if (txLen_ < sizeof tx_) tx_[txLen_++] = data;
    return 1;
}

size_t TwoWire::write(const uint8_t* data, size_t n) {
    for (size_t i = 0; i < n; i++) write(data[i]);
    return n;
}

uint8_t TwoWire::endTransmission(bool sendStop) {
    (void)sendStop;
    return gaugeAnswers(addr_) ? 0 : 2;   // 2 = address NACK
}

uint8_t TwoWire::requestFrom(uint8_t address, size_t len, bool stopBit) {
    (void)stopBit;
    rxLen_ = rxPos_ = 0;
    if (!gaugeAnswers(address) || txLen_ == 0) return 0;
    uint16_t v = gaugeRegister(tx_[0]);
    uint8_t bytes[2] = {uint8_t(v >> 8), uint8_t(v)};
    for (size_t i = 0; i < len && i < sizeof rx_; i++) rx_[rxLen_++] = bytes[i & 1];
    return uint8_t(rxLen_);
}

int TwoWire::available() { return int(rxLen_ - rxPos_); }
int TwoWire::read() { return rxPos_ < rxLen_ ? rx_[rxPos_++] : -1; }
int TwoWire::peek() { return rxPos_ < rxLen_ ? rx_[rxPos_] : -1; }

// ---------------------------------------------------------------- deep sleep

extern "C" char __start_rtc_data[], __stop_rtc_data[];
static uint64_t wakeMask = 0;

esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() { return esp_sleep_wakeup_cause_t(js_boot_wake_cause()); }
uint64_t esp_sleep_get_ext1_wakeup_status() { return js_boot_ext1_lo(); }
esp_err_t esp_sleep_pd_config(esp_sleep_pd_domain_t, esp_sleep_pd_option_t) { return 0; }

esp_err_t esp_sleep_enable_ext1_wakeup(uint64_t mask, esp_sleep_ext1_wakeup_mode_t mode) {
    (void)mode;
    wakeMask = mask;
    return 0;
}

// The page keeps the RTC memory and starts a fresh instance of the firmware
// when a wake-up button goes down, like the chip's reset after deep sleep.
void esp_deep_sleep_start() {
    Serial.flush();
    js_deep_sleep(reinterpret_cast<const uint8_t*>(__start_rtc_data), int(__stop_rtc_data - __start_rtc_data),
                  uint32_t(wakeMask), uint32_t(wakeMask >> 32));
    emscripten_force_exit(0);
    for (;;) emscripten_sleep(1000);
}

// Called once before setup(): RTC memory survives only a deep-sleep reset.
void simRestoreRtc() {
    js_boot_rtc(reinterpret_cast<uint8_t*>(__start_rtc_data), int(__stop_rtc_data - __start_rtc_data));
}
