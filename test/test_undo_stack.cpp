/**
 * @file test_undo_stack.cpp
 * @brief Unit tests for tcommand::UndoStack.
 */
#include <memory>

#include <gtest/gtest.h>

#include "tcommand/ICommand.hpp"
#include "tcommand/UndoStack.hpp"

namespace {

/// Minimal fake command tagged with an id for verifying LIFO order.
class FakeCommand final : public tcommand::ICommand {
 public:
    explicit FakeCommand(int id) : id_{id} {}

    tcommand::CommandStatus execute() override {
        return tcommand::CommandStatus::Success;
    }
    tcommand::CommandStatus undo() override {
        return tcommand::CommandStatus::Success;
    }
    std::string_view name() const noexcept override { return "Fake"; }

    int id() const noexcept { return id_; }

 private:
    int id_;
};

}  // namespace

TEST(UndoStackTest, LifoOrder) {
    tcommand::UndoStack s{8};

    s.push(std::make_unique<FakeCommand>(1));
    s.push(std::make_unique<FakeCommand>(2));
    s.push(std::make_unique<FakeCommand>(3));

    auto first  = s.pop();  // expect id=3
    auto second = s.pop();  // expect id=2
    auto third  = s.pop();  // expect id=1

    ASSERT_NE(first,  nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(third,  nullptr);

    EXPECT_EQ(dynamic_cast<FakeCommand*>(first.get())->id(),  3);
    EXPECT_EQ(dynamic_cast<FakeCommand*>(second.get())->id(), 2);
    EXPECT_EQ(dynamic_cast<FakeCommand*>(third.get())->id(),  1);

    EXPECT_EQ(s.pop(), nullptr);  // now empty
}

TEST(UndoStackTest, BoundedDropsOldest) {
    tcommand::UndoStack s{2};

    s.push(std::make_unique<FakeCommand>(1));
    s.push(std::make_unique<FakeCommand>(2));
    s.push(std::make_unique<FakeCommand>(3));  // should drop id=1

    EXPECT_EQ(s.size(), 2u);

    auto top  = s.pop();  // expect id=3
    auto next = s.pop();  // expect id=2

    EXPECT_EQ(dynamic_cast<FakeCommand*>(top.get())->id(),  3);
    EXPECT_EQ(dynamic_cast<FakeCommand*>(next.get())->id(), 2);
    EXPECT_TRUE(s.empty());
}

TEST(UndoStackTest, PopOnEmptyReturnsNull) {
    tcommand::UndoStack s{4};

    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.pop(), nullptr);
}

TEST(UndoStackTest, CapacityReportsConfiguredValue) {
    tcommand::UndoStack s{16};
    EXPECT_EQ(s.capacity(), 16u);
}