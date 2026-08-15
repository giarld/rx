#include <gtest/gtest.h>

#include "support/test_observer.h"

#include <rx/rx.h>

#include <cstdint>
#include <limits>
#include <memory>
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
} // namespace

TEST(ObservableRangeRegressionTest, SupportsNegativeStart)
{
    const auto observer = std::make_shared<rx::test::TestObserver>();
    Observable::range(-3, 3)->subscribe(observer);

    observer->expectInt64Values({-3, -2, -1});
    observer->expectComplete();
}

TEST(ObservableRangeTest, EmitsRequestedSequence)
{
    const auto observer = std::make_shared<rx::test::TestObserver>();
    Observable::range(5, 3)->subscribe(observer);

    observer->expectInt64Values({5, 6, 7});
    observer->expectComplete();
}

TEST(ObservableRangeRegressionTest, ZeroCountCompletesWithoutValues)
{
    const auto observer = std::make_shared<rx::test::TestObserver>();
    Observable::range(42, 0)->subscribe(observer);

    observer->expectInt64Values({});
    observer->expectComplete();
}

TEST(ObservableRangeRegressionTest, SingleCountEmitsStart)
{
    const auto observer = std::make_shared<rx::test::TestObserver>();
    Observable::range(std::numeric_limits<int64_t>::min(), 1)->subscribe(observer);

    observer->expectInt64Values({std::numeric_limits<int64_t>::min()});
    observer->expectComplete();
}

TEST(ObservableRangeRegressionTest, SupportsInt64UpperBoundary)
{
    const auto observer = std::make_shared<rx::test::TestObserver>();
    Observable::range(std::numeric_limits<int64_t>::max() - 1, 2)->subscribe(observer);

    observer->expectInt64Values({std::numeric_limits<int64_t>::max() - 1,
                                 std::numeric_limits<int64_t>::max()});
    observer->expectComplete();
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
