#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

#include "support/bounded_wait.h"
#include "support/test_observer.h"
#include "support/test_scheduler.h"

#include <rx/rx.h>

#include <chrono>
#include <thread>
#include <vector>

namespace
{
using namespace rx;
using namespace rx::test;

TEST(TestObserverTest, RecordsValuesAndTerminalState)
{
    const auto observer = std::make_shared<TestObserver>();

    Observable::just(1, 2, 3)->subscribe(observer);

    observer->expectInt64Values({1, 2, 3});
    observer->expectComplete();
    const auto events = observer->events();
    ASSERT_EQ(events.size(), 5u) << observer->describe();
    EXPECT_EQ(events.front().type, ObserverEventType::Subscribe);
    EXPECT_EQ(events.back().type, ObserverEventType::Complete);
}

TEST(TestObserverTest, RecordsErrorWithoutCompletion)
{
    const auto observer = std::make_shared<TestObserver>();

    Observable::error(GAnyException("expected failure"))->subscribe(observer);

    observer->expectInt64Values({});
    observer->expectErrorContains("expected failure");
}

TEST(TestObserverTest, DistinguishesEmptyErrorFromNoError)
{
    const auto observer = std::make_shared<TestObserver>();

    Observable::error(GAnyException(""))->subscribe(observer);

    EXPECT_TRUE(observer->hasError()) << observer->describe();
    observer->expectErrorContains("");
}

TEST(TestObserverTest, RejectsRepeatedErrorSignals)
{
    const auto observer = std::make_shared<TestObserver>();

    observer->onError(GAnyException("repeated failure"));
    observer->onError(GAnyException("repeated failure"));

    EXPECT_NONFATAL_FAILURE(
        observer->expectErrorContains("repeated failure"),
        "error signals=2");
}

TEST(TestObserverTest, RejectsNextAfterCompletion)
{
    const auto observer = std::make_shared<TestObserver>();

    observer->onComplete();
    observer->onNext(1);

    EXPECT_NONFATAL_FAILURE(
        observer->expectComplete(),
        "signals after termination=1");
}

TEST(TestObserverTest, RejectsNextAfterError)
{
    const auto observer = std::make_shared<TestObserver>();

    observer->onError(GAnyException("expected failure"));
    observer->onNext(1);

    EXPECT_NONFATAL_FAILURE(
        observer->expectErrorContains("expected failure"),
        "signals after termination=1");
}

TEST(TestObserverTest, WaitsForTerminalSignalWithBoundedTimeout)
{
    const auto observer = std::make_shared<TestObserver>();
    std::thread completer([observer] { observer->onComplete(); });

    EXPECT_TRUE(observer->awaitTerminal(std::chrono::milliseconds(500)))
        << observer->describe();
    completer.join();
    observer->expectComplete();
}

TEST(TestObserverTest, ReleasesSubscriptionAfterTerminationOrDisposal)
{
    const auto completed = std::make_shared<TestObserver>();
    auto completedDisposable = std::make_shared<AtomicDisposable>();
    const std::weak_ptr<Disposable> completedReference = completedDisposable;
    completed->onSubscribe(completedDisposable);
    completedDisposable.reset();
    completed->onComplete();
    EXPECT_TRUE(completedReference.expired());

    const auto disposed = std::make_shared<TestObserver>();
    auto disposedDisposable = std::make_shared<AtomicDisposable>();
    const std::weak_ptr<Disposable> disposedReference = disposedDisposable;
    disposed->onSubscribe(disposedDisposable);
    disposedDisposable.reset();
    disposed->dispose();
    EXPECT_TRUE(disposedReference.expired());
}

TEST(TestWorkerTest, RunsTasksByVirtualTimeAndInsertionOrder)
{
    TestWorker worker;
    std::vector<int32_t> values;
    worker.schedule([&values] { values.push_back(2); }, 20);
    worker.schedule([&values] { values.push_back(1); }, 10);
    worker.schedule([&values] { values.push_back(3); }, 20);

    worker.advanceBy(19);
    EXPECT_EQ(values, std::vector<int32_t>({1}));

    worker.advanceBy(1);
    EXPECT_EQ(values, std::vector<int32_t>({1, 2, 3}));
}

TEST(TestWorkerTest, SkipsCancelledTasksAndRejectsWorkAfterDisposal)
{
    TestWorker worker;
    int32_t runCount = 0;
    const auto task = worker.schedule([&runCount] { ++runCount; }, 1);
    task->dispose();
    worker.runUntilIdle();

    worker.dispose();
    const auto rejected = worker.schedule([&runCount] { ++runCount; }, 0);

    EXPECT_EQ(runCount, 0);
    EXPECT_TRUE(rejected->isDisposed());
    EXPECT_TRUE(worker.isDisposed());
}

TEST(TestSchedulerTest, SharesTimeAndOrderingAcrossWorkers)
{
    TestScheduler scheduler;
    const auto first = std::dynamic_pointer_cast<TestWorker>(scheduler.createWorker());
    const auto second = std::dynamic_pointer_cast<TestWorker>(scheduler.createWorker());
    std::vector<int32_t> values;

    first->schedule([&values] { values.push_back(3); }, 20);
    second->schedule([&values] { values.push_back(1); }, 10);
    first->schedule([&values] { values.push_back(2); }, 10);

    scheduler.advanceTo(10);
    EXPECT_EQ(values, std::vector<int32_t>({1, 2}));
    EXPECT_EQ(first->currentTime(), 10u);
    EXPECT_EQ(second->currentTime(), 10u);

    scheduler.runUntilIdle();
    EXPECT_EQ(values, std::vector<int32_t>({1, 2, 3}));
    EXPECT_EQ(scheduler.currentTime(), 20u);
}

TEST(BoundedWaitTest, UsesConditionVariableWithExplicitTimeout)
{
    BoundedWait wait;
    std::thread signaler([&wait] { wait.signal(); });

    EXPECT_TRUE(wait.await(std::chrono::milliseconds(500)))
        << "timed out waiting for the local signal";
    signaler.join();
    EXPECT_EQ(wait.count(), 1u);
}
} // namespace
