#include <gtest/gtest.h>

#include <rx/rx.h>
#include <rx/disposables/atomic_disposable.h>
#include <rx/grouped_observable.h>
#include <rx/operators/observable_observe_on.h>
#include <rx/operators/observable_timeout.h>
#include <rx/operators/observable_amb.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace
{
using namespace rx;

class DisposeOnFirstObserver : public Observer
{
public:
    void onSubscribe(const DisposablePtr &disposable) override
    {
        mDisposable = disposable;
    }

public:
    void onNext(const GAny &value) override
    {
        mValues.push_back(value.toInt64());
        mDisposable->dispose();
    }

    void onError(const GAnyException & /*error*/) override
    {
        mErrored = true;
    }

    void onComplete() override
    {
        mCompleted = true;
    }

public:
    std::vector<int64_t> mValues;
    bool mErrored = false;
    bool mCompleted = false;

private:
    DisposablePtr mDisposable;
};

struct ManualTaskState
{
    WorkerRunnable runnable;
    std::shared_ptr<AtomicDisposable> disposable;
};

class ManualWorker : public Worker
{
public:
    explicit ManualWorker(std::shared_ptr<ManualTaskState> state)
        : mState(std::move(state))
    {
    }

public:
    DisposablePtr schedule(const WorkerRunnable &run, uint64_t /*delay*/) override
    {
        mState->runnable = run;
        mState->disposable = std::make_shared<AtomicDisposable>();
        return mState->disposable;
    }

    void dispose() override
    {
        mDisposed = true;
    }

    bool isDisposed() const override
    {
        return mDisposed;
    }

private:
    std::shared_ptr<ManualTaskState> mState;
    bool mDisposed = false;
};

class ManualScheduler : public Scheduler
{
public:
    ManualScheduler()
        : mState(std::make_shared<ManualTaskState>())
    {
    }

public:
    WorkerPtr createWorker() override
    {
        return std::make_shared<ManualWorker>(mState);
    }

    void runPending() const
    {
        if (mState->runnable && mState->disposable && !mState->disposable->isDisposed()) {
            mState->runnable();
        }
    }

private:
    std::shared_ptr<ManualTaskState> mState;
};

class QueueWorker : public Worker
{
public:
    DisposablePtr schedule(const WorkerRunnable &run, uint64_t /*delay*/) override
    {
        const auto disposable = std::make_shared<AtomicDisposable>();
        mTasks.emplace_back(run, disposable);
        return disposable;
    }

    void dispose() override
    {
        mDisposed = true;
    }

    bool isDisposed() const override
    {
        return mDisposed;
    }

    void runPending()
    {
        const auto tasks = std::move(mTasks);
        mTasks.clear();
        for (const auto &task: tasks) {
            if (!task.second->isDisposed()) {
                task.first();
            }
        }
    }

private:
    bool mDisposed = false;
    std::vector<std::pair<WorkerRunnable, std::shared_ptr<AtomicDisposable> > > mTasks;
};

class TrackingDisposable : public Disposable
{
public:
    void dispose() override { mDisposed = true; }
    bool isDisposed() const override { return mDisposed; }

private:
    std::atomic<bool> mDisposed{false};
};
} // namespace

TEST(ObservableRangeRegressionTest, SupportsNegativeStart)
{
    std::vector<int64_t> values;
    Observable::range(-3, 3)->subscribe([&values](const GAny &value) {
        values.push_back(value.toInt64());
    });

    EXPECT_EQ(values, std::vector<int64_t>({-3, -2, -1}));
}

TEST(ObservableRangeRegressionTest, SupportsInt64UpperBoundary)
{
    std::vector<int64_t> values;
    Observable::range(std::numeric_limits<int64_t>::max() - 1, 2)->subscribe(
        [&values](const GAny &value) { values.push_back(value.toInt64()); });

    EXPECT_EQ(values, std::vector<int64_t>({std::numeric_limits<int64_t>::max() - 1,
                                            std::numeric_limits<int64_t>::max()}));
}

TEST(ObservableRangeRegressionTest, RejectsOverflowingInterval)
{
    EXPECT_THROW(Observable::range(std::numeric_limits<int64_t>::max(), 2), GAnyException);
}

TEST(ObservableRangeRegressionTest, StopsEmissionAfterDisposal)
{
    const auto observer = std::make_shared<DisposeOnFirstObserver>();

    Observable::range(0, 1000)->subscribe(observer);

    EXPECT_EQ(observer->mValues, std::vector<int64_t>({0}));
    EXPECT_FALSE(observer->mErrored);
    EXPECT_FALSE(observer->mCompleted);
}

TEST(ObservableCombinationRegressionTest, ZipCompletesOnce)
{
    std::vector<int64_t> values;
    int32_t completionCount = 0;
    int32_t errorCount = 0;

    Observable::zip(Observable::just(1, 2), Observable::just(10, 20),
                    [](const GAny &left, const GAny &right) {
                        return left.toInt64() + right.toInt64();
                    })
        ->subscribe(
            [&values](const GAny &value) { values.push_back(value.toInt64()); },
            [&errorCount](const GAnyException &) { ++errorCount; },
            [&completionCount] { ++completionCount; });

    EXPECT_EQ(values, std::vector<int64_t>({11, 22}));
    EXPECT_EQ(errorCount, 0);
    EXPECT_EQ(completionCount, 1);
}

TEST(ObservableCombinationRegressionTest, FlatMapForwardsInnerErrorOnce)
{
    int32_t errorCount = 0;
    int32_t completionCount = 0;

    Observable::just(1)
        ->flatMap([](const GAny &) { return Observable::error(GAnyException("inner failure")); })
        ->subscribe(
            [](const GAny &) {},
            [&errorCount](const GAnyException &) { ++errorCount; },
            [&completionCount] { ++completionCount; });

    EXPECT_EQ(errorCount, 1);
    EXPECT_EQ(completionCount, 0);
}

TEST(ObservableTakeUntilRegressionTest, EmptyTriggerDoesNotStopMainSource)
{
    std::vector<int64_t> values;
    int32_t completionCount = 0;

    Observable::just(1)
        ->takeUntil(Observable::empty())
        ->subscribe(
            [&values](const GAny &value) { values.push_back(value.toInt64()); },
            [](const GAnyException &) { FAIL() << "takeUntil(empty) must not fail"; },
            [&completionCount] { ++completionCount; });

    EXPECT_EQ(values, std::vector<int64_t>({1}));
    EXPECT_EQ(completionCount, 1);
}

TEST(SchedulerRegressionTest, DisposedDirectTaskDoesNotRun)
{
    ManualScheduler scheduler;
    bool ran = false;

    const auto disposable = scheduler.scheduleDirect([&ran] { ran = true; }, 1);
    disposable->dispose();
    scheduler.runPending();

    EXPECT_FALSE(ran);
}

TEST(ObservableCombinationRegressionTest, ZipCompletesWhenEmptySourceEnds)
{
    int32_t completionCount = 0;
    Observable::zip(Observable::empty(), Observable::never(),
                    [](const GAny &, const GAny &) { return GAny(); })
        ->subscribe(
            [](const GAny &) { FAIL() << "zip(empty, never) must not emit"; },
            [](const GAnyException &) { FAIL() << "zip(empty, never) must not fail"; },
            [&completionCount] { ++completionCount; });

    EXPECT_EQ(completionCount, 1);
}

TEST(ObservableCombinationRegressionTest, ZipAllowsDisposalFromOnNext)
{
    DisposablePtr disposable;
    int32_t valueCount = 0;
    const auto observer = std::make_shared<LambdaObserver>(
        [&disposable, &valueCount](const GAny &) {
            ++valueCount;
            disposable->dispose();
        },
        [](const GAnyException &) {}, [] {},
        [&disposable](const DisposablePtr &d) { disposable = d; });

    Observable::zip(Observable::just(1), Observable::just(2),
                    [](const GAny &left, const GAny &right) {
                        return left.toInt64() + right.toInt64();
                    })
        ->subscribe(observer);

    EXPECT_EQ(valueCount, 1);
}

TEST(ObservableCombinationRegressionTest, CombineLatestAllowsDisposalFromOnNext)
{
    DisposablePtr disposable;
    int32_t valueCount = 0;
    const auto observer = std::make_shared<LambdaObserver>(
        [&disposable, &valueCount](const GAny &) {
            ++valueCount;
            disposable->dispose();
        },
        [](const GAnyException &) {}, [] {},
        [&disposable](const DisposablePtr &d) { disposable = d; });

    Observable::combineLatest(Observable::just(1), Observable::just(2),
                              [](const GAny &left, const GAny &right) {
                                  return left.toInt64() + right.toInt64();
                              })
        ->subscribe(observer);

    EXPECT_EQ(valueCount, 1);
}

TEST(ObservableLifetimeRegressionTest, ClosedWindowRemainsSubscribable)
{
    std::shared_ptr<Observable> window;
    Observable::just(1)->window(1)->subscribe([&window](const GAny &value) {
        window = value.castAs<std::shared_ptr<Observable> >();
    });

    int32_t completionCount = 0;
    ASSERT_NE(window, nullptr);
    window->subscribe(
        [](const GAny &) {},
        [](const GAnyException &) { FAIL() << "closed window must not fail"; },
        [&completionCount] { ++completionCount; });

    EXPECT_EQ(completionCount, 1);
}

TEST(ObservableLifetimeRegressionTest, ClosedGroupRemainsSubscribable)
{
    std::shared_ptr<GroupedObservable> group;
    Observable::just(1)->groupBy([](const GAny &value) { return value; })->subscribe(
        [&group](const GAny &value) { group = value.castAs<std::shared_ptr<GroupedObservable> >(); });

    int32_t completionCount = 0;
    ASSERT_NE(group, nullptr);
    group->subscribe(
        [](const GAny &) {},
        [](const GAnyException &) { FAIL() << "closed group must not fail"; },
        [&completionCount] { ++completionCount; });

    EXPECT_EQ(completionCount, 1);
}

TEST(ObservableCallbackRegressionTest, DoOnSubscribeFailureReachesDownstream)
{
    int32_t errorCount = 0;
    Observable::just(1)
        ->doOnSubscribe([](const DisposablePtr &) { throw std::runtime_error("subscribe failure"); })
        ->subscribe(
            [](const GAny &) {},
            [&errorCount](const GAnyException &) { ++errorCount; },
            [] { FAIL() << "doOnSubscribe failure must not complete"; });

    EXPECT_EQ(errorCount, 1);
}

TEST(ObservableCallbackRegressionTest, StandardExceptionBecomesOnError)
{
    int32_t errorCount = 0;
    EXPECT_NO_THROW(
        Observable::just(1)
            ->map([](const GAny &) -> GAny { throw std::runtime_error("mapper failure"); })
            ->subscribe(
                [](const GAny &) {},
                [&errorCount](const GAnyException &) { ++errorCount; },
                [] {}));

    EXPECT_EQ(errorCount, 1);
}

TEST(ObservableCallbackRegressionTest, CreateStandardExceptionBecomesOnError)
{
    int32_t errorCount = 0;
    EXPECT_NO_THROW(
        Observable::create([](const ObservableEmitterPtr &) {
            throw std::runtime_error("source failure");
        })->subscribe(
            [](const GAny &) {},
            [&errorCount](const GAnyException &) { ++errorCount; },
            [] {}));

    EXPECT_EQ(errorCount, 1);
}

TEST(ObservableCallbackRegressionTest, CombineLatestStandardExceptionBecomesOnError)
{
    int32_t errorCount = 0;
    EXPECT_NO_THROW(
        Observable::combineLatest(Observable::just(1), Observable::just(2),
                                  [](const GAny &, const GAny &) -> GAny {
                                      throw std::runtime_error("combiner failure");
                                  })
            ->subscribe(
                [](const GAny &) {},
                [&errorCount](const GAnyException &) { ++errorCount; },
                [] {}));

    EXPECT_EQ(errorCount, 1);
}

TEST(ObservableCallbackRegressionTest, SkipWhileFailureTerminatesOnce)
{
    int32_t errorCount = 0;
    int32_t completionCount = 0;
    Observable::range(1, 3)
        ->skipWhile([](const GAny &) -> bool { throw std::runtime_error("predicate failure"); })
        ->subscribe(
            [](const GAny &) {},
            [&errorCount](const GAnyException &) { ++errorCount; },
            [&completionCount] { ++completionCount; });

    EXPECT_EQ(errorCount, 1);
    EXPECT_EQ(completionCount, 0);
}

TEST(ObservableSequenceEqualRegressionTest, DoesNotSubscribeSecondSourceAfterSynchronousError)
{
    int32_t secondSubscriptions = 0;
    const auto second = Observable::create([&secondSubscriptions](const ObservableEmitterPtr &) {
        ++secondSubscriptions;
    });

    Observable::sequenceEqual(Observable::error(GAnyException("first failure")), second)
        ->subscribe([](const GAny &) {}, [](const GAnyException &) {}, [] {});

    EXPECT_EQ(secondSubscriptions, 0);
}

TEST(ObservableParameterRegressionTest, RejectsInvalidBufferAndWindowArguments)
{
    const auto source = Observable::just(1);
    EXPECT_THROW(source->buffer(1, 0), GAnyException);
    EXPECT_THROW(source->buffer(0, 1), GAnyException);
    EXPECT_THROW(source->window(1, 0), GAnyException);
    EXPECT_THROW(source->window(0, 1), GAnyException);
    EXPECT_THROW(source->window(-1, 1), GAnyException);
}

TEST(ObservableSwitchMapRegressionTest, NullMapperResultIsAnError)
{
    int32_t errorCount = 0;
    Observable::just(1)
        ->switchMap([](const GAny &) { return std::shared_ptr<Observable>(); })
        ->subscribe(
            [](const GAny &) {},
            [&errorCount](const GAnyException &) { ++errorCount; },
            [] { FAIL() << "null switchMap result must not complete"; });

    EXPECT_EQ(errorCount, 1);
}

TEST(ObservableSwitchMapRegressionTest, NullMapperResultCancelsPreviousInner)
{
    const auto oldInnerDisposable = std::make_shared<AtomicDisposable>();
    int32_t errorCount = 0;

    Observable::just(1, 2)
        ->switchMap([oldInnerDisposable](const GAny &value) {
            if (value.toInt64() == 1) {
                return Observable::create([oldInnerDisposable](const ObservableEmitterPtr &emitter) {
                    emitter->setDisposable(oldInnerDisposable);
                });
            }
            return std::shared_ptr<Observable>();
        })
        ->subscribe(
            [](const GAny &) {},
            [&errorCount](const GAnyException &) { ++errorCount; },
            [] { FAIL() << "null switchMap result must not complete"; });

    EXPECT_TRUE(oldInnerDisposable->isDisposed());
    EXPECT_EQ(errorCount, 1);
}

TEST(ObservableObserveOnRegressionTest, DisposeBeforeDrainDropsQueuedValues)
{
    const auto worker = std::make_shared<QueueWorker>();
    const auto observer = std::make_shared<ObserveOnObserver>(
        std::make_shared<LambdaObserver>(
            [](const GAny &) { FAIL() << "disposed observeOn must not emit"; },
            [](const GAnyException &) {}, [] {}, [](const DisposablePtr &) {}),
        worker);

    observer->onSubscribe(std::make_shared<AtomicDisposable>());
    observer->onNext(1);
    observer->dispose();
    worker->runPending();

    EXPECT_TRUE(observer->isDisposed());
}

TEST(ObservableTimeoutRegressionTest, TimeoutRejectsLateSourceValue)
{
    const auto worker = std::make_shared<QueueWorker>();
    std::vector<int64_t> values;
    int32_t completionCount = 0;
    const auto observer = std::make_shared<TimeoutObserver>(
        std::make_shared<LambdaObserver>(
            [&values](const GAny &value) { values.push_back(value.toInt64()); },
            [](const GAnyException &) {}, [&completionCount] { ++completionCount; },
            [](const DisposablePtr &) {}),
        1, worker, Observable::just(10));

    observer->onSubscribe(std::make_shared<AtomicDisposable>());
    worker->runPending();
    observer->onNext(20);

    EXPECT_EQ(values, std::vector<int64_t>({10}));
    EXPECT_EQ(completionCount, 1);
}

TEST(ObservableGroupByRegressionTest, KeepsDifferentKeyTypesSeparate)
{
    int32_t groupCount = 0;
    Observable::just(GAny(1), GAny("1"))
        ->groupBy([](const GAny &value) { return value; })
        ->subscribe([&groupCount](const GAny &) { ++groupCount; });

    EXPECT_EQ(groupCount, 2);
}

TEST(ObservableScanRegressionTest, NullFirstValueStillUsesAccumulator)
{
    int32_t accumulatorCalls = 0;
    std::vector<GAny> values;

    Observable::just(GAny(nullptr), GAny(1))
        ->scan([&accumulatorCalls](const GAny &, const GAny &next) {
            ++accumulatorCalls;
            return next;
        })
        ->subscribe([&values](const GAny &value) { values.push_back(value); });

    ASSERT_EQ(values.size(), 2u);
    EXPECT_TRUE(values[0] == nullptr);
    EXPECT_EQ(values[1].toInt64(), 1);
    EXPECT_EQ(accumulatorCalls, 1);
}

TEST(ObservableFlatMapRegressionTest, MainErrorCancelsActiveInner)
{
    const auto innerDisposable = std::make_shared<TrackingDisposable>();
    const auto source = Observable::create([innerDisposable](const ObservableEmitterPtr &emitter) {
        emitter->onNext(1);
        emitter->onError(GAnyException("main failure"));
    });

    source->flatMap([innerDisposable](const GAny &) {
        return Observable::create([innerDisposable](const ObservableEmitterPtr &emitter) {
            emitter->setDisposable(innerDisposable);
        });
    })->subscribe([](const GAny &) {}, [](const GAnyException &) {}, [] {});

    EXPECT_TRUE(innerDisposable->isDisposed());
}

TEST(ObservableConcatMapRegressionTest, DrainsAllQueuedSynchronousValues)
{
    std::vector<int64_t> values;
    Observable::range(0, 1000)
        ->concatMap([](const GAny &value) { return Observable::just(value); })
        ->subscribe([&values](const GAny &value) { values.push_back(value.toInt64()); });

    ASSERT_EQ(values.size(), 1000u);
    EXPECT_EQ(values.front(), 0);
    EXPECT_EQ(values.back(), 999);
}

TEST(ObservableAmbRegressionTest, DisposingCoordinatorCancelsEverySource)
{
    const auto first = std::make_shared<TrackingDisposable>();
    const auto second = std::make_shared<TrackingDisposable>();
    const auto coordinator = std::make_shared<AmbCoordinator>(
        std::make_shared<LambdaObserver>(
            [](const GAny &) {}, [](const GAnyException &) {}, [] {}, [](const DisposablePtr &) {}),
        2);

    coordinator->onSubscribe(0, first);
    coordinator->onSubscribe(1, second);
    coordinator->dispose();

    EXPECT_TRUE(first->isDisposed());
    EXPECT_TRUE(second->isDisposed());
}
