/**
 * @file test_command_queue.cpp
 * @brief Unit tests for tcommand::CommandQueue.
 */
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "tcommand/CommandQueue.hpp"
#include "tcommand/ICommand.hpp"

namespace {

/// Minimal fake command used to verify queue ordering by id.
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

TEST(CommandQueueTest, PushPopFifoOrder) {
    tcommand::CommandQueue q{8};

    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(q.tryPush(std::make_unique<FakeCommand>(i)));
    }
    EXPECT_EQ(q.size(), 4u);

    for (int i = 0; i < 4; ++i) {
        auto cmd = q.pop();
        ASSERT_NE(cmd, nullptr);
        auto* fake = dynamic_cast<FakeCommand*>(cmd.get());
        ASSERT_NE(fake, nullptr);
        EXPECT_EQ(fake->id(), i);
    }
}

TEST(CommandQueueTest, TryPushFailsWhenFull) {
    tcommand::CommandQueue q{2};

    ASSERT_TRUE(q.tryPush(std::make_unique<FakeCommand>(1)));
    ASSERT_TRUE(q.tryPush(std::make_unique<FakeCommand>(2)));
    EXPECT_FALSE(q.tryPush(std::make_unique<FakeCommand>(3)));
    EXPECT_EQ(q.size(), 2u);
}

TEST(CommandQueueTest, ShutdownReleasesBlockedPop) {
    tcommand::CommandQueue q{4};
    std::atomic<bool> popped{false};

    std::thread consumer([&] {
        auto cmd = q.pop();
        EXPECT_EQ(cmd, nullptr);  // null on shutdown with empty queue
        popped.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(popped.load());

    q.shutdown();
    consumer.join();
    EXPECT_TRUE(popped.load());
}

TEST(CommandQueueTest, MultiProducerSingleConsumer) {
    constexpr int kProducers = 4;
    constexpr int kPerProducer = 250;

    tcommand::CommandQueue q{64};
    std::atomic<int> consumed{0};

    std::thread consumer([&] {
        while (true) {
            auto cmd = q.pop();
            if (!cmd) break;
            consumed.fetch_add(1);
        }
    });

    std::vector<std::thread> producers;
    producers.reserve(kProducers);
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < kPerProducer; ++i) {
                q.push(std::make_unique<FakeCommand>(p * 1000 + i));
            }
        });
    }
    for (auto& t : producers) t.join();

    // Wait for drain
    while (q.size() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    q.shutdown();
    consumer.join();

    EXPECT_EQ(consumed.load(), kProducers * kPerProducer);
}