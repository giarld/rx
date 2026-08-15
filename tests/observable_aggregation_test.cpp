#include <gtest/gtest.h>

#include "support/test_observer.h"

#include <rx/rx.h>

#include <cstdint>
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

TEST(ObservableScanTest, EmitsRunningAccumulation)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::just(1, 2, 3)
        ->scan([](const GAny &sum, const GAny &next) {
            return sum.toInt64() + next.toInt64();
        })
        ->subscribe(observer);

    observer->expectInt64Values({1, 3, 6});
    observer->expectComplete();
}

TEST(ObservableScanTest, NullFirstValueStillUsesAccumulator)
{
    int32_t accumulatorCalls = 0;
    const auto observer = std::make_shared<TestObserver>();
    Observable::just(GAny(nullptr), GAny(1))
        ->scan([&accumulatorCalls](const GAny &, const GAny &next) {
            ++accumulatorCalls;
            return next;
        })
        ->subscribe(observer);

    const auto values = observer->values();
    ASSERT_EQ(values.size(), 2u);
    EXPECT_TRUE(values[0] == nullptr);
    EXPECT_EQ(values[1].toInt64(), 1);
    EXPECT_EQ(accumulatorCalls, 1);
    observer->expectComplete();
}

TEST(ObservableScanTest, ConvertsAccumulatorExceptionAndStops)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::just(1, 2, 3)
        ->scan([](const GAny &, const GAny &) -> GAny {
            throw std::runtime_error("accumulator failure");
        })
        ->subscribe(observer);

    observer->expectInt64Values({1});
    observer->expectErrorContains("accumulator failure");
}

TEST(ObservableReduceTest, EmitsFinalValueAndErrorsForEmptySource)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::just(1, 2, 3)
        ->reduce([](const GAny &sum, const GAny &next) {
            return sum.toInt64() + next.toInt64();
        })
        ->subscribe(observer);
    observer->expectInt64Values({6});
    observer->expectComplete();

    const auto emptyObserver = std::make_shared<TestObserver>();
    Observable::empty()
        ->reduce([](const GAny &sum, const GAny &next) { return sum.toInt64() + next.toInt64(); })
        ->subscribe(emptyObserver);
    emptyObserver->expectErrorContains("No elements in sequence");
}

TEST(ObservableReduceTest, ConvertsAccumulatorExceptionAndForwardsUpstreamError)
{
    const auto callbackObserver = std::make_shared<TestObserver>();
    Observable::just(1, 2, 3)
        ->reduce([](const GAny &, const GAny &) -> GAny {
            throw std::runtime_error("accumulator failure");
        })
        ->subscribe(callbackObserver);
    callbackObserver->expectErrorContains("accumulator failure");

    const auto upstreamObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))
        ->reduce([](const GAny &sum, const GAny &next) { return sum.toInt64() + next.toInt64(); })
        ->subscribe(upstreamObserver);
    upstreamObserver->expectErrorContains("upstream failure");
}

TEST(ObservableAllTest, CoversTrueFalseEmptyAndPredicateException)
{
    const auto allTrue = std::make_shared<TestObserver>();
    Observable::just(2, 4)->all([](const GAny &value) { return value.toInt64() % 2 == 0; })
        ->subscribe(allTrue);
    allTrue->expectInt64Values({1});
    allTrue->expectComplete();

    const auto falseResult = std::make_shared<TestObserver>();
    Observable::just(2, 3, 4)->all([](const GAny &value) { return value.toInt64() % 2 == 0; })
        ->subscribe(falseResult);
    falseResult->expectInt64Values({0});
    falseResult->expectComplete();

    const auto empty = std::make_shared<TestObserver>();
    Observable::empty()->all([](const GAny &) { return false; })->subscribe(empty);
    empty->expectInt64Values({1});
    empty->expectComplete();

    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::just(1)->all([](const GAny &) -> bool {
        throw std::runtime_error("predicate failure");
    })->subscribe(errorObserver);
    errorObserver->expectErrorContains("predicate failure");
}

TEST(ObservableAllTest, ForwardsUpstreamErrorAndHonorsDisposal)
{
    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))
        ->all([](const GAny &) { return true; })
        ->subscribe(errorObserver);
    errorObserver->expectErrorContains("upstream failure");

    const auto disposedObserver = std::make_shared<DisposeOnSubscribeObserver>();
    Observable::just(1, 2, 3)
        ->all([](const GAny &) { return false; })
        ->subscribe(disposedObserver);
    disposedObserver->expectInt64Values({});
    disposedObserver->expectNotTerminated();
}

TEST(ObservableAnyTest, CoversTrueFalseEmptyAndPredicateException)
{
    const auto anyTrue = std::make_shared<TestObserver>();
    Observable::just(1, 2)->any([](const GAny &value) { return value.toInt64() % 2 == 0; })
        ->subscribe(anyTrue);
    anyTrue->expectInt64Values({1});
    anyTrue->expectComplete();

    const auto falseResult = std::make_shared<TestObserver>();
    Observable::just(1, 3)->any([](const GAny &value) { return value.toInt64() % 2 == 0; })
        ->subscribe(falseResult);
    falseResult->expectInt64Values({0});
    falseResult->expectComplete();

    const auto empty = std::make_shared<TestObserver>();
    Observable::empty()->any([](const GAny &) { return true; })->subscribe(empty);
    empty->expectInt64Values({0});
    empty->expectComplete();

    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::just(1)->any([](const GAny &) -> bool {
        throw std::runtime_error("predicate failure");
    })->subscribe(errorObserver);
    errorObserver->expectErrorContains("predicate failure");
}

TEST(ObservableAnyTest, ForwardsUpstreamErrorAndHonorsDisposal)
{
    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))
        ->any([](const GAny &) { return false; })
        ->subscribe(errorObserver);
    errorObserver->expectErrorContains("upstream failure");

    const auto disposedObserver = std::make_shared<DisposeOnSubscribeObserver>();
    Observable::just(1, 2, 3)
        ->any([](const GAny &) { return true; })
        ->subscribe(disposedObserver);
    disposedObserver->expectInt64Values({});
    disposedObserver->expectNotTerminated();
}

TEST(ObservableContainsTest, ReportsMembership)
{
    const auto present = std::make_shared<TestObserver>();
    Observable::just(1, 2, 3)->contains(2)->subscribe(present);
    present->expectInt64Values({1});
    present->expectComplete();

    const auto absent = std::make_shared<TestObserver>();
    Observable::just(1, 2, 3)->contains(4)->subscribe(absent);
    absent->expectInt64Values({0});
    absent->expectComplete();
}

TEST(ObservableIsEmptyTest, DistinguishesEmptyAndNonEmptySources)
{
    const auto empty = std::make_shared<TestObserver>();
    Observable::empty()->isEmpty()->subscribe(empty);
    empty->expectInt64Values({1});
    empty->expectComplete();

    const auto nonEmpty = std::make_shared<TestObserver>();
    Observable::just(1)->isEmpty()->subscribe(nonEmpty);
    nonEmpty->expectInt64Values({0});
    nonEmpty->expectComplete();
}

TEST(ObservableDefaultIfEmptyTest, UsesFallbackOnlyForEmptySource)
{
    const auto empty = std::make_shared<TestObserver>();
    Observable::empty()->defaultIfEmpty(99)->subscribe(empty);
    empty->expectInt64Values({99});
    empty->expectComplete();

    const auto nonEmpty = std::make_shared<TestObserver>();
    Observable::just(1)->defaultIfEmpty(99)->subscribe(nonEmpty);
    nonEmpty->expectInt64Values({1});
    nonEmpty->expectComplete();

    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))->defaultIfEmpty(99)->subscribe(errorObserver);
    errorObserver->expectErrorContains("upstream failure");
}

TEST(ObservableRepeatTest, CoversRepeatCountZeroAndUpstreamError)
{
    const auto repeated = std::make_shared<TestObserver>();
    Observable::just(1, 2)->repeat(2)->subscribe(repeated);
    repeated->expectInt64Values({1, 2, 1, 2});
    repeated->expectComplete();

    const auto zero = std::make_shared<TestObserver>();
    Observable::just(1)->repeat(0)->subscribe(zero);
    zero->expectInt64Values({});
    zero->expectComplete();

    const auto errorObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("upstream failure"))->repeat(3)->subscribe(errorObserver);
    errorObserver->expectErrorContains("upstream failure");
}

TEST(ObservableRepeatTest, StopsWhenDownstreamDisposes)
{
    const auto observer = std::make_shared<DisposeAfterFirstObserver>();
    Observable::just(1, 2)->repeat(3)->subscribe(observer);

    observer->expectInt64Values({1});
    observer->expectNotTerminated();
}
