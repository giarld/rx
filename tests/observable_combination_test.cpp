#include <gtest/gtest.h>

#include "support/bounded_wait.h"
#include "support/test_observer.h"

#include <rx/rx.h>
#include <rx/disposables/atomic_disposable.h>
#include <rx/operators/observable_amb.h>

#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
using namespace rx;
using namespace rx::test;

class TrackingDisposable : public Disposable
{
public:
    void dispose() override
    {
        mDisposed = true;
    }

    bool isDisposed() const override
    {
        return mDisposed;
    }

private:
    std::atomic<bool> mDisposed{false};
};

struct ManualSource
{
    ManualSource()
        : disposable(std::make_shared<TrackingDisposable>()),
          observable(Observable::create([this](const ObservableEmitterPtr &value) {
              emitter = value;
              emitter->setDisposable(disposable);
              ++subscriptions;
          }))
    {
    }

    std::shared_ptr<TrackingDisposable> disposable;
    ObservableEmitterPtr emitter;
    std::shared_ptr<Observable> observable;
    int32_t subscriptions = 0;
};
} // namespace

TEST(ObservableMergeTest, CoversArrayVariadicFlatteningErrorAndCancellation)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::mergeArray({Observable::just(1, 2), Observable::just(3)})
        ->subscribe(observer);
    observer->expectInt64Values({1, 2, 3});
    observer->expectComplete();

    const auto variadic = std::make_shared<TestObserver>();
    Observable::merge(Observable::just(4), Observable::just(5))->subscribe(variadic);
    variadic->expectInt64Values({4, 5});
    variadic->expectComplete();

    const auto flattened = std::make_shared<TestObserver>();
    const auto outer = Observable::just(GAny(Observable::just(6)), GAny(Observable::just(7)));
    const auto mergeOne = static_cast<std::shared_ptr<Observable> (*)(
        const std::shared_ptr<Observable> &)>(&Observable::merge);
    mergeOne(outer)->subscribe(flattened);
    flattened->expectInt64Values({6, 7});
    flattened->expectComplete();

    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::merge(Observable::just(1), Observable::error(GAnyException("merge failure")))
        ->subscribe(errorObserver);
    errorObserver->expectInt64Values({1});
    errorObserver->expectErrorContains("merge failure");

    ManualSource first;
    ManualSource second;
    const auto disposedObserver = std::make_shared<TestObserver>();
    Observable::merge(first.observable, second.observable)->subscribe(disposedObserver);
    disposedObserver->dispose();
    EXPECT_TRUE(first.disposable->isDisposed());
    EXPECT_TRUE(second.disposable->isDisposed());
    disposedObserver->expectNotTerminated();
}

TEST(ObservableConcatTest, CoversArrayVariadicOrderErrorAndCancellation)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::concatArray({Observable::just(1, 2), Observable::just(3, 4)})
        ->subscribe(observer);
    observer->expectInt64Values({1, 2, 3, 4});
    observer->expectComplete();

    const auto variadic = std::make_shared<TestObserver>();
    Observable::concat(Observable::just(5), Observable::just(6))->subscribe(variadic);
    variadic->expectInt64Values({5, 6});
    variadic->expectComplete();

    int32_t lateSubscriptions = 0;
    const auto late = Observable::create([&lateSubscriptions](const ObservableEmitterPtr &) {
        ++lateSubscriptions;
    });
    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::concat(Observable::error(GAnyException("concat failure")), late)
        ->subscribe(errorObserver);
    errorObserver->expectErrorContains("concat failure");
    EXPECT_EQ(lateSubscriptions, 0);

    ManualSource first;
    int32_t secondSubscriptions = 0;
    const auto second = Observable::create([&secondSubscriptions](const ObservableEmitterPtr &) {
        ++secondSubscriptions;
    });
    const auto disposedObserver = std::make_shared<TestObserver>();
    Observable::concat(first.observable, second)->subscribe(disposedObserver);
    disposedObserver->dispose();
    EXPECT_TRUE(first.disposable->isDisposed());
    EXPECT_EQ(secondSubscriptions, 0);
}

TEST(ObservableAmbTest, FirstSignalWinsAndCancelsOtherSources)
{
    ManualSource first;
    ManualSource second;
    const auto observer = std::make_shared<TestObserver>();
    Observable::ambArray({first.observable, second.observable})->subscribe(observer);

    second.emitter->onNext(20);
    second.emitter->onComplete();
    first.emitter->onNext(10);
    first.emitter->onError(GAnyException("loser failure"));

    observer->expectInt64Values({20});
    observer->expectComplete();
    EXPECT_TRUE(first.disposable->isDisposed());
}

TEST(ObservableAmbTest, ErrorCanWinAndEmptyArrayCompletes)
{
    ManualSource first;
    ManualSource second;
    const auto observer = std::make_shared<TestObserver>();
    Observable::amb(first.observable, second.observable)->subscribe(observer);
    first.emitter->onError(GAnyException("winner failure"));
    second.emitter->onNext(2);

    observer->expectErrorContains("winner failure");
    EXPECT_TRUE(second.disposable->isDisposed());

    const auto emptyObserver = std::make_shared<TestObserver>();
    Observable::ambArray({})->subscribe(emptyObserver);
    emptyObserver->expectComplete();
}

TEST(ObservableAmbTest, WinnerCompletionTerminatesOnceAndCancelsEverySource)
{
    const auto first = std::make_shared<TrackingDisposable>();
    const auto second = std::make_shared<TrackingDisposable>();
    const auto observer = std::make_shared<TestObserver>();
    const auto coordinator = std::make_shared<AmbCoordinator>(observer, 2);
    coordinator->onSubscribe(0, first);
    coordinator->onSubscribe(1, second);

    coordinator->onComplete(0);
    coordinator->onNext(0, 1);
    coordinator->onError(0, GAnyException("late winner failure"));
    coordinator->onComplete(0);
    coordinator->onNext(1, 2);
    coordinator->onError(1, GAnyException("loser failure"));

    observer->expectInt64Values({});
    observer->expectComplete();
    EXPECT_TRUE(first->isDisposed());
    EXPECT_TRUE(second->isDisposed());
}

TEST(ObservableAmbTest, ConcurrentFirstSignalsProduceExactlyOneWinner)
{
    const auto first = std::make_shared<TrackingDisposable>();
    const auto second = std::make_shared<TrackingDisposable>();
    const auto observer = std::make_shared<TestObserver>();
    const auto coordinator = std::make_shared<AmbCoordinator>(observer, 2);
    coordinator->onSubscribe(0, first);
    coordinator->onSubscribe(1, second);

    const auto start = std::make_shared<std::barrier<> >(3);
    const auto finished = std::make_shared<BoundedWait>(2);
    const auto race = [start, finished, coordinator](size_t index, int32_t value) {
        start->arrive_and_wait();
        coordinator->onNext(index, value);
        finished->signal();
    };
    std::thread firstThread(race, 0, 1);
    std::thread secondThread(race, 1, 2);
    start->arrive_and_wait();

    if (!finished->await(std::chrono::seconds(1))) {
        firstThread.detach();
        secondThread.detach();
        FAIL() << "amb first-signal race timed out";
        return;
    }
    firstThread.join();
    secondThread.join();

    ASSERT_EQ(observer->values().size(), 1u);
    const auto winner = observer->values()[0].toInt64();
    EXPECT_TRUE(winner == 1 || winner == 2);
    EXPECT_NE(first->isDisposed(), second->isDisposed());
    observer->expectNotTerminated();
    coordinator->dispose();
}

TEST(ObservableZipTest, ArrayPreservesRowsAndStopsAtShortestSource)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::zipArray(
        {Observable::just(1, 2), Observable::just(10, 20, 30), Observable::just(100, 200)},
        [](const std::vector<GAny> &values) {
            return values[0].toInt64() + values[1].toInt64() + values[2].toInt64();
        })
        ->subscribe(observer);

    observer->expectInt64Values({111, 222});
    observer->expectComplete();
}

TEST(ObservableZipTest, PropagatesSourceAndZipperErrorsAndCancelsPeers)
{
    ManualSource first;
    ManualSource second;
    const auto sourceError = std::make_shared<TestObserver>();
    Observable::zip(first.observable, second.observable,
                    [](const GAny &left, const GAny &right) {
                        return left.toInt64() + right.toInt64();
                    })
        ->subscribe(sourceError);
    first.emitter->onError(GAnyException("zip source failure"));
    sourceError->expectErrorContains("zip source failure");
    EXPECT_TRUE(second.disposable->isDisposed());

    const auto zipperError = std::make_shared<TestObserver>();
    Observable::zip(Observable::just(1), Observable::just(2),
                    [](const GAny &, const GAny &) -> GAny {
                        throw std::runtime_error("zipper failure");
                    })
        ->subscribe(zipperError);
    zipperError->expectErrorContains("zipper failure");

    ManualSource cancellableFirst;
    ManualSource cancellableSecond;
    ManualSource cancellableThird;
    const auto disposedObserver = std::make_shared<TestObserver>();
    Observable::zipArray(
        {cancellableFirst.observable, cancellableSecond.observable, cancellableThird.observable},
        [](const std::vector<GAny> &) { return GAny(); })
        ->subscribe(disposedObserver);
    disposedObserver->dispose();

    EXPECT_TRUE(cancellableFirst.disposable->isDisposed());
    EXPECT_TRUE(cancellableSecond.disposable->isDisposed());
    EXPECT_TRUE(cancellableThird.disposable->isDisposed());
    disposedObserver->expectNotTerminated();

    ManualSource binaryFirst;
    ManualSource binarySecond;
    const auto binaryObserver = std::make_shared<TestObserver>();
    Observable::zip(binaryFirst.observable, binarySecond.observable,
                    [](const GAny &, const GAny &) { return GAny(); })
        ->subscribe(binaryObserver);
    binaryObserver->dispose();

    EXPECT_TRUE(binaryFirst.disposable->isDisposed());
    EXPECT_TRUE(binarySecond.disposable->isDisposed());
    binaryObserver->expectNotTerminated();
}

TEST(ObservableCombineLatestTest, ArrayUsesLatestValuesUntilAllSourcesComplete)
{
    ManualSource first;
    ManualSource second;
    ManualSource third;
    const auto observer = std::make_shared<TestObserver>();
    Observable::combineLatestArray(
        {first.observable, second.observable, third.observable},
        [](const std::vector<GAny> &values) {
            return values[0].toInt64() + values[1].toInt64() + values[2].toInt64();
        })
        ->subscribe(observer);

    first.emitter->onNext(1);
    second.emitter->onNext(10);
    third.emitter->onNext(100);
    first.emitter->onNext(2);
    first.emitter->onComplete();
    second.emitter->onNext(20);
    second.emitter->onComplete();
    third.emitter->onComplete();

    observer->expectInt64Values({111, 112, 122});
    observer->expectComplete();
}

TEST(ObservableCombineLatestTest, EmptyOrErroredSourceTerminatesAndCancelsPeers)
{
    ManualSource other;
    const auto emptyObserver = std::make_shared<TestObserver>();
    Observable::combineLatest(Observable::empty(), other.observable,
                              [](const GAny &, const GAny &) { return GAny(); })
        ->subscribe(emptyObserver);
    emptyObserver->expectComplete();
    EXPECT_EQ(other.subscriptions, 0);

    ManualSource first;
    ManualSource second;
    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::combineLatest(first.observable, second.observable,
                              [](const GAny &left, const GAny &right) {
                                  return left.toInt64() + right.toInt64();
                              })
        ->subscribe(errorObserver);
    second.emitter->onError(GAnyException("combine failure"));
    errorObserver->expectErrorContains("combine failure");
    EXPECT_TRUE(first.disposable->isDisposed());

    ManualSource cancellableFirst;
    ManualSource cancellableSecond;
    ManualSource cancellableThird;
    const auto disposedObserver = std::make_shared<TestObserver>();
    Observable::combineLatestArray(
        {cancellableFirst.observable, cancellableSecond.observable, cancellableThird.observable},
        [](const std::vector<GAny> &) { return GAny(); })
        ->subscribe(disposedObserver);
    disposedObserver->dispose();

    EXPECT_TRUE(cancellableFirst.disposable->isDisposed());
    EXPECT_TRUE(cancellableSecond.disposable->isDisposed());
    EXPECT_TRUE(cancellableThird.disposable->isDisposed());
    disposedObserver->expectNotTerminated();

    ManualSource binaryFirst;
    ManualSource binarySecond;
    const auto binaryObserver = std::make_shared<TestObserver>();
    Observable::combineLatest(binaryFirst.observable, binarySecond.observable,
                              [](const GAny &, const GAny &) { return GAny(); })
        ->subscribe(binaryObserver);
    binaryObserver->dispose();

    EXPECT_TRUE(binaryFirst.disposable->isDisposed());
    EXPECT_TRUE(binarySecond.disposable->isDisposed());
    binaryObserver->expectNotTerminated();
}

TEST(ObservableTakeUntilTest, TriggerWinsBeforeMainSubscription)
{
    int32_t mainSubscriptions = 0;
    const auto main = Observable::create([&mainSubscriptions](const ObservableEmitterPtr &) {
        ++mainSubscriptions;
    });
    const auto observer = std::make_shared<TestObserver>();
    main->takeUntil(Observable::just(1))->subscribe(observer);

    observer->expectComplete();
    EXPECT_EQ(mainSubscriptions, 0);
}

TEST(ObservableTakeUntilTest, PropagatesEitherErrorAndCancelsBothSources)
{
    ManualSource main;
    ManualSource trigger;
    const auto triggerError = std::make_shared<TestObserver>();
    main.observable->takeUntil(trigger.observable)->subscribe(triggerError);
    trigger.emitter->onError(GAnyException("trigger failure"));
    triggerError->expectErrorContains("trigger failure");
    EXPECT_TRUE(main.disposable->isDisposed());

    ManualSource failingMain;
    ManualSource other;
    const auto mainError = std::make_shared<TestObserver>();
    failingMain.observable->takeUntil(other.observable)->subscribe(mainError);
    failingMain.emitter->onError(GAnyException("main failure"));
    mainError->expectErrorContains("main failure");
    EXPECT_TRUE(other.disposable->isDisposed());

    ManualSource cancellableMain;
    ManualSource cancellableTrigger;
    const auto disposedObserver = std::make_shared<TestObserver>();
    cancellableMain.observable->takeUntil(cancellableTrigger.observable)->subscribe(disposedObserver);
    disposedObserver->dispose();

    EXPECT_TRUE(cancellableMain.disposable->isDisposed());
    EXPECT_TRUE(cancellableTrigger.disposable->isDisposed());
    disposedObserver->expectNotTerminated();
}

TEST(ObservableJoinTest, MatchesValuesWhileBothDurationsRemainOpen)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::just(1, 2)
        ->join(Observable::just(10),
               [](const GAny &) { return Observable::never(); },
               [](const GAny &) { return Observable::never(); },
               [](const GAny &left, const GAny &right) {
                   return left.toInt64() + right.toInt64();
               })
        ->subscribe(observer);

    observer->expectInt64Values({11, 12});
    observer->expectComplete();
}

TEST(ObservableJoinTest, ConvertsSelectorFailureAndCancelsOtherSource)
{
    ManualSource right;
    const auto observer = std::make_shared<TestObserver>();
    Observable::just(1)
        ->join(right.observable,
               [](const GAny &) -> std::shared_ptr<Observable> {
                   throw std::runtime_error("duration failure");
               },
               [](const GAny &) { return Observable::never(); },
               [](const GAny &, const GAny &) { return GAny(); })
        ->subscribe(observer);

    observer->expectErrorContains("duration failure");
    EXPECT_EQ(right.subscriptions, 0);
}

TEST(ObservableJoinTest, ResultSelectorFailureTerminatesOnce)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::just(1)
        ->join(Observable::just(2),
               [](const GAny &) { return Observable::never(); },
               [](const GAny &) { return Observable::never(); },
               [](const GAny &, const GAny &) -> GAny {
                   throw std::runtime_error("result failure");
               })
        ->subscribe(observer);

    observer->expectErrorContains("result failure");
}

TEST(ObservableJoinTest, SourceErrorAndDownstreamCancellationDisposeBothSources)
{
    ManualSource failingLeft;
    ManualSource right;
    const auto errorObserver = std::make_shared<TestObserver>();
    failingLeft.observable
        ->join(right.observable,
               [](const GAny &) { return Observable::never(); },
               [](const GAny &) { return Observable::never(); },
               [](const GAny &, const GAny &) { return GAny(); })
        ->subscribe(errorObserver);

    failingLeft.emitter->onError(GAnyException("left failure"));

    errorObserver->expectErrorContains("left failure");
    EXPECT_TRUE(failingLeft.disposable->isDisposed());
    EXPECT_TRUE(right.disposable->isDisposed());

    ManualSource left;
    ManualSource cancellableRight;
    const auto disposedObserver = std::make_shared<TestObserver>();
    left.observable
        ->join(cancellableRight.observable,
               [](const GAny &) { return Observable::never(); },
               [](const GAny &) { return Observable::never(); },
               [](const GAny &, const GAny &) { return GAny(); })
        ->subscribe(disposedObserver);
    disposedObserver->dispose();

    EXPECT_TRUE(left.disposable->isDisposed());
    EXPECT_TRUE(cancellableRight.disposable->isDisposed());
    disposedObserver->expectNotTerminated();
}

TEST(ObservableJoinTest, DurationClosureExcludesOldValuesAndDurationErrorDisposesSources)
{
    ManualSource left;
    ManualSource right;
    ManualSource leftDuration;
    const auto observer = std::make_shared<TestObserver>();
    left.observable
        ->join(right.observable,
               [&leftDuration](const GAny &) { return leftDuration.observable; },
               [](const GAny &) { return Observable::never(); },
               [](const GAny &leftValue, const GAny &rightValue) {
                   return leftValue.toInt64() + rightValue.toInt64();
               })
        ->subscribe(observer);

    left.emitter->onNext(1);
    ASSERT_NE(leftDuration.emitter, nullptr);
    leftDuration.emitter->onComplete();
    right.emitter->onNext(10);
    left.emitter->onComplete();
    right.emitter->onComplete();

    observer->expectInt64Values({});
    observer->expectComplete();

    ManualSource failingLeft;
    ManualSource cancellableRight;
    ManualSource failingDuration;
    const auto errorObserver = std::make_shared<TestObserver>();
    failingLeft.observable
        ->join(cancellableRight.observable,
               [&failingDuration](const GAny &) { return failingDuration.observable; },
               [](const GAny &) { return Observable::never(); },
               [](const GAny &, const GAny &) { return GAny(); })
        ->subscribe(errorObserver);

    failingLeft.emitter->onNext(1);
    ASSERT_NE(failingDuration.emitter, nullptr);
    failingDuration.emitter->onError(GAnyException("duration source failure"));

    errorObserver->expectErrorContains("duration source failure");
    EXPECT_TRUE(failingLeft.disposable->isDisposed());
    EXPECT_TRUE(cancellableRight.disposable->isDisposed());
    EXPECT_TRUE(failingDuration.disposable->isDisposed());
}

TEST(ObservableSequenceEqualTest, CoversEqualDifferentLengthAndComparatorFailure)
{
    const auto equal = std::make_shared<TestObserver>();
    Observable::sequenceEqual(Observable::just(1, 2), Observable::just(1, 2), nullptr, 1)
        ->subscribe(equal);
    ASSERT_EQ(equal->values().size(), 1u);
    EXPECT_TRUE(equal->values()[0].toBool());
    equal->expectComplete();

    const auto different = std::make_shared<TestObserver>();
    Observable::sequenceEqual(Observable::just(1), Observable::just(1, 2))->subscribe(different);
    ASSERT_EQ(different->values().size(), 1u);
    EXPECT_FALSE(different->values()[0].toBool());
    different->expectComplete();

    const auto comparatorError = std::make_shared<TestObserver>();
    Observable::sequenceEqual(
        Observable::just(1), Observable::just(1),
        [](const GAny &, const GAny &) -> GAny {
            throw std::runtime_error("comparator failure");
        })
        ->subscribe(comparatorError);
    comparatorError->expectErrorContains("comparator failure");
}

TEST(ObservableSequenceEqualTest, EarlyDifferenceCancelsBothSourcesAndTerminatesOnce)
{
    ManualSource first;
    ManualSource second;
    const auto observer = std::make_shared<TestObserver>();
    Observable::sequenceEqual(first.observable, second.observable)->subscribe(observer);

    first.emitter->onNext(1);
    second.emitter->onNext(2);
    first.emitter->onComplete();
    second.emitter->onError(GAnyException("late failure"));

    ASSERT_EQ(observer->values().size(), 1u);
    EXPECT_FALSE(observer->values()[0].toBool());
    observer->expectComplete();
    EXPECT_TRUE(first.disposable->isDisposed());
    EXPECT_TRUE(second.disposable->isDisposed());
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

TEST(ObservableFlatMapRegressionTest, SynchronousInnerReentryKeepsSingleTermination)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::just(1)
        ->flatMap([](const GAny &) {
            return Observable::create([](const ObservableEmitterPtr &emitter) {
                emitter->onNext(10);
                emitter->onComplete();
                emitter->onError(GAnyException("late failure"));
            });
        })
        ->subscribe(observer);

    observer->expectInt64Values({10});
    observer->expectComplete();
}

TEST(ObservableSwitchMapRegressionTest, SynchronousInnerReentryKeepsSingleTermination)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::just(1)
        ->switchMap([](const GAny &) {
            return Observable::create([](const ObservableEmitterPtr &emitter) {
                emitter->onNext(10);
                emitter->onComplete();
                emitter->onError(GAnyException("late failure"));
            });
        })
        ->subscribe(observer);

    observer->expectInt64Values({10});
    observer->expectComplete();
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
