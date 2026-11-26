#include <unity.h>

#include <string.h>

#include <vector>

#include "Ssd1683.h"

using epdsim::Refresh;
using epdsim::Ssd1683;

static void cmd(Ssd1683& p, uint8_t c, std::initializer_list<uint8_t> data = {}, uint32_t t = 0) {
    p.write(false, c, t);
    for (uint8_t d : data) p.write(true, d, t);
}

// The sequence GxEPD2_420_GDEY042T81::_setPartialRamArea sends.
static void window(Ssd1683& p, int x, int y, int w, int h) {
    cmd(p, 0x11, {0x03});
    cmd(p, 0x44, {uint8_t(x / 8), uint8_t((x + w - 1) / 8)});
    cmd(p, 0x45, {uint8_t(y % 256), uint8_t(y / 256), uint8_t((y + h - 1) % 256), uint8_t((y + h - 1) / 256)});
    cmd(p, 0x4e, {uint8_t(x / 8)});
    cmd(p, 0x4f, {uint8_t(y % 256), uint8_t(y / 256)});
}

static void fill(Ssd1683& p, uint8_t ram, int bytes, uint8_t value) {
    p.write(false, ram, 0);
    for (int i = 0; i < bytes; i++) p.write(true, value, 0);
}

void test_full_screen_write() {
    Ssd1683 p;
    cmd(p, 0x12);
    window(p, 0, 0, 400, 300);
    fill(p, 0x24, Ssd1683::kStride * Ssd1683::kHeight, 0x00);
    TEST_ASSERT_FALSE(p.white(0, 0));
    TEST_ASSERT_FALSE(p.white(399, 299));
    TEST_ASSERT_EQUAL(0, p.unknownCommands());
}

void test_partial_window_wraps_rows() {
    Ssd1683 p;
    window(p, 0, 0, 400, 300);
    fill(p, 0x24, Ssd1683::kStride * Ssd1683::kHeight, 0xff);
    // 16 × 3 pixel window at (80, 100): 2 bytes per row, 3 rows
    window(p, 80, 100, 16, 3);
    p.write(false, 0x24, 0);
    for (uint8_t d : {0x00, 0xff, 0xff, 0x00, 0x0f, 0xf0}) p.write(true, d, 0);
    TEST_ASSERT_FALSE(p.white(80, 100));
    TEST_ASSERT_FALSE(p.white(87, 100));
    TEST_ASSERT_TRUE(p.white(88, 100));
    TEST_ASSERT_TRUE(p.white(80, 101));
    TEST_ASSERT_FALSE(p.white(88, 101));
    TEST_ASSERT_FALSE(p.white(80, 102));
    TEST_ASSERT_TRUE(p.white(84, 102));
    TEST_ASSERT_TRUE(p.white(88, 102));
    TEST_ASSERT_FALSE(p.white(95, 102));
    // outside the window untouched
    TEST_ASSERT_TRUE(p.white(96, 100));
    TEST_ASSERT_TRUE(p.white(80, 103));
    // a fourth row wraps back to the window start
    for (uint8_t d : {0xaa, 0xaa}) p.write(true, d, 0);
    TEST_ASSERT_FALSE(p.white(81, 100));
    TEST_ASSERT_TRUE(p.white(80, 100));
}

void test_red_ram_separate() {
    Ssd1683 p;
    window(p, 0, 0, 400, 300);
    fill(p, 0x26, 10, 0x00);
    TEST_ASSERT_TRUE(p.white(0, 0));
    TEST_ASSERT_EQUAL_HEX8(0x00, p.redRam()[0]);
}

void test_refresh_kinds_and_busy() {
    Ssd1683 p;
    std::vector<Refresh> seen;
    std::vector<uint32_t> times;
    p.onRefresh = [&](const uint8_t*, Refresh k, uint32_t ms) { seen.push_back(k); times.push_back(ms); };

    // _PowerOn
    cmd(p, 0x22, {0xe0}, 1000);
    cmd(p, 0x20, {}, 1000);
    TEST_ASSERT_TRUE(p.busy(1050));
    TEST_ASSERT_FALSE(p.busy(1100));
    TEST_ASSERT_TRUE(p.poweredOn());

    // _Update_Full (fast variant)
    cmd(p, 0x21, {0x40, 0x00}, 2000);
    cmd(p, 0x1a, {0x6e}, 2000);
    cmd(p, 0x22, {0xd7}, 2000);
    cmd(p, 0x20, {}, 2000);
    TEST_ASSERT_TRUE(p.busy(3000));
    TEST_ASSERT_FALSE(p.busy(3200));
    TEST_ASSERT_FALSE(p.poweredOn());

    // _Update_Part
    cmd(p, 0x21, {0x00, 0x00}, 5000);
    cmd(p, 0x22, {0xfc}, 5000);
    cmd(p, 0x20, {}, 5000);
    TEST_ASSERT_TRUE(p.busy(5399));
    TEST_ASSERT_FALSE(p.busy(5400));
    TEST_ASSERT_TRUE(p.poweredOn());

    TEST_ASSERT_EQUAL(2, int(seen.size()));
    TEST_ASSERT_TRUE(seen[0] == Refresh::Full);
    TEST_ASSERT_TRUE(seen[1] == Refresh::Partial);
    TEST_ASSERT_EQUAL_UINT32(1200, times[0]);
    TEST_ASSERT_EQUAL_UINT32(400, times[1]);
    TEST_ASSERT_EQUAL_UINT32(2, p.refreshCount());
}

void test_time_scale_zero_is_instant() {
    Ssd1683 p;
    p.timeScale = 0;
    cmd(p, 0x22, {0xf7}, 10);
    cmd(p, 0x20, {}, 10);
    TEST_ASSERT_FALSE(p.busy(10));
}

void test_deep_sleep_until_reset() {
    Ssd1683 p;
    window(p, 0, 0, 400, 300);
    fill(p, 0x24, 1, 0x00);
    cmd(p, 0x10, {0x01});
    TEST_ASSERT_TRUE(p.sleeping());
    window(p, 0, 0, 400, 300);
    fill(p, 0x24, 1, 0xff);            // ignored while asleep
    TEST_ASSERT_FALSE(p.white(0, 0));
    p.hardwareReset(0);
    TEST_ASSERT_FALSE(p.sleeping());
    TEST_ASSERT_FALSE(p.white(0, 0));  // RAM survives deep sleep mode 1
    p.powerLoss();
    TEST_ASSERT_TRUE(p.white(0, 0));
}

void test_y_decrement_mode() {
    Ssd1683 p;
    cmd(p, 0x11, {0x01});   // x increments, y decrements
    cmd(p, 0x44, {0, 0});
    cmd(p, 0x45, {9, 0, 8, 0});
    cmd(p, 0x4e, {0});
    cmd(p, 0x4f, {9, 0});
    fill(p, 0x24, 2, 0x00);
    TEST_ASSERT_FALSE(p.white(0, 9));
    TEST_ASSERT_FALSE(p.white(0, 8));
    TEST_ASSERT_TRUE(p.white(0, 10));
    TEST_ASSERT_TRUE(p.white(0, 7));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_full_screen_write);
    RUN_TEST(test_partial_window_wraps_rows);
    RUN_TEST(test_red_ram_separate);
    RUN_TEST(test_refresh_kinds_and_busy);
    RUN_TEST(test_time_scale_zero_is_instant);
    RUN_TEST(test_deep_sleep_until_reset);
    RUN_TEST(test_y_decrement_mode);
    return UNITY_END();
}
