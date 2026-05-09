/**
 * @file test_mock_hal.cpp
 * @brief Unit tests for tcommand::hal::MockHal.
 *
 * Validates the in-memory register bank, failure injection, and time
 * primitives. These are the foundations every higher-level test relies on.
 */
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include "hal/MockHal.hpp"

namespace {

constexpr uint32_t kTestReg = 0x40000000;

}  // namespace

TEST(MockHalTest, WriteThenReadReturnsValue) {
    tcommand::hal::MockHal hal{0.0};

    EXPECT_TRUE(hal.writeRegister(kTestReg, 0xDEADBEEF));
    EXPECT_EQ(hal.readRegister(kTestReg), 0xDEADBEEFu);
}

TEST(MockHalTest, ReadOfUnwrittenAddressReturnsZero) {
    tcommand::hal::MockHal hal{0.0};

    EXPECT_EQ(hal.readRegister(0x99999999), 0u);
}

TEST(MockHalTest, InjectedFailuresMakeWriteReturnFalse) {
    tcommand::hal::MockHal hal{0.0};

    hal.injectWriteFailures(2);

    // First two writes fail
    EXPECT_FALSE(hal.writeRegister(kTestReg, 1));
    EXPECT_FALSE(hal.writeRegister(kTestReg, 2));

    // Third one succeeds
    EXPECT_TRUE(hal.writeRegister(kTestReg, 3));
    EXPECT_EQ(hal.readRegister(kTestReg), 3u);

    EXPECT_EQ(hal.failedWriteCount(), 2u);
}

TEST(MockHalTest, WriteCountTracksAllAttempts) {
    tcommand::hal::MockHal hal{0.0};

    hal.injectWriteFailures(1);
    hal.writeRegister(kTestReg, 1);  // fails
    hal.writeRegister(kTestReg, 2);  // succeeds
    hal.writeRegister(kTestReg, 3);  // succeeds

    EXPECT_EQ(hal.writeCount(), 3u);
    EXPECT_EQ(hal.failedWriteCount(), 1u);
}

TEST(MockHalTest, MillisAdvancesOverTime) {
    tcommand::hal::MockHal hal{1.0};

    const auto t0 = hal.millis();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const auto t1 = hal.millis();

    EXPECT_GE(t1, t0 + 5);  // at least ~5ms passed
}

TEST(MockHalTest, DelayMsRespectsTimeScale) {
    tcommand::hal::MockHal hal{0.0};  // delays disabled

    const auto t0 = std::chrono::steady_clock::now();
    hal.delayMs(1000);  // would normally sleep 1 second
    const auto t1 = std::chrono::steady_clock::now();

    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    // With time_scale=0, delay should be effectively instant
    EXPECT_LT(elapsed_ms, 50);
}