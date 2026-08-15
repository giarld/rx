#include <gtest/gtest.h>

#include "support/bounded_wait.h"
#include "support/test_observer.h"
#include "support/test_scheduler.h"

#include <rx/rx.h>
#include <rx/disposables/atomic_disposable.h>
#include <rx/operators/observable_observe_on.h>

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

TEST(SchedulerRegressionTest, DisposedDirectTaskDoesNotRun)
{
    TestScheduler scheduler;
    bool ran = false;

    const auto disposable = scheduler.scheduleDirect([&ran] { ran = true; }, 1);
    disposable->dispose();
    scheduler.runUntilIdle();

    EXPECT_FALSE(ran);
}

TEST(ObservableObserveOnRegressionTest, DisposeBeforeDrainDropsQueuedValues)
{
    const auto worker = std::make_shared<TestWorker>();
    const auto observer = std::make_shared<ObserveOnObserver>(
        std::make_shared<LambdaObserver>(
            [](const GAny &) { FAIL() << "disposed observeOn must not emit"; },
            [](const GAnyException &) {}, [] {}, [](const DisposablePtr &) {}),
        worker);

    observer->onSubscribe(std::make_shared<AtomicDisposable>());
    observer->onNext(1);
    observer->dispose();
    worker->runUntilIdle();

    EXPECT_TRUE(observer->isDisposed());
}

TEST(DisposeTaskTest, DisposesScheduledTaskAndWorkerTogether)
{
    const auto worker = std::make_shared<TestWorker>();
    const auto task = std::make_shared<DisposeTask>(worker);
    const auto scheduled = worker->schedule([] {}, 1);
    task->setDisposable(scheduled);

    task->dispose();
    task->dispose();

    EXPECT_TRUE(task->isDisposed());
    EXPECT_TRUE(scheduled->isDisposed());
    EXPECT_TRUE(worker->isDisposed());
}

TEST(SchedulerContractTest, ScheduleDirectRunsInDueTimeOrder)
{
    TestScheduler scheduler;
    std::vector<int32_t> values;

    const auto second = scheduler.scheduleDirect([&values] { values.push_back(2); }, 20);
    const auto first = scheduler.scheduleDirect([&values] { values.push_back(1); }, 10);
    scheduler.runUntilIdle();

    EXPECT_EQ(values, std::vector<int32_t>({1, 2}));
}

TEST(WorkerContractTest, NowReturnsNondecreasingSteadyTime)
{
    TestWorker worker;

    const auto first = worker.now();
    const auto second = worker.now();

    EXPECT_GT(first, 0U);
    EXPECT_GE(second, first);
}

TEST(ObservableSubscribeOnTest, DefersSubscriptionAndSupportsCancellation)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    int32_t subscriptions = 0;
    const auto source = Observable::create([&subscriptions](const ObservableEmitterPtr &emitter) {
        ++subscriptions;
        emitter->onNext(1);
        emitter->onComplete();
    });
    const auto observer = std::make_shared<TestObserver>();

    source->subscribeOn(scheduler)->subscribe(observer);
    EXPECT_EQ(subscriptions, 0);
    scheduler->runUntilIdle();
    EXPECT_EQ(subscriptions, 1);
    observer->expectInt64Values({1});
    observer->expectComplete();

    const auto cancelledObserver = std::make_shared<TestObserver>();
    source->subscribeOn(scheduler)->subscribe(cancelledObserver);
    cancelledObserver->dispose();
    scheduler->runUntilIdle();
    EXPECT_EQ(subscriptions, 1);
    cancelledObserver->expectInt64Values({});
    cancelledObserver->expectNotTerminated();
}

TEST(ObservableSubscribeOnTest, ForwardsSourceErrorAfterScheduledSubscription)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();

    Observable::error(GAnyException("subscribeOn failure"))
        ->subscribeOn(scheduler)
        ->subscribe(observer);
    observer->expectNotTerminated();

    scheduler->runUntilIdle();
    observer->expectInt64Values({});
    observer->expectErrorContains("subscribeOn failure");
}

TEST(ObservableObserveOnTest, PreservesSignalOrderOnTheWorker)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();

    Observable::just(1, 2, 3)->observeOn(scheduler)->subscribe(observer);
    observer->expectInt64Values({});
    observer->expectNotTerminated();

    scheduler->runUntilIdle();
    observer->expectInt64Values({1, 2, 3});
    observer->expectComplete();
}

TEST(ObservableObserveOnTest, ForwardsSourceErrorOnTheWorker)
{
    const auto scheduler = std::make_shared<TestScheduler>();
    const auto observer = std::make_shared<TestObserver>();

    Observable::error(GAnyException("observeOn failure"))
        ->observeOn(scheduler)
        ->subscribe(observer);
    observer->expectNotTerminated();

    scheduler->runUntilIdle();
    observer->expectInt64Values({});
    observer->expectErrorContains("observeOn failure");
}

TEST(TimerSchedulerTest, RunsImmediateTasksAndHonorsCancellationAndShutdown)
{
    ScopedGlobalTimerScheduler timerScope("TimerSchedulerTest");
    const auto scheduler = TimerScheduler::create(timerScope.scheduler());
    const auto worker = scheduler->createWorker();
    int32_t runCount = 0;

    worker->schedule([&runCount] { ++runCount; });
    const auto cancelled = worker->schedule([&runCount] { ++runCount; });
    cancelled->dispose();
    timerScope.scheduler()->loop();
    EXPECT_EQ(runCount, 1);

    worker->dispose();
    worker->schedule([&runCount] { ++runCount; });
    timerScope.scheduler()->loop();
    EXPECT_EQ(runCount, 1);
}

TEST(TimerSchedulerTest, RunsPositiveDelayAfterImmediateWork)
{
    ScopedGlobalTimerScheduler timerScope("TimerSchedulerDelayTest");
    const auto scheduler = TimerScheduler::create(timerScope.scheduler());
    BoundedWait completed(2);
    std::vector<int32_t> order;
    std::mutex orderMutex;
    std::thread timerThread([timerScheduler = timerScope.scheduler()] {
        timerScheduler->run();
    });

    const auto delayedTask = scheduler->scheduleDirect([&] {
        {
            std::lock_guard lock(orderMutex);
            order.push_back(2);
        }
        completed.signal();
    }, 5);
    const auto immediateTask = scheduler->scheduleDirect([&] {
        {
            std::lock_guard lock(orderMutex);
            order.push_back(1);
        }
        completed.signal();
    });

    EXPECT_TRUE(completed.await(std::chrono::milliseconds(1000)))
        << "timer scheduler did not complete immediate and delayed work";
    {
        std::lock_guard lock(orderMutex);
        EXPECT_EQ(order, std::vector<int32_t>({1, 2}));
    }

    timerScope.scheduler()->stop();
    timerThread.join();
}

TEST(MainThreadSchedulerTest, UsesTheCurrentGlobalTimerScheduler)
{
    ScopedGlobalTimerScheduler timerScope("MainThreadSchedulerTest");
    const auto scheduler = MainThreadScheduler::create();
    BoundedWait completed;

    const auto task = scheduler->scheduleDirect([&completed] { completed.signal(); });
    timerScope.scheduler()->loop();

    EXPECT_TRUE(completed.await(std::chrono::milliseconds(100)))
        << "main-thread scheduler did not run on the configured global timer";
}

TEST(MainThreadSchedulerTest, CancelsDelayedWork)
{
    ScopedGlobalTimerScheduler timerScope("MainThreadSchedulerCancellationTest");
    const auto scheduler = MainThreadScheduler::create();
    BoundedWait cancelled;
    BoundedWait deadline;
    std::thread timerThread([timerScheduler = timerScope.scheduler()] {
        timerScheduler->run();
    });

    const auto task = scheduler->scheduleDirect([&cancelled] { cancelled.signal(); }, 5);
    task->dispose();
    timerScope.scheduler()->post([&deadline] { deadline.signal(); }, 10);

    EXPECT_TRUE(deadline.await(std::chrono::milliseconds(1000)))
        << "main-thread cancellation deadline timed out";
    EXPECT_EQ(cancelled.count(), 0U);

    timerScope.scheduler()->stop();
    timerThread.join();
}

TEST(NewThreadSchedulerTest, RunsImmediateAndDelayedTasksWithBoundedWaits)
{
    ScopedGlobalTimerScheduler timerScope("NewThreadSchedulerTest");
    const auto scheduler = NewThreadScheduler::create();
    auto worker = scheduler->createWorker();
    BoundedWait completed(2);
    BoundedWait cancelled;
    BoundedWait cancellationDeadline;
    BoundedWait cleanup;
    std::vector<int32_t> order;
    std::mutex orderMutex;
    std::thread timerThread([timerScheduler = timerScope.scheduler()] {
        timerScheduler->run();
    });

    auto delayedTask = worker->schedule([&] {
        {
            std::lock_guard lock(orderMutex);
            order.push_back(2);
        }
        completed.signal();
    }, 5);
    auto immediateTask = worker->schedule([&] {
        {
            std::lock_guard lock(orderMutex);
            order.push_back(1);
        }
        completed.signal();
    });
    auto cancelledTask = worker->schedule([&cancelled] { cancelled.signal(); }, 100);
    cancelledTask->dispose();
    timerScope.scheduler()->post([&cancellationDeadline] { cancellationDeadline.signal(); }, 110);

    EXPECT_TRUE(completed.await(std::chrono::milliseconds(1000)))
        << "new-thread immediate and delayed tasks timed out";
    {
        std::lock_guard lock(orderMutex);
        EXPECT_EQ(order, std::vector<int32_t>({1, 2}));
    }
    EXPECT_TRUE(cancellationDeadline.await(std::chrono::milliseconds(1000)))
        << "new-thread cancellation deadline timed out";
    EXPECT_EQ(cancelled.count(), 0U);
    immediateTask.reset();
    delayedTask.reset();
    cancelledTask.reset();
    worker->dispose();
    worker.reset();
    timerScope.scheduler()->post([&cleanup] { cleanup.signal(); }, 1);
    EXPECT_TRUE(cleanup.await(std::chrono::milliseconds(1000)))
        << "new-thread scheduler cleanup timed out";
    timerScope.scheduler()->stop();
    timerThread.join();
}

TEST(NewThreadWorkerTest, DisposeWinsBeforeDelayedSubmission)
{
    ScopedGlobalTimerScheduler timerScope("NewThreadWorkerRaceTest");
    auto worker = NewThreadScheduler::create()->createWorker();
    BoundedWait ran;
    BoundedWait deadline;
    BoundedWait cleanup;
    std::thread timerThread([timerScheduler = timerScope.scheduler()] {
        timerScheduler->run();
    });

    worker->schedule([&ran] { ran.signal(); }, 5);
    worker->dispose();
    timerScope.scheduler()->post([&deadline] { deadline.signal(); }, 10);

    EXPECT_TRUE(deadline.await(std::chrono::milliseconds(1000)))
        << "new-thread dispose race deadline timed out";
    EXPECT_EQ(ran.count(), 0U);

    worker.reset();
    timerScope.scheduler()->post([&cleanup] { cleanup.signal(); }, 1);
    EXPECT_TRUE(cleanup.await(std::chrono::milliseconds(1000)))
        << "new-thread worker cleanup timed out";
    timerScope.scheduler()->stop();
    timerThread.join();
}

TEST(TaskSystemSchedulerTest, RunsImmediateAndDelayedTasks)
{
    GTaskSystem taskSystem("TaskSystemSchedulerTest", 1);
    taskSystem.start();
    const auto scheduler = TaskSystemScheduler::create(&taskSystem);
    const auto worker = scheduler->createWorker();
    BoundedWait completed(2);
    std::vector<int32_t> order;
    std::mutex orderMutex;

    const auto delayedTask = worker->schedule([&] {
        {
            std::lock_guard lock(orderMutex);
            order.push_back(2);
        }
        completed.signal();
    }, 5);
    const auto immediateTask = worker->schedule([&] {
        {
            std::lock_guard lock(orderMutex);
            order.push_back(1);
        }
        completed.signal();
    });

    EXPECT_TRUE(completed.await(std::chrono::milliseconds(1000)))
        << "task-system scheduler timed out, completed=" << completed.count();
    {
        std::lock_guard lock(orderMutex);
        EXPECT_EQ(order, std::vector<int32_t>({1, 2}));
    }
    taskSystem.stopAndWait();
}

TEST(TaskSystemWorkerTest, DisposeWinsBeforeQueuedExecution)
{
    GTaskSystem taskSystem("TaskSystemWorkerRaceTest", 1);
    BoundedWait blockerStarted;
    BoundedWait releaseBlocker;
    BoundedWait queueDrained;
    std::atomic<bool> blockerReleased = false;
    std::atomic<bool> ran = false;
    taskSystem.start();
    taskSystem.submit([&] {
        blockerStarted.signal();
        blockerReleased.store(
            releaseBlocker.await(std::chrono::milliseconds(1000)),
            std::memory_order_release);
    });
    const bool started = blockerStarted.await(std::chrono::milliseconds(1000));

    const auto scheduler = TaskSystemScheduler::create(&taskSystem);
    const auto worker = scheduler->createWorker();
    worker->schedule([&ran] { ran.store(true, std::memory_order_release); });
    worker->dispose();
    taskSystem.submit([&queueDrained] { queueDrained.signal(); });
    releaseBlocker.signal();

    EXPECT_TRUE(started) << "failed to occupy the task-system worker";
    EXPECT_TRUE(queueDrained.await(std::chrono::milliseconds(1000)))
        << "task-system queue did not drain after releasing the blocker";
    EXPECT_TRUE(blockerReleased.load(std::memory_order_acquire))
        << "task-system blocker timed out before release";
    EXPECT_FALSE(ran.load(std::memory_order_acquire));
    taskSystem.stopAndWait();
}

TEST(ObservableObserveOnTest, DisposalWinsBeforeQueuedTaskSystemDrain)
{
    GTaskSystem taskSystem("ObserveOnTaskSystemRaceTest", 1);
    BoundedWait blockerStarted;
    BoundedWait releaseBlocker;
    BoundedWait queueDrained;
    std::atomic<bool> blockerReleased = false;
    taskSystem.start();
    taskSystem.submit([&] {
        blockerStarted.signal();
        blockerReleased.store(
            releaseBlocker.await(std::chrono::milliseconds(1000)),
            std::memory_order_release);
    });
    const bool started = blockerStarted.await(std::chrono::milliseconds(1000));

    const auto scheduler = TaskSystemScheduler::create(&taskSystem);
    const auto observer = std::make_shared<TestObserver>();
    ObservableEmitterPtr emitter;
    Observable::create([&emitter](const ObservableEmitterPtr &sourceEmitter) {
        emitter = sourceEmitter;
    })->observeOn(scheduler)->subscribe(observer);
    emitter->onNext(1);
    observer->dispose();
    taskSystem.submit([&queueDrained] { queueDrained.signal(); });
    releaseBlocker.signal();

    EXPECT_TRUE(started) << "failed to occupy the observeOn task-system worker";
    EXPECT_TRUE(queueDrained.await(std::chrono::milliseconds(1000)))
        << "observeOn task-system queue did not drain";
    EXPECT_TRUE(blockerReleased.load(std::memory_order_acquire))
        << "observeOn task-system blocker timed out before release";
    observer->expectInt64Values({});
    observer->expectNotTerminated();
    taskSystem.stopAndWait();
}

TEST(TaskSystemWorkerTest, DisposedTaskDoesNotReachDelayedSubmission)
{
    GTaskSystem taskSystem("TaskSystemWorkerDelayCancellationTest", 1);
    taskSystem.start();
    const auto scheduler = TaskSystemScheduler::create(&taskSystem);
    const auto cancelledWorker = scheduler->createWorker();
    const auto sentinelWorker = scheduler->createWorker();
    BoundedWait ran;
    BoundedWait deadline;

    const auto cancelled = cancelledWorker->schedule([&ran] { ran.signal(); }, 5);
    cancelled->dispose();
    sentinelWorker->schedule([&deadline] { deadline.signal(); }, 10);

    EXPECT_TRUE(deadline.await(std::chrono::milliseconds(1000)))
        << "task-system delayed cancellation deadline timed out";
    EXPECT_EQ(ran.count(), 0U);
    taskSystem.stopAndWait();
}

TEST(JobSystemSchedulerTest, RunsImmediateTasksAndRejectsWorkAfterDisposal)
{
    GJobSystem jobSystem("JobSystemSchedulerTest", 1, 1);
    jobSystem.adopt();
    const auto scheduler = JobSystemScheduler::create(&jobSystem);
    const auto worker = scheduler->createWorker();
    BoundedWait completed(2);
    std::vector<int32_t> order;
    std::mutex orderMutex;

    worker->schedule([&] {
        {
            std::lock_guard lock(orderMutex);
            order.push_back(1);
        }
        completed.signal();
    });
    worker->schedule([&] {
        {
            std::lock_guard lock(orderMutex);
            order.push_back(2);
        }
        completed.signal();
    });
    EXPECT_TRUE(completed.await(std::chrono::milliseconds(1000)))
        << "job-system immediate tasks timed out";
    {
        std::lock_guard lock(orderMutex);
        EXPECT_EQ(order, std::vector<int32_t>({1, 2}));
    }

    worker->dispose();
    std::atomic<bool> ranAfterDisposal = false;
    worker->schedule([&ranAfterDisposal] {
        ranAfterDisposal.store(true, std::memory_order_release);
    });
    EXPECT_FALSE(ranAfterDisposal.load(std::memory_order_acquire));
    jobSystem.emancipate();
}

TEST(JobSystemWorkerTest, DisposedQueuedTaskDoesNotRun)
{
    BoundedWait blockerStarted;
    BoundedWait releaseBlocker;
    BoundedWait queueDrained;
    std::atomic<bool> blockerReleased = false;
    std::atomic<bool> ran = false;
    GJobSystem jobSystem("JobSystemWorkerCancellationTest", 1, 1);
    jobSystem.adopt();
    const auto scheduler = JobSystemScheduler::create(&jobSystem);
    const auto worker = scheduler->createWorker();

    worker->schedule([&] {
        blockerStarted.signal();
        blockerReleased.store(
            releaseBlocker.await(std::chrono::milliseconds(1000)),
            std::memory_order_release);
    });
    const bool started = blockerStarted.await(std::chrono::milliseconds(1000));
    const auto cancelled = worker->schedule([&ran] {
        ran.store(true, std::memory_order_release);
    });
    cancelled->dispose();
    worker->schedule([&queueDrained] { queueDrained.signal(); });
    releaseBlocker.signal();

    EXPECT_TRUE(started) << "failed to occupy the job-system worker";
    EXPECT_TRUE(queueDrained.await(std::chrono::milliseconds(1000)))
        << "job-system queue did not drain";
    EXPECT_TRUE(blockerReleased.load(std::memory_order_acquire))
        << "job-system blocker timed out before release";
    EXPECT_FALSE(ran.load(std::memory_order_acquire));
    jobSystem.emancipate();
}
