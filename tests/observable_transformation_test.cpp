#include <gtest/gtest.h>

#include "support/test_observer.h"

#include <rx/rx.h>
#include <rx/disposables/atomic_disposable.h>
#include <rx/operators/observable_switch_map.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace
{
using namespace rx;
using namespace rx::test;

class DisposeAfterFirstObserver : public TestObserver
{
public:
    void onNext(const GAny &value) override
    {
        TestObserver::onNext(value);
        dispose();
    }
};

class DisposeOnSubscribeObserver : public TestObserver
{
public:
    void onSubscribe(const DisposablePtr &disposable) override
    {
        TestObserver::onSubscribe(disposable);
        disposable->dispose();
    }
};

std::vector<std::vector<int64_t> > nestedInt64Values(const TestObserver &observer)
{
    std::vector<std::vector<int64_t> > result;
    for (const auto &value: observer.values()) {
        std::vector<int64_t> row;
        for (const auto &item: value.castAs<std::vector<GAny> >()) {
            row.push_back(item.toInt64());
        }
        result.push_back(std::move(row));
    }
    return result;
}
} // namespace

TEST(ObservableMapTest, TransformsValuesAndForwardsUpstreamError)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::range(1, 4)
        ->map([](const GAny &value) { return value.toInt64() * 10; })
        ->subscribe(observer);
    observer->expectInt64Values({10, 20, 30, 40});
    observer->expectComplete();

    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))
        ->map([](const GAny &value) { return value; })
        ->subscribe(errorObserver);
    errorObserver->expectErrorContains("upstream failure");
}

TEST(ObservableMapTest, ConvertsMapperExceptionAndStopsFurtherSignals)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::range(1, 3)
        ->map([](const GAny &) -> GAny { throw std::runtime_error("mapper failure"); })
        ->subscribe(observer);

    observer->expectInt64Values({});
    observer->expectErrorContains("mapper failure");
}

TEST(ObservableMapTest, StopsWhenDownstreamDisposes)
{
    const auto observer = std::make_shared<DisposeAfterFirstObserver>();
    Observable::range(1, 5)
        ->map([](const GAny &value) { return value; })
        ->subscribe(observer);

    observer->expectInt64Values({1});
    observer->expectNotTerminated();
}

TEST(ObservableFlatMapTest, MergesInnerSourcesAndConvertsMapperException)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::just(1, 2)
        ->flatMap([](const GAny &value) {
            const auto number = value.toInt64();
            return Observable::just(number, number * 10);
        })
        ->subscribe(observer);
    observer->expectInt64Values({1, 10, 2, 20});
    observer->expectComplete();

    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::just(1, 2)
        ->flatMap([](const GAny &) -> std::shared_ptr<Observable> {
            throw std::runtime_error("flat mapper failure");
        })
        ->subscribe(errorObserver);
    errorObserver->expectErrorContains("flat mapper failure");
}

TEST(ObservableFlatMapTest, ForwardsUpstreamErrorAndStopsWhenDisposed)
{
    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))
        ->flatMap([](const GAny &value) { return Observable::just(value); })
        ->subscribe(errorObserver);
    errorObserver->expectErrorContains("upstream failure");

    const auto disposedObserver = std::make_shared<DisposeAfterFirstObserver>();
    Observable::just(1, 2)
        ->flatMap([](const GAny &value) { return Observable::just(value, value); })
        ->subscribe(disposedObserver);
    disposedObserver->expectInt64Values({1});
    disposedObserver->expectNotTerminated();
}

TEST(ObservableConcatMapTest, PreservesInnerOrderAndForwardsInnerError)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::just(1, 2)
        ->concatMap([](const GAny &value) {
            const auto number = value.toInt64();
            return Observable::just(number, number * 10);
        })
        ->subscribe(observer);
    observer->expectInt64Values({1, 10, 2, 20});
    observer->expectComplete();

    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::just(1)
        ->concatMap([](const GAny &) { return Observable::error(GAnyException("inner failure")); })
        ->subscribe(errorObserver);
    errorObserver->expectErrorContains("inner failure");
}

TEST(ObservableConcatMapTest, ConvertsMapperExceptionAndStopsWhenDisposed)
{
    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::just(1, 2)
        ->concatMap([](const GAny &) -> std::shared_ptr<Observable> {
            throw std::runtime_error("concat mapper failure");
        })
        ->subscribe(errorObserver);
    errorObserver->expectErrorContains("concat mapper failure");

    const auto disposedObserver = std::make_shared<DisposeAfterFirstObserver>();
    Observable::just(1, 2)
        ->concatMap([](const GAny &value) { return Observable::just(value, value); })
        ->subscribe(disposedObserver);
    disposedObserver->expectInt64Values({1});
    disposedObserver->expectNotTerminated();
}

TEST(ObservableSwitchMapTest, SwitchesToLatestInnerAndWaitsForItAfterUpstreamCompletion)
{
    const auto upstreamDisposable = std::make_shared<AtomicDisposable>();
    const auto firstDisposable = std::make_shared<AtomicDisposable>();
    const auto secondDisposable = std::make_shared<AtomicDisposable>();
    const auto observer = std::make_shared<TestObserver>();
    const auto parent = std::make_shared<SwitchMapObserver>(
        observer, [firstDisposable, secondDisposable](const GAny &value) {
            if (value.toInt64() == 1) {
                return Observable::create([firstDisposable](const ObservableEmitterPtr &emitter) {
                    emitter->setDisposable(firstDisposable);
                });
            }
            return Observable::create([secondDisposable](const ObservableEmitterPtr &emitter) {
                emitter->setDisposable(secondDisposable);
            });
        });

    parent->onSubscribe(upstreamDisposable);
    parent->onNext(1);
    parent->innerNext(1, 10);

    parent->onNext(2);
    EXPECT_TRUE(firstDisposable->isDisposed());
    parent->innerNext(1, 11);
    parent->innerError(1, GAnyException("late inner failure"));

    parent->onComplete();
    EXPECT_FALSE(secondDisposable->isDisposed());
    observer->expectInt64Values({10});
    observer->expectNotTerminated();

    parent->innerNext(2, 20);
    parent->innerComplete(2);
    observer->expectInt64Values({10, 20});
    observer->expectComplete();

    parent->dispose();
    EXPECT_TRUE(upstreamDisposable->isDisposed());
    EXPECT_TRUE(secondDisposable->isDisposed());
}

TEST(ObservableSwitchMapTest, ForwardsInnerError)
{
    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::just(1)
        ->switchMap([](const GAny &) { return Observable::error(GAnyException("inner failure")); })
        ->subscribe(errorObserver);
    errorObserver->expectErrorContains("inner failure");
}

TEST(ObservableSwitchMapTest, ConvertsMapperExceptionAndStopsWhenDisposed)
{
    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::just(1, 2)
        ->switchMap([](const GAny &) -> std::shared_ptr<Observable> {
            throw std::runtime_error("switch mapper failure");
        })
        ->subscribe(errorObserver);
    errorObserver->expectErrorContains("switch mapper failure");

    const auto disposedObserver = std::make_shared<DisposeAfterFirstObserver>();
    Observable::just(1, 2)
        ->switchMap([](const GAny &value) { return Observable::just(value, value); })
        ->subscribe(disposedObserver);
    disposedObserver->expectInt64Values({1});
    disposedObserver->expectNotTerminated();
}

TEST(ObservableBufferTest, SupportsExactOverlappingAndInvalidBoundaries)
{
    const auto exact = std::make_shared<TestObserver>();
    Observable::range(1, 5)->buffer(2)->subscribe(exact);
    EXPECT_EQ(nestedInt64Values(*exact),
              std::vector<std::vector<int64_t> >({{1, 2}, {3, 4}, {5}}));
    exact->expectComplete();

    const auto overlapping = std::make_shared<TestObserver>();
    Observable::range(1, 4)->buffer(3, 1)->subscribe(overlapping);
    EXPECT_EQ(nestedInt64Values(*overlapping),
              std::vector<std::vector<int64_t> >({{1, 2, 3}, {2, 3, 4}, {3, 4}, {4}}));
    overlapping->expectComplete();

    EXPECT_THROW(Observable::just(1)->buffer(0), GAnyException);
    EXPECT_THROW(Observable::just(1)->buffer(1, 0), GAnyException);
}

TEST(ObservableBufferTest, ForwardsUpstreamErrorAndStopsWhenDisposed)
{
    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))->buffer(2)->subscribe(errorObserver);
    errorObserver->expectInt64Values({});
    errorObserver->expectErrorContains("upstream failure");

    const auto disposedObserver = std::make_shared<DisposeAfterFirstObserver>();
    Observable::range(1, 6)->buffer(2)->subscribe(disposedObserver);
    EXPECT_EQ(nestedInt64Values(*disposedObserver), std::vector<std::vector<int64_t> >({{1, 2}}));
    disposedObserver->expectNotTerminated();
}

TEST(ObservableToArrayTest, CollectsValuesAndEmitsEmptyArray)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::range(1, 3)->toArray()->subscribe(observer);
    EXPECT_EQ(nestedInt64Values(*observer), std::vector<std::vector<int64_t> >({{1, 2, 3}}));
    observer->expectComplete();

    const auto emptyObserver = std::make_shared<TestObserver>();
    Observable::empty()->toArray()->subscribe(emptyObserver);
    EXPECT_EQ(nestedInt64Values(*emptyObserver), std::vector<std::vector<int64_t> >({{}}));
    emptyObserver->expectComplete();
}

TEST(ObservableToArrayTest, ForwardsUpstreamErrorAndHonorsEarlyDisposal)
{
    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))->toArray()->subscribe(errorObserver);
    errorObserver->expectInt64Values({});
    errorObserver->expectErrorContains("upstream failure");

    const auto disposedObserver = std::make_shared<DisposeOnSubscribeObserver>();
    Observable::range(1, 3)->toArray()->subscribe(disposedObserver);
    disposedObserver->expectInt64Values({});
    disposedObserver->expectNotTerminated();
}

TEST(ObservableStartWithTest, SupportsSingleArrayAndVariadicPrefixes)
{
    const auto single = std::make_shared<TestObserver>();
    Observable::just(3)->startWith(2)->subscribe(single);
    single->expectInt64Values({2, 3});
    single->expectComplete();

    const auto array = std::make_shared<TestObserver>();
    Observable::just(3)->startWithArray({1, 2})->subscribe(array);
    array->expectInt64Values({1, 2, 3});
    array->expectComplete();

    const auto variadic = std::make_shared<TestObserver>();
    Observable::just(3)->startWith(1, 2)->subscribe(variadic);
    variadic->expectInt64Values({1, 2, 3});
    variadic->expectComplete();
}

TEST(ObservableDoOnEachTest, InvokesConvenienceCallbacksAndFinallyOnce)
{
    int32_t nextCalls = 0;
    int32_t completeCalls = 0;
    int32_t subscribeCalls = 0;
    int32_t finallyCalls = 0;
    const auto observer = std::make_shared<TestObserver>();

    Observable::just(1, 2)
        ->doOnNext([&nextCalls](const GAny &) { ++nextCalls; })
        ->doOnComplete([&completeCalls] { ++completeCalls; })
        ->doOnSubscribe([&subscribeCalls](const DisposablePtr &) { ++subscribeCalls; })
        ->doFinally([&finallyCalls] { ++finallyCalls; })
        ->subscribe(observer);

    observer->expectInt64Values({1, 2});
    observer->expectComplete();
    EXPECT_EQ(nextCalls, 2);
    EXPECT_EQ(completeCalls, 1);
    EXPECT_EQ(subscribeCalls, 1);
    EXPECT_EQ(finallyCalls, 1);

    int32_t errorCalls = 0;
    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))
        ->doOnError([&errorCalls](const GAnyException &) { ++errorCalls; })
        ->subscribe(errorObserver);
    errorObserver->expectErrorContains("upstream failure");
    EXPECT_EQ(errorCalls, 1);
}

TEST(ObservableDoOnEachTest, RunsFinallyOnceAfterErrorOrDisposal)
{
    int32_t errorFinallyCalls = 0;
    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))
        ->doFinally([&errorFinallyCalls] { ++errorFinallyCalls; })
        ->subscribe(errorObserver);
    errorObserver->expectErrorContains("upstream failure");
    errorObserver->dispose();
    EXPECT_EQ(errorFinallyCalls, 1);

    int32_t disposeFinallyCalls = 0;
    const auto disposedObserver = std::make_shared<DisposeAfterFirstObserver>();
    Observable::range(1, 3)
        ->doFinally([&disposeFinallyCalls] { ++disposeFinallyCalls; })
        ->subscribe(disposedObserver);
    disposedObserver->expectInt64Values({1});
    disposedObserver->expectNotTerminated();
    disposedObserver->dispose();
    EXPECT_EQ(disposeFinallyCalls, 1);
}

TEST(ObservableDoOnEachTest, ConvertsOnNextExceptionAndStopsFurtherSignals)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::range(1, 3)
        ->doOnEach([](const GAny &) { throw std::runtime_error("side effect failure"); })
        ->subscribe(observer);

    observer->expectInt64Values({});
    observer->expectErrorContains("side effect failure");
}

TEST(ObservableDoOnEachTest, ConvertsErrorAndCompleteCallbackExceptions)
{
    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))
        ->doOnError([](const GAnyException &) { throw std::runtime_error("error callback failure"); })
        ->subscribe(errorObserver);
    errorObserver->expectInt64Values({});
    errorObserver->expectErrorContains("error callback failure");

    const auto completeObserver = std::make_shared<TestObserver>();
    Observable::just(1)
        ->doOnComplete([] { throw std::runtime_error("complete callback failure"); })
        ->subscribe(completeObserver);
    completeObserver->expectInt64Values({1});
    completeObserver->expectErrorContains("complete callback failure");
}
