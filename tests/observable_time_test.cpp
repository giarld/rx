#include <gtest/gtest.h>

#include "support/bounded_wait.h"
#include "support/test_observer.h"
#include "support/test_scheduler.h"

#include <rx/rx.h>
#include <rx/disposables/atomic_disposable.h>
#include <rx/operators/observable_timeout.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
using namespace rx;
using namespace rx::test;
} // namespace

TEST(ObservableTimeoutRegressionTest, TimeoutRejectsLateSourceValue)
{
    const auto worker = std::make_shared<TestWorker>();
    std::vector<int64_t> values;
    int32_t completionCount = 0;
    const auto observer = std::make_shared<TimeoutObserver>(
        std::make_shared<LambdaObserver>(
            [&values](const GAny &value) { values.push_back(value.toInt64()); },
            [](const GAnyException &) {}, [&completionCount] { ++completionCount; },
            [](const DisposablePtr &) {}),
        1, worker, Observable::just(10));

    observer->onSubscribe(std::make_shared<AtomicDisposable>());
    worker->runUntilIdle();
    observer->onNext(20);

    EXPECT_EQ(values, std::vector<int64_t>({10}));
    EXPECT_EQ(completionCount, 1);
}

TEST(ObservableIntervalTest, EmitsPeriodicallyAndStopsAfterDisposal)
{
    ScopedGlobalTimerScheduler timerScope("ObservableIntervalTest");
    BoundedWait initialDelay;
    BoundedWait emittedThree(3);
    BoundedWait firstDeadline;
    BoundedWait secondDeadline;
    std::atomic<bool> emittedBeforeInitialDelay = false;
    std::mutex valuesMutex;
    std::vector<int64_t> values;
    std::thread timerThread([timerScheduler = timerScope.scheduler()] {
        timerScheduler->run();
    });

    const auto subscription = Observable::interval(20, 1)->subscribe(
        [&](const GAny &value) {
            if (initialDelay.count() == 0) {
                emittedBeforeInitialDelay.store(true, std::memory_order_release);
            }
            {
                std::lock_guard lock(valuesMutex);
                values.push_back(value.toInt64());
            }
            emittedThree.signal();
        },
        [](const GAnyException &) {},
        [] {});
    timerScope.scheduler()->post([&initialDelay] { initialDelay.signal(); }, 1);
    const bool reachedInitialDelay = initialDelay.await(std::chrono::milliseconds(1000));
    const bool reachedThreeValues = emittedThree.await(std::chrono::milliseconds(1000));
    subscription->dispose();
    timerScope.scheduler()->post([&firstDeadline] { firstDeadline.signal(); }, 5);
    const bool reachedFirstDeadline = firstDeadline.await(std::chrono::milliseconds(1000));
    size_t disposedValueCount;
    {
        std::lock_guard lock(valuesMutex);
        disposedValueCount = values.size();
    }
    timerScope.scheduler()->post([&secondDeadline] { secondDeadline.signal(); }, 5);
    const bool reachedSecondDeadline = secondDeadline.await(std::chrono::milliseconds(1000));

    timerScope.scheduler()->stop();
    timerThread.join();

    EXPECT_TRUE(reachedInitialDelay) << "interval initial-delay sentinel timed out";
    EXPECT_FALSE(emittedBeforeInitialDelay.load(std::memory_order_acquire));
    EXPECT_TRUE(reachedThreeValues) << "interval did not emit three periodic values";
    EXPECT_TRUE(reachedFirstDeadline) << "interval first disposal deadline timed out";
    EXPECT_TRUE(reachedSecondDeadline) << "interval second disposal deadline timed out";
    ASSERT_GE(values.size(), 3U);
    EXPECT_EQ(values[0], 0);
    EXPECT_EQ(values[1], 1);
    EXPECT_EQ(values[2], 2);
    EXPECT_EQ(values.size(), disposedValueCount);
}

TEST(ObservableTimerTest, EmitsOnceAndCompletes)
{
    ScopedGlobalTimerScheduler timerScope("ObservableTimerTest");
    const auto observer = std::make_shared<TestObserver>();
    BoundedWait deadline;
    std::thread timerThread([timerScheduler = timerScope.scheduler()] {
        timerScheduler->run();
    });

    Observable::timer(1)->subscribe(observer);
    const bool completed = observer->awaitTerminal(std::chrono::milliseconds(1000));
    timerScope.scheduler()->post([&deadline] { deadline.signal(); }, 10);
    const bool reachedDeadline = deadline.await(std::chrono::milliseconds(1000));

    timerScope.scheduler()->stop();
    timerThread.join();

    EXPECT_TRUE(completed) << "timer did not complete: " << observer->describe();
    EXPECT_TRUE(reachedDeadline) << "timer single-emission deadline timed out";
    observer->expectInt64Values({0});
    observer->expectComplete();
}

TEST(ObservableTimerTest, DisposalCancelsPendingEmission)
{
    ScopedGlobalTimerScheduler timerScope("ObservableTimerCancellationTest");
    const auto observer = std::make_shared<TestObserver>();
    BoundedWait deadline;
    std::thread timerThread([timerScheduler = timerScope.scheduler()] {
        timerScheduler->run();
    });

    Observable::timer(10)->subscribe(observer);
    observer->dispose();
    timerScope.scheduler()->post([&deadline] { deadline.signal(); }, 20);
    EXPECT_TRUE(deadline.await(std::chrono::milliseconds(1000)))
        << "timer cancellation deadline timed out";
    observer->expectInt64Values({});
    observer->expectNotTerminated();

    timerScope.scheduler()->stop();
    timerThread.join();
}

TEST(ObservableDefaultTimeSchedulerTest, UsesTheGlobalMainThreadScheduler)
{
    ScopedGlobalTimerScheduler timerScope("ObservableDefaultTimeSchedulerTest");
    const auto delayObserver = std::make_shared<TestObserver>();
    const auto debounceObserver = std::make_shared<TestObserver>();
    const auto sampleObserver = std::make_shared<TestObserver>();
    const auto timeoutObserver = std::make_shared<TestObserver>();
    ObservableEmitterPtr delayEmitter;
    ObservableEmitterPtr debounceEmitter;
    ObservableEmitterPtr sampleEmitter;
    std::thread timerThread([timerScheduler = timerScope.scheduler()] {
        timerScheduler->run();
    });

    Observable::create([&delayEmitter](const ObservableEmitterPtr &emitter) {
        delayEmitter = emitter;
    })->delay(1)->subscribe(delayObserver);
    Observable::create([&debounceEmitter](const ObservableEmitterPtr &emitter) {
        debounceEmitter = emitter;
    })->debounce(1)->subscribe(debounceObserver);
    Observable::create([&sampleEmitter](const ObservableEmitterPtr &emitter) {
        sampleEmitter = emitter;
    })->sample(1)->subscribe(sampleObserver);
    Observable::never()->timeout(1, Observable::just(4))->subscribe(timeoutObserver);

    delayEmitter->onNext(1);
    debounceEmitter->onNext(2);
    sampleEmitter->onNext(3);
    EXPECT_TRUE(delayObserver->awaitValueCount(1, std::chrono::milliseconds(1000)))
        << "default delay scheduler timed out: " << delayObserver->describe();
    EXPECT_TRUE(debounceObserver->awaitValueCount(1, std::chrono::milliseconds(1000)))
        << "default debounce scheduler timed out: " << debounceObserver->describe();
    EXPECT_TRUE(sampleObserver->awaitValueCount(1, std::chrono::milliseconds(1000)))
        << "default sample scheduler timed out: " << sampleObserver->describe();
    EXPECT_TRUE(timeoutObserver->awaitTerminal(std::chrono::milliseconds(1000)))
        << "default timeout scheduler timed out: " << timeoutObserver->describe();

    delayObserver->expectInt64Values({1});
    delayObserver->expectNotTerminated();
    debounceObserver->expectInt64Values({2});
    debounceObserver->expectNotTerminated();
    sampleObserver->expectInt64Values({3});
    sampleObserver->expectNotTerminated();
    timeoutObserver->expectInt64Values({4});
    timeoutObserver->expectComplete();

    delayObserver->dispose();
    debounceObserver->dispose();
    sampleObserver->dispose();
    timerScope.scheduler()->stop();
    timerThread.join();
}

TEST(ObservableDelayTest, DelaysValuesAndCompletionInSourceOrder)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();

    Observable::just(1, 2)->delay(10, scheduler)->subscribe(observer);
    scheduler->advanceBy(9);
    observer->expectInt64Values({});
    observer->expectNotTerminated();

    scheduler->advanceBy(1);
    observer->expectInt64Values({1, 2});
    observer->expectComplete();
}

TEST(ObservableDelayTest, DisposalCancelsPendingSignals)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();

    Observable::just(1)->delay(10, scheduler)->subscribe(observer);
    observer->dispose();
    scheduler->runUntilIdle();

    observer->expectInt64Values({});
    observer->expectNotTerminated();
}

TEST(ObservableDelayTest, ForwardsSourceErrorWithoutWaitingForTheValueDelay)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();

    Observable::error(GAnyException("delay failure"))
        ->delay(10, scheduler)->subscribe(observer);
    scheduler->advanceBy(0);

    observer->expectInt64Values({});
    observer->expectErrorContains("delay failure");
}

TEST(ObservableDebounceTest, EmitsOnlyTheLatestValueAfterSilence)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();
    ObservableEmitterPtr emitter;

    Observable::create([&emitter](const ObservableEmitterPtr &sourceEmitter) {
        emitter = sourceEmitter;
    })->debounce(10, scheduler)->subscribe(observer);

    emitter->onNext(1);
    scheduler->advanceBy(5);
    emitter->onNext(2);
    scheduler->advanceBy(9);
    observer->expectInt64Values({});

    scheduler->advanceBy(1);
    observer->expectInt64Values({2});
    emitter->onComplete();
    observer->expectComplete();
}

TEST(ObservableDebounceTest, CompletionFlushesThePendingValue)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();
    ObservableEmitterPtr emitter;

    Observable::create([&emitter](const ObservableEmitterPtr &sourceEmitter) {
        emitter = sourceEmitter;
    })->debounce(10, scheduler)->subscribe(observer);

    emitter->onNext(7);
    emitter->onComplete();

    observer->expectInt64Values({7});
    observer->expectComplete();
    scheduler->runUntilIdle();
    observer->expectInt64Values({7});
}

TEST(ObservableDebounceTest, DisposalCancelsPendingValue)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();
    ObservableEmitterPtr emitter;

    Observable::create([&emitter](const ObservableEmitterPtr &sourceEmitter) {
        emitter = sourceEmitter;
    })->debounce(10, scheduler)->subscribe(observer);

    emitter->onNext(7);
    observer->dispose();
    scheduler->runUntilIdle();

    observer->expectInt64Values({});
    observer->expectNotTerminated();
}

TEST(ObservableDebounceTest, ForwardsSourceErrorAndCancelsPendingValue)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();
    ObservableEmitterPtr emitter;

    Observable::create([&emitter](const ObservableEmitterPtr &sourceEmitter) {
        emitter = sourceEmitter;
    })->debounce(10, scheduler)->subscribe(observer);

    emitter->onNext(7);
    emitter->onError(GAnyException("debounce failure"));
    scheduler->runUntilIdle();

    observer->expectInt64Values({});
    observer->expectErrorContains("debounce failure");
}

TEST(ObservableSampleTest, EmitsTheLatestValueAtEachPeriod)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();
    ObservableEmitterPtr emitter;

    Observable::create([&emitter](const ObservableEmitterPtr &sourceEmitter) {
        emitter = sourceEmitter;
    })->sample(10, scheduler)->subscribe(observer);

    emitter->onNext(1);
    emitter->onNext(2);
    scheduler->advanceBy(10);
    observer->expectInt64Values({2});

    emitter->onNext(3);
    scheduler->advanceBy(10);
    observer->expectInt64Values({2, 3});
    emitter->onComplete();
    observer->expectComplete();
}

TEST(ObservableSampleTest, DisposalStopsPeriodicSampling)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();
    ObservableEmitterPtr emitter;

    Observable::create([&emitter](const ObservableEmitterPtr &sourceEmitter) {
        emitter = sourceEmitter;
    })->sample(10, scheduler)->subscribe(observer);

    emitter->onNext(3);
    observer->dispose();
    scheduler->runUntilIdle();

    observer->expectInt64Values({});
    observer->expectNotTerminated();
}

TEST(ObservableSampleTest, ForwardsSourceErrorAndStopsSampling)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();
    ObservableEmitterPtr emitter;

    Observable::create([&emitter](const ObservableEmitterPtr &sourceEmitter) {
        emitter = sourceEmitter;
    })->sample(10, scheduler)->subscribe(observer);

    emitter->onNext(3);
    emitter->onError(GAnyException("sample failure"));
    scheduler->runUntilIdle();

    observer->expectInt64Values({});
    observer->expectErrorContains("sample failure");
}

TEST(ObservableTimeoutTest, SwitchesToFallbackAtTheDeadline)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();

    Observable::never()->timeout(10, scheduler, Observable::just(9))->subscribe(observer);
    scheduler->advanceBy(9);
    observer->expectNotTerminated();

    scheduler->advanceBy(1);
    observer->expectInt64Values({9});
    observer->expectComplete();
}

TEST(ObservableTimeoutTest, EmitsTimeoutErrorWithoutFallback)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();

    Observable::never()->timeout(10, scheduler)->subscribe(observer);
    scheduler->advanceBy(9);
    observer->expectNotTerminated();

    scheduler->advanceBy(1);
    observer->expectInt64Values({});
    observer->expectErrorContains("Timeout");
}

TEST(ObservableTimeoutTest, ForwardsSourceErrorBeforeTheDeadline)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();

    Observable::error(GAnyException("source failure"))->timeout(10, scheduler)->subscribe(observer);
    scheduler->runUntilIdle();

    observer->expectInt64Values({});
    observer->expectErrorContains("source failure");
}

TEST(ObservableTimeoutTest, ForwardsSourceCompletionBeforeTheDeadline)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();

    Observable::just(4)->timeout(10, scheduler)->subscribe(observer);
    scheduler->runUntilIdle();

    observer->expectInt64Values({4});
    observer->expectComplete();
}

TEST(ObservableTimeoutTest, DisposalCancelsPendingTimeout)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();

    Observable::never()->timeout(10, scheduler, Observable::just(9))->subscribe(observer);
    observer->dispose();
    scheduler->runUntilIdle();

    observer->expectInt64Values({});
    observer->expectNotTerminated();
}

TEST(ObservableTimeoutTest, ForwardsFallbackError)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();

    Observable::never()->timeout(10, scheduler,
        Observable::error(GAnyException("fallback failure")))->subscribe(observer);
    scheduler->runUntilIdle();

    observer->expectInt64Values({});
    observer->expectErrorContains("fallback failure");
}
