#include <gtest/gtest.h>

#include <rx/rx.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{
using namespace rx;

struct SubscriptionResult
{
    std::vector<GAny> values;
    std::string error;
    int32_t completionCount = 0;
};

SubscriptionResult subscribeTo(const std::shared_ptr<Observable> &source)
{
    SubscriptionResult result;
    source->subscribe(
        [&result](const GAny &value) { result.values.push_back(value); },
        [&result](const GAnyException &error) { result.error = error.toString(); },
        [&result] { ++result.completionCount; });
    return result;
}

std::vector<int64_t> toInt64Values(const std::vector<GAny> &values)
{
    std::vector<int64_t> result;
    result.reserve(values.size());
    for (const auto &value: values) {
        result.push_back(value.toInt64());
    }
    return result;
}
} // namespace

TEST(ObservableCreationTest, JustEmitsValuesInOrderAndCompletes)
{
    const auto result = subscribeTo(Observable::just(1, 2, 3));

    EXPECT_EQ(toInt64Values(result.values), std::vector<int64_t>({1, 2, 3}));
    EXPECT_TRUE(result.error.empty());
    EXPECT_EQ(result.completionCount, 1);
}

TEST(ObservableCreationTest, EmptyCompletesWithoutValues)
{
    const auto result = subscribeTo(Observable::empty());

    EXPECT_TRUE(result.values.empty());
    EXPECT_TRUE(result.error.empty());
    EXPECT_EQ(result.completionCount, 1);
}

TEST(ObservableCreationTest, ErrorForwardsFailureWithoutCompletion)
{
    const auto result = subscribeTo(Observable::error(GAnyException("expected failure")));

    EXPECT_TRUE(result.values.empty());
    EXPECT_NE(result.error.find("expected failure"), std::string::npos);
    EXPECT_EQ(result.completionCount, 0);
}

TEST(ObservableTransformationTest, MapTransformsEveryValue)
{
    const auto result = subscribeTo(Observable::range(1, 4)->map([](const GAny &value) {
        return value.toInt64() * 10;
    }));

    EXPECT_EQ(toInt64Values(result.values), std::vector<int64_t>({10, 20, 30, 40}));
    EXPECT_TRUE(result.error.empty());
    EXPECT_EQ(result.completionCount, 1);
}

TEST(ObservableFilteringTest, FilterAndTakeComposeCorrectly)
{
    const auto result = subscribeTo(
        Observable::range(0, 10)
            ->filter([](const GAny &value) { return value.toInt64() % 2 == 0; })
            ->take(3));

    EXPECT_EQ(toInt64Values(result.values), std::vector<int64_t>({0, 2, 4}));
    EXPECT_TRUE(result.error.empty());
    EXPECT_EQ(result.completionCount, 1);
}

TEST(ObservableErrorHandlingTest, OnErrorReturnEmitsFallbackAndCompletes)
{
    const auto result = subscribeTo(
        Observable::error(GAnyException("failure"))->onErrorReturn(GAny(42)));

    EXPECT_EQ(toInt64Values(result.values), std::vector<int64_t>({42}));
    EXPECT_TRUE(result.error.empty());
    EXPECT_EQ(result.completionCount, 1);
}
