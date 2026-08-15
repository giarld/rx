#include <gtest/gtest.h>

#include "support/test_observer.h"

#include <rx/disposables/atomic_disposable.h>
#include <rx/disposables/disposable_helper.h>
#include <rx/disposables/sequential_disposable.h>
#include <rx/operators/observable_create.h>
#include <rx/rx.h>

#include <atomic>
#include <barrier>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{
using namespace rx;
using namespace rx::test;

class CountingDisposable : public Disposable
{
public:
    void dispose() override
    {
        ++mDisposeCount;
    }

    bool isDisposed() const override
    {
        return mDisposeCount.load() != 0;
    }

    int32_t disposeCount() const
    {
        return mDisposeCount.load();
    }

private:
    std::atomic<int32_t> mDisposeCount = 0;
};

class ThrowingObserver : public Observer
{
public:
    enum class Callback
    {
        Next,
        Error,
        Complete
    };

    explicit ThrowingObserver(Callback callback)
        : mCallback(callback)
    {
    }

public:
    void onSubscribe(const DisposablePtr &) override
    {
    }

    void onNext(const GAny &) override
    {
        if (mCallback == Callback::Next) {
            throw std::runtime_error("next callback failure");
        }
    }

    void onError(const GAnyException &) override
    {
        if (mCallback == Callback::Error) {
            throw std::runtime_error("error callback failure");
        }
    }

    void onComplete() override
    {
        if (mCallback == Callback::Complete) {
            throw std::runtime_error("complete callback failure");
        }
    }

private:
    Callback mCallback;
};

class LifetimeObservable : public Observable
{
public:
    explicit LifetimeObservable(int32_t &destructionCount)
        : mDestructionCount(destructionCount)
    {
    }

    ~LifetimeObservable() override
    {
        ++mDestructionCount;
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        observer->onSubscribe(std::make_shared<CountingDisposable>());
        observer->onComplete();
    }

private:
    int32_t &mDestructionCount;
};
} // namespace

TEST(ObservableLifetimeTest, BasePointerUsesTheVirtualDestructor)
{
    int32_t destructionCount = 0;
    std::shared_ptr<Observable> observable = std::make_shared<LifetimeObservable>(destructionCount);

    observable.reset();

    EXPECT_EQ(destructionCount, 1);
}

TEST(ObservableSubscribeTest, ObserverOverloadDelegatesToTheSource)
{
    const auto observer = std::make_shared<TestObserver>();

    Observable::just(1, 2)->subscribe(observer);

    observer->expectInt64Values({1, 2});
    observer->expectComplete();
}

TEST(ObservableSubscribeTest, LambdaOverloadsRouteSignalsAndReturnDisposable)
{
    std::vector<int64_t> values;
    int32_t completionCount = 0;
    const auto completed = Observable::just(1, 2)->subscribe(
        [&values](const GAny &value) { values.push_back(value.toInt64()); },
        nullptr,
        [&completionCount] { ++completionCount; });

    EXPECT_EQ(values, (std::vector<int64_t>{1, 2}));
    EXPECT_EQ(completionCount, 1);
    EXPECT_TRUE(completed->isDisposed());

    int32_t nextCount = 0;
    const auto active = Observable::never()->subscribe([&nextCount](const GAny &) { ++nextCount; });
    EXPECT_FALSE(active->isDisposed());
    active->dispose();
    EXPECT_TRUE(active->isDisposed());
    EXPECT_EQ(nextCount, 0);
}

TEST(ObserverContractTest, SubscriptionPrecedesSignalsAndEmitterStopsAfterTermination)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::create([](const ObservableEmitterPtr &emitter) {
        emitter->onNext(1);
        emitter->onComplete();
        emitter->onNext(2);
        emitter->onError(GAnyException("late failure"));
    })->subscribe(observer);

    const auto events = observer->events();
    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0].type, ObserverEventType::Subscribe);
    EXPECT_EQ(events[1].type, ObserverEventType::Next);
    EXPECT_EQ(events[2].type, ObserverEventType::Complete);
    observer->expectInt64Values({1});
    observer->expectComplete();
}

TEST(LambdaObserverTest, OnNextFailureBecomesOneErrorAndDisposesUpstream)
{
    const auto upstream = std::make_shared<CountingDisposable>();
    int32_t nextCount = 0;
    int32_t errorCount = 0;
    std::string error;
    int32_t completionCount = 0;
    const auto observer = std::make_shared<LambdaObserver>(
        [&nextCount](const GAny &) {
            ++nextCount;
            throw std::runtime_error("next failure");
        },
        [&errorCount, &error](const GAnyException &e) {
            ++errorCount;
            error = e.toString();
        },
        [&completionCount] { ++completionCount; },
        nullptr);

    observer->onSubscribe(upstream);
    observer->onNext(1);
    observer->onNext(2);
    observer->onComplete();

    EXPECT_EQ(nextCount, 1);
    EXPECT_EQ(errorCount, 1);
    EXPECT_NE(error.find("next failure"), std::string::npos);
    EXPECT_EQ(completionCount, 0);
    EXPECT_EQ(upstream->disposeCount(), 1);
    EXPECT_TRUE(observer->isDisposed());
}

TEST(LambdaObserverTest, SubscribeAndCompleteFailuresUseTheErrorCallbackOnce)
{
    const auto subscribeUpstream = std::make_shared<CountingDisposable>();
    int32_t subscribeErrors = 0;
    const auto subscribeObserver = std::make_shared<LambdaObserver>(
        nullptr,
        [&subscribeErrors](const GAnyException &e) {
            ++subscribeErrors;
            EXPECT_NE(e.toString().find("subscribe failure"), std::string::npos);
        },
        nullptr,
        [](const DisposablePtr &) { throw std::runtime_error("subscribe failure"); });
    subscribeObserver->onSubscribe(subscribeUpstream);

    EXPECT_EQ(subscribeErrors, 1);
    EXPECT_EQ(subscribeUpstream->disposeCount(), 1);
    EXPECT_TRUE(subscribeObserver->isDisposed());

    const auto completeUpstream = std::make_shared<CountingDisposable>();
    int32_t completeErrors = 0;
    const auto completeObserver = std::make_shared<LambdaObserver>(
        nullptr,
        [&completeErrors](const GAnyException &e) {
            ++completeErrors;
            EXPECT_NE(e.toString().find("complete failure"), std::string::npos);
        },
        [] { throw std::runtime_error("complete failure"); },
        nullptr);
    completeObserver->onSubscribe(completeUpstream);
    completeObserver->onComplete();
    completeObserver->onComplete();

    EXPECT_EQ(completeErrors, 1);
    EXPECT_TRUE(completeObserver->isDisposed());
}

TEST(LambdaObserverTest, ErrorCallbackFailureIsContainedAndTerminatesOnce)
{
    const auto upstream = std::make_shared<CountingDisposable>();
    int32_t errorCount = 0;
    int32_t nextCount = 0;
    int32_t completeCount = 0;
    const auto observer = std::make_shared<LambdaObserver>(
        [&nextCount](const GAny &) { ++nextCount; },
        [&errorCount](const GAnyException &) {
            ++errorCount;
            throw std::runtime_error("error callback failure");
        },
        [&completeCount] { ++completeCount; },
        nullptr);

    observer->onSubscribe(upstream);
    EXPECT_NO_THROW(observer->onError(GAnyException("source failure")));
    observer->onNext(1);
    observer->onError(GAnyException("late failure"));
    observer->onComplete();

    EXPECT_EQ(errorCount, 1);
    EXPECT_EQ(nextCount, 0);
    EXPECT_EQ(completeCount, 0);
    EXPECT_TRUE(observer->isDisposed());
}

TEST(ObservableEmitterTest, ReplacesDisposableAndReleasesTheActiveOneOnTermination)
{
    ObservableEmitterPtr emitter;
    const auto observer = std::make_shared<TestObserver>();
    Observable::create([&emitter](const ObservableEmitterPtr &value) { emitter = value; })
        ->subscribe(observer);

    const auto first = std::make_shared<CountingDisposable>();
    const auto second = std::make_shared<CountingDisposable>();
    emitter->setDisposable(first);
    emitter->setDisposable(second);

    EXPECT_EQ(first->disposeCount(), 1);
    EXPECT_EQ(second->disposeCount(), 0);

    emitter->onError(GAnyException("expected failure"));
    emitter->onComplete();
    emitter->onNext(1);

    EXPECT_EQ(second->disposeCount(), 1);
    EXPECT_TRUE(emitter->isDisposed());
    observer->expectInt64Values({});
    observer->expectErrorContains("expected failure");
}

TEST(ObservableEmitterTest, DownstreamCallbackFailuresDoNotEscape)
{
    const auto nextEmitter =
        std::make_shared<CreateEmitter>(std::make_shared<ThrowingObserver>(ThrowingObserver::Callback::Next));
    EXPECT_NO_THROW(nextEmitter->onNext(1));
    EXPECT_TRUE(nextEmitter->isDisposed());

    const auto errorEmitter =
        std::make_shared<CreateEmitter>(std::make_shared<ThrowingObserver>(ThrowingObserver::Callback::Error));
    EXPECT_NO_THROW(errorEmitter->onError(GAnyException("source failure")));
    EXPECT_TRUE(errorEmitter->isDisposed());

    const auto completeEmitter = std::make_shared<CreateEmitter>(
        std::make_shared<ThrowingObserver>(ThrowingObserver::Callback::Complete));
    EXPECT_NO_THROW(completeEmitter->onComplete());
    EXPECT_TRUE(completeEmitter->isDisposed());
}

TEST(AtomicDisposableTest, DisposeIsIdempotentAndVisibleAcrossThreads)
{
    AtomicDisposable disposable;
    std::vector<std::thread> threads;
    for (int32_t i = 0; i < 8; ++i) {
        threads.emplace_back([&disposable] { disposable.dispose(); });
    }
    for (auto &thread: threads) {
        thread.join();
    }

    EXPECT_TRUE(disposable.isDisposed());
    disposable.dispose();
    EXPECT_TRUE(disposable.isDisposed());
}

TEST(DisposableHelperTest, SetReplaceAndDisposeFollowOwnershipRules)
{
    GMutex lock;
    DisposablePtr field;
    const auto first = std::make_shared<CountingDisposable>();
    const auto second = std::make_shared<CountingDisposable>();
    const auto third = std::make_shared<CountingDisposable>();

    EXPECT_TRUE(DisposableHelper::setOnce(field, first, lock));
    EXPECT_TRUE(DisposableHelper::set(field, second, lock));
    EXPECT_EQ(first->disposeCount(), 1);
    EXPECT_TRUE(DisposableHelper::replace(field, third, lock));
    EXPECT_EQ(second->disposeCount(), 0);
    EXPECT_TRUE(DisposableHelper::dispose(field, lock));
    EXPECT_EQ(third->disposeCount(), 1);
    EXPECT_FALSE(DisposableHelper::dispose(field, lock));
    EXPECT_TRUE(DisposableHelper::isDisposed(field));

    const auto late = std::make_shared<CountingDisposable>();
    EXPECT_FALSE(DisposableHelper::trySet(field, late, lock));
    EXPECT_EQ(late->disposeCount(), 1);
}

TEST(DisposableHelperTest, RejectsAssignmentsAfterDisposal)
{
    GMutex lock;
    DisposablePtr field = DisposableHelper::disposed();
    const auto setCandidate = std::make_shared<CountingDisposable>();
    const auto setOnceCandidate = std::make_shared<CountingDisposable>();
    const auto replaceCandidate = std::make_shared<CountingDisposable>();

    EXPECT_FALSE(DisposableHelper::set(field, setCandidate, lock));
    EXPECT_FALSE(DisposableHelper::setOnce(field, setOnceCandidate, lock));
    EXPECT_FALSE(DisposableHelper::replace(field, replaceCandidate, lock));
    EXPECT_FALSE(DisposableHelper::set(field, nullptr, lock));
    EXPECT_FALSE(DisposableHelper::replace(field, nullptr, lock));

    EXPECT_EQ(setCandidate->disposeCount(), 1);
    EXPECT_EQ(setOnceCandidate->disposeCount(), 1);
    EXPECT_EQ(replaceCandidate->disposeCount(), 1);
    EXPECT_TRUE(DisposableHelper::isDisposed(field));

    const auto clearedBySet = std::make_shared<CountingDisposable>();
    DisposablePtr setField = clearedBySet;
    EXPECT_TRUE(DisposableHelper::set(setField, nullptr, lock));
    EXPECT_EQ(clearedBySet->disposeCount(), 1);
    EXPECT_EQ(setField, nullptr);

    const auto clearedByReplace = std::make_shared<CountingDisposable>();
    DisposablePtr replaceField = clearedByReplace;
    EXPECT_TRUE(DisposableHelper::replace(replaceField, nullptr, lock));
    EXPECT_EQ(clearedByReplace->disposeCount(), 0);
    EXPECT_EQ(replaceField, nullptr);
}

TEST(DisposableHelperTest, TrySetAndValidateCoverNonViolatingBranches)
{
    GMutex lock;
    DisposablePtr field;
    const auto first = std::make_shared<CountingDisposable>();
    const auto rejected = std::make_shared<CountingDisposable>();

    EXPECT_FALSE(DisposableHelper::trySet(field, nullptr, lock));
    EXPECT_TRUE(DisposableHelper::trySet(field, first, lock));
    EXPECT_FALSE(DisposableHelper::trySet(field, nullptr, lock));
    EXPECT_FALSE(DisposableHelper::trySet(field, rejected, lock));
    EXPECT_EQ(rejected->disposeCount(), 1);
    EXPECT_EQ(first->disposeCount(), 0);

    EXPECT_TRUE(DisposableHelper::validate(nullptr, first));

    EXPECT_TRUE(DisposableHelper::dispose(field, lock));
    EXPECT_EQ(first->disposeCount(), 1);
}

TEST(DisposableHelperTest, DisposeRacingTrySetDisposesTheCandidateExactlyOnce)
{
    for (int32_t i = 0; i < 32; ++i) {
        GMutex lock;
        DisposablePtr field;
        const auto candidate = std::make_shared<CountingDisposable>();
        std::barrier start(2);
        std::thread setter([&] {
            start.arrive_and_wait();
            DisposableHelper::trySet(field, candidate, lock);
        });
        std::thread disposer([&] {
            start.arrive_and_wait();
            DisposableHelper::dispose(field, lock);
        });
        setter.join();
        disposer.join();

        EXPECT_TRUE(DisposableHelper::isDisposed(field));
        EXPECT_EQ(candidate->disposeCount(), 1);
    }
}

TEST(SequentialDisposableTest, UpdateReplaceAndLateAssignmentFollowOwnershipRules)
{
    const auto first = std::make_shared<CountingDisposable>();
    const auto second = std::make_shared<CountingDisposable>();
    const auto third = std::make_shared<CountingDisposable>();
    SequentialDisposable disposable(first);

    EXPECT_TRUE(disposable.update(second));
    EXPECT_EQ(first->disposeCount(), 1);
    EXPECT_TRUE(disposable.replace(third));
    EXPECT_EQ(second->disposeCount(), 0);

    disposable.dispose();
    disposable.dispose();
    EXPECT_TRUE(disposable.isDisposed());
    EXPECT_EQ(third->disposeCount(), 1);

    const auto late = std::make_shared<CountingDisposable>();
    EXPECT_FALSE(disposable.update(late));
    EXPECT_EQ(late->disposeCount(), 1);
}

TEST(SequentialDisposableTest, ConcurrentUpdatesDisposeEveryCandidateExactlyOnce)
{
    SequentialDisposable disposable;
    std::vector<std::shared_ptr<CountingDisposable>> candidates;
    std::vector<std::thread> threads;
    for (int32_t i = 0; i < 16; ++i) {
        candidates.push_back(std::make_shared<CountingDisposable>());
    }
    for (const auto &candidate: candidates) {
        threads.emplace_back([&disposable, candidate] { disposable.update(candidate); });
    }
    for (auto &thread: threads) {
        thread.join();
    }
    disposable.dispose();

    for (const auto &candidate: candidates) {
        EXPECT_EQ(candidate->disposeCount(), 1);
    }
}
