#pragma once
#include <Arduino.h>

enum class Btn : uint8_t { None, Prev, Next, Menu, Ok, Back };

struct ButtonEvent {
    Btn button = Btn::None;
    bool longPress = false;
};

class Buttons {
public:
    void begin();
    ButtonEvent poll();              // call often; returns at most one event
    bool anyDown() const;
    static uint64_t wakeMask();      // bitmask of all button GPIOs for ext1 wake-up
    static Btn fromWakeMask(uint64_t mask);
    void inject(Btn b) { injected_ = b; }   // e.g. the button that woke us up

private:
    struct State {
        uint8_t pin;
        Btn id;
        bool down = false;
        bool longSent = false;
        uint32_t changedAt = 0;
    };
    State s_[5];
    Btn injected_ = Btn::None;
};
