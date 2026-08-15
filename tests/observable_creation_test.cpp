#include <gtest/gtest.h>

#include "support/test_observer.h"

#include <rx/rx.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace
{
using namespace rx;
using namespace rx::test;

class DisposeOnSubscribeObserver : public TestObserver
{
public:
    void onSubscribe(const DisposablePtr &disposable) override
    {
        TestObserver::onSubscribe(disposable);
        disposable->dispose();
    }
};
} // namespace

TEST(ObservableCreateTest, HonorsTerminationAndDisposal)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::create([](const ObservableEmitterPtr &emitter) {
        emitter->onNext(1);
        emitter->onComplete();
        emitter->onNext(2);
        emitter->onError(GAnyException("late failure"));
    })->subscribe(observer);

    observer->expectInt64Values({1});
    observer->expectComplete();

    const auto disposedObserver = std::make_shared<DisposeOnSubscribeObserver>();
    Observable::create([](const ObservableEmitterPtr &emitter) {
        emitter->onNext(1);
        emitter->onComplete();
    })->subscribe(disposedObserver);

    disposedObserver->expectInt64Values({});
    disposedObserver->expectNotTerminated();
}

TEST(ObservableCreateTest, ConvertsSourceExceptionToSingleError)
{
    const auto observer = std::make_shared<TestObserver>();
    EXPECT_NO_THROW(Observable::create([](const ObservableEmitterPtr &) {
        throw std::runtime_error("source failure");
    })->subscribe(observer));

    observer->expectInt64Values({});
    observer->expectErrorContains("source failure");
}

TEST(ObservableEmptyTest, CompletesWithoutValues)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::empty()->subscribe(observer);

    observer->expectInt64Values({});
    observer->expectComplete();
}

TEST(ObservableFromArrayTest, EmitsValuesInOrderAndSupportsEmptyInput)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::fromArray({1, 2, 3})->subscribe(observer);
    observer->expectInt64Values({1, 2, 3});
    observer->expectComplete();

    const auto emptyObserver = std::make_shared<TestObserver>();
    Observable::fromArray({})->subscribe(emptyObserver);
    emptyObserver->expectInt64Values({});
    emptyObserver->expectComplete();
}

TEST(ObservableFromArrayTest, StopsImmediatelyWhenDisposedOnSubscribe)
{
    const auto observer = std::make_shared<DisposeOnSubscribeObserver>();
    Observable::fromArray({1, 2, 3})->subscribe(observer);

    observer->expectInt64Values({});
    observer->expectNotTerminated();
}

TEST(ObservableJustTest, EmitsSingleAndMultipleValuesInOrder)
{
    const auto singleObserver = std::make_shared<TestObserver>();
    Observable::just(7)->subscribe(singleObserver);
    singleObserver->expectInt64Values({7});
    singleObserver->expectComplete();

    const auto multipleObserver = std::make_shared<TestObserver>();
    Observable::just(1, 2, 3)->subscribe(multipleObserver);
    multipleObserver->expectInt64Values({1, 2, 3});
    multipleObserver->expectComplete();

    const auto emptyObserver = std::make_shared<TestObserver>();
    Observable::just()->subscribe(emptyObserver);
    emptyObserver->expectInt64Values({});
    emptyObserver->expectComplete();
}

TEST(ObservableNeverTest, OnlySignalsSubscription)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::never()->subscribe(observer);

    observer->expectInt64Values({});
    observer->expectNotTerminated();
    ASSERT_EQ(observer->events().size(), 1u);
    EXPECT_EQ(observer->events()[0].type, ObserverEventType::Subscribe);
}

TEST(ObservableErrorTest, ForwardsFailureWithoutCompletion)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::error(GAnyException("expected failure"))->subscribe(observer);

    observer->expectInt64Values({});
    observer->expectErrorContains("expected failure");
}

TEST(ObservableDeferTest, SubscribesToConfiguredSourceForEveryObserver)
{
    int32_t subscriptions = 0;
    const auto source = Observable::create([&subscriptions](const ObservableEmitterPtr &emitter) {
        emitter->onNext(++subscriptions);
        emitter->onComplete();
    });
    const auto deferred = Observable::defer(source);

    const auto first = std::make_shared<TestObserver>();
    const auto second = std::make_shared<TestObserver>();
    deferred->subscribe(first);
    deferred->subscribe(second);

    first->expectInt64Values({1});
    first->expectComplete();
    second->expectInt64Values({2});
    second->expectComplete();
}

TEST(ObservableFromCallableTest, EmitsResultAndConvertsExceptionToError)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::fromCallable([] { return GAny(42); })->subscribe(observer);
    observer->expectInt64Values({42});
    observer->expectComplete();

    const auto errorObserver = std::make_shared<TestObserver>();
    EXPECT_NO_THROW(Observable::fromCallable([]() -> GAny {
        throw std::runtime_error("callable failure");
    })->subscribe(errorObserver));
    errorObserver->expectInt64Values({});
    errorObserver->expectErrorContains("callable failure");
}
