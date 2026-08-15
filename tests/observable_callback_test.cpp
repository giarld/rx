#include <gtest/gtest.h>

#include "support/test_observer.h"

#include <rx/rx.h>

#include <cstdint>
#include <stdexcept>

namespace
{
using namespace rx;
using namespace rx::test;
} // namespace

TEST(ObservableCallbackRegressionTest, DoOnSubscribeFailureReachesDownstream)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::just(1)
        ->doOnSubscribe([](const DisposablePtr &) { throw std::runtime_error("subscribe failure"); })
        ->subscribe(observer);

    observer->expectInt64Values({});
    observer->expectErrorContains("subscribe failure");
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
