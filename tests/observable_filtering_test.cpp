#include <gtest/gtest.h>

#include "support/test_observer.h"

#include <rx/rx.h>
#include <rx/disposables/atomic_disposable.h>

#include <stdexcept>

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
} // namespace

TEST(ObservableFilterTest, SelectsValuesAndConvertsPredicateException)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::range(0, 6)
        ->filter([](const GAny &value) { return value.toInt64() % 2 == 0; })
        ->subscribe(observer);
    observer->expectInt64Values({0, 2, 4});
    observer->expectComplete();

    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::range(1, 3)
        ->filter([](const GAny &) -> bool { throw std::runtime_error("filter failure"); })
        ->subscribe(errorObserver);
    errorObserver->expectErrorContains("filter failure");
}

TEST(ObservableFilterTest, StopsWhenDownstreamDisposes)
{
    const auto observer = std::make_shared<DisposeAfterFirstObserver>();
    Observable::range(1, 5)
        ->filter([](const GAny &) { return true; })
        ->subscribe(observer);

    observer->expectInt64Values({1});
    observer->expectNotTerminated();
}

TEST(ObservableDistinctTest, SupportsDefaultAndSelectedKeys)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::just(1, 2, 1, 3, 2)->distinct()->subscribe(observer);
    observer->expectInt64Values({1, 2, 3});
    observer->expectComplete();

    const auto selected = std::make_shared<TestObserver>();
    Observable::just(1, 3, 2, 4)
        ->distinct([](const GAny &value) { return value.toInt64() % 2; })
        ->subscribe(selected);
    selected->expectInt64Values({1, 2});
    selected->expectComplete();
}

TEST(ObservableDistinctTest, ConvertsKeySelectorException)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::just(1, 2)
        ->distinct([](const GAny &) -> GAny { throw std::runtime_error("key failure"); })
        ->subscribe(observer);
    observer->expectErrorContains("key failure");
}

TEST(ObservableDistinctTest, ForwardsUpstreamErrorAndStopsWhenDisposed)
{
    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))->distinct()->subscribe(errorObserver);
    errorObserver->expectErrorContains("upstream failure");

    const auto disposedObserver = std::make_shared<DisposeAfterFirstObserver>();
    Observable::just(1, 2, 3)->distinct()->subscribe(disposedObserver);
    disposedObserver->expectInt64Values({1});
    disposedObserver->expectNotTerminated();
}

TEST(ObservableDistinctUntilChangedTest, CoversAllOverloadsAndComparatorFailure)
{
    const auto basic = std::make_shared<TestObserver>();
    Observable::just(1, 1, 2, 2, 1)->distinctUntilChanged()->subscribe(basic);
    basic->expectInt64Values({1, 2, 1});
    basic->expectComplete();

    const auto selected = std::make_shared<TestObserver>();
    Observable::just(1, 3, 2, 4)
        ->distinctUntilChanged([](const GAny &value) { return value.toInt64() % 2; })
        ->subscribe(selected);
    selected->expectInt64Values({1, 2});
    selected->expectComplete();

    const auto compared = std::make_shared<TestObserver>();
    Observable::just(1, 2, 4, 5)
        ->distinctUntilChanged([](const GAny &left, const GAny &right) {
            return left.toInt64() % 2 == right.toInt64() % 2;
        })
        ->subscribe(compared);
    compared->expectInt64Values({1, 2, 5});
    compared->expectComplete();

    const auto combined = std::make_shared<TestObserver>();
    Observable::just(1, 3, 2, 4)
        ->distinctUntilChanged(
            [](const GAny &value) { return value.toInt64() % 2; },
            [](const GAny &left, const GAny &right) { return left == right; })
        ->subscribe(combined);
    combined->expectInt64Values({1, 2});
    combined->expectComplete();

    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::just(1, 2)
        ->distinctUntilChanged([](const GAny &, const GAny &) -> bool {
            throw std::runtime_error("comparator failure");
        })
        ->subscribe(errorObserver);
    errorObserver->expectErrorContains("comparator failure");
}

TEST(ObservableElementAtTest, CoversHitOutOfBoundsAndDefault)
{
    const auto hit = std::make_shared<TestObserver>();
    Observable::just(10, 20, 30)->elementAt(1)->subscribe(hit);
    hit->expectInt64Values({20});
    hit->expectComplete();

    const auto missing = std::make_shared<TestObserver>();
    Observable::just(10)->elementAt(2)->subscribe(missing);
    missing->expectErrorContains("Index out of bounds");

    const auto fallback = std::make_shared<TestObserver>();
    Observable::empty()->elementAt(0, 99)->subscribe(fallback);
    fallback->expectInt64Values({99});
    fallback->expectComplete();
}

TEST(ObservableElementAtTest, ForwardsErrorsAndCancelsUpstreamAfterMatch)
{
    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))->elementAt(0)->subscribe(errorObserver);
    errorObserver->expectErrorContains("upstream failure");

    const auto defaultErrorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("default upstream failure"))
        ->elementAt(0, 99)
        ->subscribe(defaultErrorObserver);
    defaultErrorObserver->expectErrorContains("default upstream failure");

    const auto upstream = std::make_shared<AtomicDisposable>();
    const auto hit = std::make_shared<TestObserver>();
    Observable::create([upstream](const ObservableEmitterPtr &emitter) {
        emitter->setDisposable(upstream);
        emitter->onNext(10);
        emitter->onNext(20);
    })->elementAt(0)->subscribe(hit);

    hit->expectInt64Values({10});
    hit->expectComplete();
    EXPECT_TRUE(upstream->isDisposed());
}

TEST(ObservableFirstTest, CoversValueEmptyAndDefault)
{
    const auto hit = std::make_shared<TestObserver>();
    Observable::just(10, 20)->first()->subscribe(hit);
    hit->expectInt64Values({10});
    hit->expectComplete();

    const auto missing = std::make_shared<TestObserver>();
    Observable::empty()->first()->subscribe(missing);
    missing->expectErrorContains("Index out of bounds");

    const auto fallback = std::make_shared<TestObserver>();
    Observable::empty()->first(99)->subscribe(fallback);
    fallback->expectInt64Values({99});
    fallback->expectComplete();
}

TEST(ObservableFirstTest, ForwardsErrorsFromBothOverloads)
{
    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))->first()->subscribe(errorObserver);
    errorObserver->expectErrorContains("upstream failure");

    const auto defaultErrorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("default upstream failure"))
        ->first(99)
        ->subscribe(defaultErrorObserver);
    defaultErrorObserver->expectErrorContains("default upstream failure");
}

TEST(ObservableLastTest, CoversValueEmptyAndDefault)
{
    const auto hit = std::make_shared<TestObserver>();
    Observable::just(10, 20)->last()->subscribe(hit);
    hit->expectInt64Values({20});
    hit->expectComplete();

    const auto missing = std::make_shared<TestObserver>();
    Observable::empty()->last()->subscribe(missing);
    missing->expectErrorContains("No elements in sequence");

    const auto fallback = std::make_shared<TestObserver>();
    Observable::empty()->last(99)->subscribe(fallback);
    fallback->expectInt64Values({99});
    fallback->expectComplete();

    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))->last()->subscribe(errorObserver);
    errorObserver->expectErrorContains("upstream failure");

    const auto defaultErrorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("default upstream failure"))
        ->last(99)
        ->subscribe(defaultErrorObserver);
    defaultErrorObserver->expectErrorContains("default upstream failure");
}

TEST(ObservableIgnoreElementsTest, IgnoresValuesAndForwardsTerminalSignals)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::range(1, 3)->ignoreElements()->subscribe(observer);
    observer->expectInt64Values({});
    observer->expectComplete();

    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))->ignoreElements()->subscribe(errorObserver);
    errorObserver->expectErrorContains("upstream failure");
}

TEST(ObservableSkipTest, CoversZeroWithinAndBeyondLength)
{
    const auto zero = std::make_shared<TestObserver>();
    Observable::range(1, 3)->skip(0)->subscribe(zero);
    zero->expectInt64Values({1, 2, 3});
    zero->expectComplete();

    const auto within = std::make_shared<TestObserver>();
    Observable::range(1, 4)->skip(2)->subscribe(within);
    within->expectInt64Values({3, 4});
    within->expectComplete();

    const auto beyond = std::make_shared<TestObserver>();
    Observable::range(1, 2)->skip(3)->subscribe(beyond);
    beyond->expectInt64Values({});
    beyond->expectComplete();

    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))->skip(1)->subscribe(errorObserver);
    errorObserver->expectErrorContains("upstream failure");
}

TEST(ObservableSkipLastTest, CoversZeroWithinAndBeyondLength)
{
    const auto zero = std::make_shared<TestObserver>();
    Observable::range(1, 3)->skipLast(0)->subscribe(zero);
    zero->expectInt64Values({1, 2, 3});
    zero->expectComplete();

    const auto within = std::make_shared<TestObserver>();
    Observable::range(1, 4)->skipLast(2)->subscribe(within);
    within->expectInt64Values({1, 2});
    within->expectComplete();

    const auto beyond = std::make_shared<TestObserver>();
    Observable::range(1, 2)->skipLast(3)->subscribe(beyond);
    beyond->expectInt64Values({});
    beyond->expectComplete();

    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))->skipLast(1)->subscribe(errorObserver);
    errorObserver->expectErrorContains("upstream failure");
}

TEST(ObservableTakeTest, CoversZeroBoundaryAndUpstreamError)
{
    const auto zero = std::make_shared<TestObserver>();
    Observable::range(1, 3)->take(0)->subscribe(zero);
    zero->expectInt64Values({});
    zero->expectComplete();

    const auto limited = std::make_shared<TestObserver>();
    Observable::range(1, 5)->take(2)->subscribe(limited);
    limited->expectInt64Values({1, 2});
    limited->expectComplete();

    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))->take(2)->subscribe(errorObserver);
    errorObserver->expectErrorContains("upstream failure");
}

TEST(ObservableTakeLastTest, CoversZeroWithinAndBeyondLength)
{
    const auto zero = std::make_shared<TestObserver>();
    Observable::range(1, 3)->takeLast(0)->subscribe(zero);
    zero->expectInt64Values({});
    zero->expectComplete();

    const auto within = std::make_shared<TestObserver>();
    Observable::range(1, 4)->takeLast(2)->subscribe(within);
    within->expectInt64Values({3, 4});
    within->expectComplete();

    const auto beyond = std::make_shared<TestObserver>();
    Observable::range(1, 2)->takeLast(3)->subscribe(beyond);
    beyond->expectInt64Values({1, 2});
    beyond->expectComplete();

    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))->takeLast(1)->subscribe(errorObserver);
    errorObserver->expectErrorContains("upstream failure");
}

TEST(ObservableTakeWhileTest, StopsAtBoundaryAndConvertsPredicateException)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::range(1, 5)
        ->takeWhile([](const GAny &value) { return value.toInt64() < 4; })
        ->subscribe(observer);
    observer->expectInt64Values({1, 2, 3});
    observer->expectComplete();

    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::range(1, 3)
        ->takeWhile([](const GAny &) -> bool { throw std::runtime_error("predicate failure"); })
        ->subscribe(errorObserver);
    errorObserver->expectErrorContains("predicate failure");
}

TEST(ObservableSkipWhileTest, StopsTestingAfterFirstFalseAndConvertsException)
{
    int32_t calls = 0;
    const auto observer = std::make_shared<TestObserver>();
    Observable::range(1, 5)
        ->skipWhile([&calls](const GAny &value) {
            ++calls;
            return value.toInt64() < 3;
        })
        ->subscribe(observer);
    observer->expectInt64Values({3, 4, 5});
    observer->expectComplete();
    EXPECT_EQ(calls, 3);

    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::range(1, 3)
        ->skipWhile([](const GAny &) -> bool { throw std::runtime_error("predicate failure"); })
        ->subscribe(errorObserver);
    errorObserver->expectErrorContains("predicate failure");
}
