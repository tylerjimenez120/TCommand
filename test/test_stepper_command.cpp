/**
 * @file test_stepper_command.cpp
 * @brief Unit tests for concrete commands (StepperMoveCommand, RelaySetCommand).
 */
#include <gtest/gtest.h>

#include "commands/RelaySetCommand.hpp"
#include "commands/StepperMoveCommand.hpp"
#include "hal/MockHal.hpp"

using tcommand::CommandStatus;
using tcommand::commands::RelaySetCommand;
using tcommand::commands::StepperMoveCommand;

TEST(StepperMoveCommandTest, ExecuteWritesTargetAndEnable) {
    tcommand::hal::MockHal hal{0.0};

    // Pre-set current position to 10
    hal.writeRegister(tcommand::commands::kStepperCurrentReg, 10);

    StepperMoveCommand cmd{hal, 100};
    EXPECT_EQ(cmd.execute(), CommandStatus::Success);

    // Verify target and enable were written
    EXPECT_EQ(hal.readRegister(tcommand::commands::kStepperTargetReg), 100u);
    EXPECT_EQ(hal.readRegister(tcommand::commands::kStepperControlReg),
              tcommand::commands::kStepperEnableBit);
}

TEST(StepperMoveCommandTest, UndoRestoresPreviousPosition) {
    tcommand::hal::MockHal hal{0.0};

    hal.writeRegister(tcommand::commands::kStepperCurrentReg, 50);

    StepperMoveCommand cmd{hal, 200};
    ASSERT_EQ(cmd.execute(), CommandStatus::Success);

    // After execute, target should be 200
    EXPECT_EQ(hal.readRegister(tcommand::commands::kStepperTargetReg), 200u);

    EXPECT_EQ(cmd.undo(), CommandStatus::Success);

    // After undo, target should be restored to 50
    EXPECT_EQ(hal.readRegister(tcommand::commands::kStepperTargetReg), 50u);
}

TEST(StepperMoveCommandTest, UndoFailsIfNotExecuted) {
    tcommand::hal::MockHal hal{0.0};
    StepperMoveCommand cmd{hal, 100};

    // Calling undo before execute is a contract violation
    EXPECT_EQ(cmd.undo(), CommandStatus::PermanentFailure);
}

TEST(RelaySetCommandTest, ExecuteTurnsOn) {
    tcommand::hal::MockHal hal{0.0};

    // Initially off
    hal.writeRegister(tcommand::commands::kRelayStateReg, 0);

    RelaySetCommand cmd{hal, true};
    EXPECT_EQ(cmd.execute(), CommandStatus::Success);

    EXPECT_EQ(hal.readRegister(tcommand::commands::kRelayStateReg), 1u);
}

TEST(RelaySetCommandTest, ExecuteTurnsOff) {
    tcommand::hal::MockHal hal{0.0};

    // Initially on
    hal.writeRegister(tcommand::commands::kRelayStateReg, 1);

    RelaySetCommand cmd{hal, false};
    EXPECT_EQ(cmd.execute(), CommandStatus::Success);

    EXPECT_EQ(hal.readRegister(tcommand::commands::kRelayStateReg), 0u);
}

TEST(RelaySetCommandTest, UndoRestoresPreviousState) {
    tcommand::hal::MockHal hal{0.0};

    // Start with relay on
    hal.writeRegister(tcommand::commands::kRelayStateReg, 1);

    // Turn it off
    RelaySetCommand cmd{hal, false};
    ASSERT_EQ(cmd.execute(), CommandStatus::Success);
    EXPECT_EQ(hal.readRegister(tcommand::commands::kRelayStateReg), 0u);

    // Undo should restore to on
    EXPECT_EQ(cmd.undo(), CommandStatus::Success);
    EXPECT_EQ(hal.readRegister(tcommand::commands::kRelayStateReg), 1u);
}