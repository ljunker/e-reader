#include <unity.h>

#include "FuelGauge.h"

void setUp() {}
void tearDown() {}

void test_crc8_smbus_check_value() {
    const uint8_t s[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    TEST_ASSERT_EQUAL_HEX8(0xF4, fuelgauge::crc8(s, sizeof s));
    TEST_ASSERT_EQUAL_HEX8(0x00, fuelgauge::crc8(s, 0));
}

void test_max17048_voltage() {
    TEST_ASSERT_EQUAL(3900, fuelgauge::max17048Millivolts(49920));   // 49920 × 78.125 µV
    TEST_ASSERT_EQUAL(0, fuelgauge::max17048Millivolts(0));
    TEST_ASSERT_EQUAL(5120, fuelgauge::max17048Millivolts(0xFFFF));   // no overflow
}

void test_max17048_percent() {
    TEST_ASSERT_EQUAL(50, fuelgauge::max17048Percent(0x3200));
    TEST_ASSERT_EQUAL(51, fuelgauge::max17048Percent(0x3280));   // 50.5 rounds up
    TEST_ASSERT_EQUAL(100, fuelgauge::max17048Percent(0x6680));  // 102.5 is clamped
}

void test_linear_percent() {
    TEST_ASSERT_EQUAL(0, fuelgauge::linearPercent(3000, 3300, 4150));
    TEST_ASSERT_EQUAL(50, fuelgauge::linearPercent(3725, 3300, 4150));
    TEST_ASSERT_EQUAL(100, fuelgauge::linearPercent(4200, 3300, 4150));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_crc8_smbus_check_value);
    RUN_TEST(test_max17048_voltage);
    RUN_TEST(test_max17048_percent);
    RUN_TEST(test_linear_percent);
    return UNITY_END();
}
