#include <gtest/gtest.h>

#include "support/test_observer.h"

#include <rx/rx.h>
#include <rx/disposables/atomic_disposable.h>
#include <rx/grouped_observable.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace
{
using namespace rx;
using namespace rx::test;

std::vector<std::vector<int64_t> > collectWindows(const std::shared_ptr<Observable> &source)
{
    std::vector<std::shared_ptr<TestObserver> > observers;
    source->subscribe([&observers](const GAny &value) {
        const auto observer = std::make_shared<TestObserver>();
        value.castAs<std::shared_ptr<Observable> >()->subscribe(observer);
        observers.push_back(observer);
    });

    std::vector<std::vector<int64_t> > result;
    for (const auto &observer: observers) {
        std::vector<int64_t> values;
        for (const auto &value: observer->values()) {
            values.push_back(value.toInt64());
        }
        result.push_back(std::move(values));
        observer->expectComplete();
    }
    return result;
}
} // namespace

TEST(ObservableWindowTest, SupportsExactOverlappingAndGappedWindows)
{
    EXPECT_EQ(collectWindows(Observable::range(1, 5)->window(2)),
              std::vector<std::vector<int64_t> >({{1, 2}, {3, 4}, {5}}));
    EXPECT_EQ(collectWindows(Observable::range(1, 4)->window(3, 1)),
              std::vector<std::vector<int64_t> >({{1, 2, 3}, {2, 3, 4}, {3, 4}, {4}}));
    EXPECT_EQ(collectWindows(Observable::range(1, 6)->window(2, 3)),
              std::vector<std::vector<int64_t> >({{1, 2}, {4, 5}}));
}

TEST(ObservableWindowTest, PropagatesErrorToActiveWindowAndOuterObserver)
{
    const auto outer = std::make_shared<TestObserver>();
    std::shared_ptr<TestObserver> inner;
    Observable::create([](const ObservableEmitterPtr &emitter) {
        emitter->onNext(1);
        emitter->onError(GAnyException("window failure"));
        emitter->onComplete();
    })->window(3)->subscribe(std::make_shared<LambdaObserver>(
        [&outer, &inner](const GAny &value) {
            outer->onNext(value);
            inner = std::make_shared<TestObserver>();
            value.castAs<std::shared_ptr<Observable> >()->subscribe(inner);
        },
        [&outer](const GAnyException &error) { outer->onError(error); },
        [&outer] { outer->onComplete(); },
        [&outer](const DisposablePtr &disposable) { outer->onSubscribe(disposable); }));

    ASSERT_NE(inner, nullptr);
    inner->expectInt64Values({1});
    inner->expectErrorContains("window failure");
    outer->expectErrorContains("window failure");
}

TEST(ObservableWindowTest, DownstreamCancellationCompletesActiveWindowAndDisposesUpstream)
{
    const auto upstream = std::make_shared<AtomicDisposable>();
    ObservableEmitterPtr sourceEmitter;
    const auto source = Observable::create([&sourceEmitter, upstream](const ObservableEmitterPtr &emitter) {
        sourceEmitter = emitter;
        emitter->setDisposable(upstream);
    });
    const auto outer = std::make_shared<TestObserver>();
    std::shared_ptr<TestObserver> inner;
    source->window(2)->subscribe(std::make_shared<LambdaObserver>(
        [&outer, &inner](const GAny &value) {
            outer->onNext(value);
            inner = std::make_shared<TestObserver>();
            value.castAs<std::shared_ptr<Observable> >()->subscribe(inner);
        },
        [&outer](const GAnyException &error) { outer->onError(error); },
        [&outer] { outer->onComplete(); },
        [&outer](const DisposablePtr &disposable) { outer->onSubscribe(disposable); }));

    ASSERT_NE(sourceEmitter, nullptr);
    sourceEmitter->onNext(1);
    ASSERT_NE(inner, nullptr);
    outer->dispose();
    sourceEmitter->onNext(2);
    sourceEmitter->onComplete();

    EXPECT_EQ(outer->values().size(), 1u);
    outer->expectNotTerminated();
    inner->expectInt64Values({1});
    inner->expectComplete();
    EXPECT_TRUE(upstream->isDisposed());
}

TEST(ObservableGroupByTest, GroupsSelectedValuesByKey)
{
    struct GroupResult
    {
        int64_t key;
        std::shared_ptr<TestObserver> observer;
    };
    std::vector<GroupResult> groups;
    const auto outer = std::make_shared<TestObserver>();
    Observable::range(1, 4)
        ->groupBy(
            [](const GAny &value) { return value.toInt64() % 2; },
            [](const GAny &value) { return value.toInt64() * 10; })
        ->subscribe(std::make_shared<LambdaObserver>(
            [&groups, &outer](const GAny &value) {
                outer->onNext(value);
                const auto group = value.castAs<std::shared_ptr<GroupedObservable> >();
                const auto observer = std::make_shared<TestObserver>();
                group->subscribe(observer);
                groups.push_back({group->getKey().toInt64(), observer});
            },
            [&outer](const GAnyException &error) { outer->onError(error); },
            [&outer] { outer->onComplete(); },
            [&outer](const DisposablePtr &disposable) { outer->onSubscribe(disposable); }));

    ASSERT_EQ(groups.size(), 2u);
    EXPECT_EQ(groups[0].key, 1);
    groups[0].observer->expectInt64Values({10, 30});
    groups[0].observer->expectComplete();
    EXPECT_EQ(groups[1].key, 0);
    groups[1].observer->expectInt64Values({20, 40});
    groups[1].observer->expectComplete();
    outer->expectComplete();
}

TEST(ObservableGroupByTest, SelectorFailureErrorsGroupsAndOuterOnce)
{
    const auto outer = std::make_shared<TestObserver>();
    std::shared_ptr<TestObserver> groupObserver;
    Observable::just(1, 2)
        ->groupBy(
            [](const GAny &) { return 0; },
            [](const GAny &value) -> GAny {
                if (value.toInt64() == 2) {
                    throw std::runtime_error("value selector failure");
                }
                return value;
            })
        ->subscribe(std::make_shared<LambdaObserver>(
            [&outer, &groupObserver](const GAny &value) {
                outer->onNext(value);
                groupObserver = std::make_shared<TestObserver>();
                value.castAs<std::shared_ptr<GroupedObservable> >()->subscribe(groupObserver);
            },
            [&outer](const GAnyException &error) { outer->onError(error); },
            [&outer] { outer->onComplete(); },
            [&outer](const DisposablePtr &disposable) { outer->onSubscribe(disposable); }));

    ASSERT_NE(groupObserver, nullptr);
    groupObserver->expectInt64Values({1});
    groupObserver->expectErrorContains("value selector failure");
    outer->expectErrorContains("value selector failure");
}

TEST(ObservableGroupByTest, KeySelectorFailureErrorsGroupsAndOuterOnce)
{
    const auto outer = std::make_shared<TestObserver>();
    std::shared_ptr<TestObserver> groupObserver;
    Observable::just(1, 2)
        ->groupBy([](const GAny &value) -> GAny {
            if (value.toInt64() == 2) {
                throw std::runtime_error("key selector failure");
            }
            return 0;
        })
        ->subscribe(std::make_shared<LambdaObserver>(
            [&outer, &groupObserver](const GAny &value) {
                outer->onNext(value);
                groupObserver = std::make_shared<TestObserver>();
                value.castAs<std::shared_ptr<GroupedObservable> >()->subscribe(groupObserver);
            },
            [&outer](const GAnyException &error) { outer->onError(error); },
            [&outer] { outer->onComplete(); },
            [&outer](const DisposablePtr &disposable) { outer->onSubscribe(disposable); }));

    ASSERT_NE(groupObserver, nullptr);
    groupObserver->expectInt64Values({1});
    groupObserver->expectErrorContains("key selector failure");
    outer->expectErrorContains("key selector failure");
}

TEST(ObservableGroupByTest, UpstreamErrorTerminatesGroupsAndOuterOnce)
{
    const auto outer = std::make_shared<TestObserver>();
    std::shared_ptr<TestObserver> groupObserver;
    Observable::create([](const ObservableEmitterPtr &emitter) {
        emitter->onNext(1);
        emitter->onError(GAnyException("group upstream failure"));
        emitter->onNext(2);
        emitter->onComplete();
    })->groupBy([](const GAny &) { return 0; })
        ->subscribe(std::make_shared<LambdaObserver>(
            [&outer, &groupObserver](const GAny &value) {
                outer->onNext(value);
                groupObserver = std::make_shared<TestObserver>();
                value.castAs<std::shared_ptr<GroupedObservable> >()->subscribe(groupObserver);
            },
            [&outer](const GAnyException &error) { outer->onError(error); },
            [&outer] { outer->onComplete(); },
            [&outer](const DisposablePtr &disposable) { outer->onSubscribe(disposable); }));

    ASSERT_NE(groupObserver, nullptr);
    groupObserver->expectInt64Values({1});
    groupObserver->expectErrorContains("group upstream failure");
    outer->expectErrorContains("group upstream failure");
}

TEST(ObservableGroupByTest, DownstreamCancellationCompletesActiveGroupAndDisposesUpstream)
{
    const auto upstream = std::make_shared<AtomicDisposable>();
    ObservableEmitterPtr sourceEmitter;
    const auto source = Observable::create([&sourceEmitter, upstream](const ObservableEmitterPtr &emitter) {
        sourceEmitter = emitter;
        emitter->setDisposable(upstream);
    });
    const auto outer = std::make_shared<TestObserver>();
    std::shared_ptr<TestObserver> inner;
    source->groupBy([](const GAny &value) { return value; })
        ->subscribe(std::make_shared<LambdaObserver>(
            [&outer, &inner](const GAny &value) {
                outer->onNext(value);
                inner = std::make_shared<TestObserver>();
                value.castAs<std::shared_ptr<GroupedObservable> >()->subscribe(inner);
            },
            [&outer](const GAnyException &error) { outer->onError(error); },
            [&outer] { outer->onComplete(); },
            [&outer](const DisposablePtr &disposable) { outer->onSubscribe(disposable); }));

    ASSERT_NE(sourceEmitter, nullptr);
    sourceEmitter->onNext(1);
    ASSERT_NE(inner, nullptr);
    outer->dispose();
    sourceEmitter->onNext(2);
    sourceEmitter->onComplete();

    EXPECT_EQ(outer->values().size(), 1u);
    outer->expectNotTerminated();
    inner->expectInt64Values({1});
    inner->expectComplete();
    EXPECT_TRUE(upstream->isDisposed());
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

TEST(ObservableParameterRegressionTest, RejectsInvalidBufferAndWindowArguments)
{
    const auto source = Observable::just(1);
    EXPECT_THROW(source->buffer(1, 0), GAnyException);
    EXPECT_THROW(source->buffer(0, 1), GAnyException);
    EXPECT_THROW(source->window(1, 0), GAnyException);
    EXPECT_THROW(source->window(0, 1), GAnyException);
    EXPECT_THROW(source->window(-1, 1), GAnyException);
}

TEST(ObservableGroupByRegressionTest, KeepsDifferentKeyTypesSeparate)
{
    int32_t groupCount = 0;
    Observable::just(GAny(1), GAny("1"))
        ->groupBy([](const GAny &value) { return value; })
        ->subscribe([&groupCount](const GAny &) { ++groupCount; });

    EXPECT_EQ(groupCount, 2);
}
