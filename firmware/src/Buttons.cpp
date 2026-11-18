#include "Buttons.h"

#include "pins.h"

static constexpr uint32_t DEBOUNCE_MS = 25;
static constexpr uint32_t LONG_MS = 700;

static const struct { uint8_t pin; Btn id; } kMap[5] = {
    {PIN_BTN_PREV, Btn::Prev}, {PIN_BTN_NEXT, Btn::Next}, {PIN_BTN_MENU, Btn::Menu},
    {PIN_BTN_OK, Btn::Ok},     {PIN_BTN_BACK, Btn::Back},
};

void Buttons::begin() {
    for (int i = 0; i < 5; i++) {
        s_[i].pin = kMap[i].pin;
        s_[i].id = kMap[i].id;
        pinMode(s_[i].pin, INPUT_PULLUP);
        // a button still held from the wake-up press must not fire again
        s_[i].down = digitalRead(s_[i].pin) == LOW;
        s_[i].longSent = s_[i].down;
        s_[i].changedAt = millis();
    }
}

bool Buttons::anyDown() const {
    for (auto& s : s_)
        if (s.down) return true;
    return false;
}

ButtonEvent Buttons::poll() {
    if (injected_ != Btn::None) {
        ButtonEvent e{injected_, false};
        injected_ = Btn::None;
        return e;
    }
    uint32_t now = millis();
    for (auto& s : s_) {
        bool raw = digitalRead(s.pin) == LOW;
        if (raw != s.down && now - s.changedAt >= DEBOUNCE_MS) {
            s.down = raw;
            s.changedAt = now;
            if (!raw) {                       // released
                bool wasLong = s.longSent;
                s.longSent = false;
                if (!wasLong) return {s.id, false};
            }
        } else if (s.down && !s.longSent && now - s.changedAt >= LONG_MS) {
            s.longSent = true;
            return {s.id, true};
        }
    }
    return {};
}

uint64_t Buttons::wakeMask() {
    uint64_t m = 0;
    for (auto& k : kMap) m |= 1ULL << k.pin;
    return m;
}

Btn Buttons::fromWakeMask(uint64_t mask) {
    for (auto& k : kMap)
        if (mask & (1ULL << k.pin)) return k.id;
    return Btn::None;
}
