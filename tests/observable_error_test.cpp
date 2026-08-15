#include <gtest/gtest.h>

#include "support/test_observer.h"

#include <rx/rx.h>
#include <rx/disposables/atomic_disposable.h>

#include <cstdint>
#include <stdexcept>

namespace
{
using namespace rx;
using namespace rx::test;

template<typename Recover>
void expectActiveFallbackDisposedAfterCancellation(Recover recover)
{
    const auto fallbackDisposable = std::make_shared<AtomicDisposable>();
    const auto fallback = Observable::create([fallbackDisposable](const ObservableEmitterPtr &emitter) {
        emitter->setDisposable(fallbackDisposable);
    });
    const auto observer = std::make_shared<TestObserver>();
    recover(Observable::error(GAnyException("primary failure")), fallback)->subscribe(observer);

    EXPECT_FALSE(fallbackDisposable->isDisposed());
    observer->dispose();

    EXPECT_TRUE(fallbackDisposable->isDisposed());
    observer->expectNotTerminated();
}

TEST(ObservableOnErrorReturnTest, EmitsFallbackAndCompletes)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::error(GAnyException("failure"))->onErrorReturn(GAny(42))->subscribe(observer);

    observer->expectInt64Values({42});
    observer->expectComplete();
}

TEST(ObservableOnErrorReturnTest, PassesSuccessfulSourceThrough)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::just(1, 2)->onErrorReturn(GAny(42))->subscribe(observer);

    observer->expectInt64Values({1, 2});
    observer->expectComplete();
}

TEST(ObservableOnErrorReturnTest, RepeatedUpstreamTerminationProducesOneFallback)
{
    const auto observer = std::make_shared<TestObserver>();
    Observable::create([](const ObservableEmitterPtr &emitter) {
        emitter->onError(GAnyException("first failure"));
        emitter->onError(GAnyException("second failure"));
        emitter->onNext(1);
        emitter->onComplete();
    })->onErrorReturn(GAny(42))->subscribe(observer);

    observer->expectInt64Values({42});
    observer->expectComplete();
}

TEST(ObservableOnErrorReturnTest, DownstreamCancellationDisposesSource)
{
    const auto upstream = std::make_shared<AtomicDisposable>();
    const auto source = Observable::create([upstream](const ObservableEmitterPtr &emitter) {
        emitter->setDisposable(upstream);
    });
    const auto observer = std::make_shared<TestObserver>();
    source->onErrorReturn(GAny(42))->subscribe(observer);
    observer->dispose();

    EXPECT_TRUE(upstream->isDisposed());
    observer->expectNotTerminated();
}

TEST(ObservableOnErrorResumeNextTest, FunctionAndFixedFallbackRecoverOnce)
{
    int32_t resumeCalls = 0;
    const auto functionObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("primary failure"))
        ->onErrorResumeNext([&resumeCalls](const GAnyException &) {
            ++resumeCalls;
            return Observable::just(10, 20);
        })
        ->subscribe(functionObserver);

    functionObserver->expectInt64Values({10, 20});
    functionObserver->expectComplete();
    EXPECT_EQ(resumeCalls, 1);

    const auto fixedObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("primary failure"))
        ->onErrorResumeNext(Observable::just(30))
        ->subscribe(fixedObserver);
    fixedObserver->expectInt64Values({30});
    fixedObserver->expectComplete();
}

TEST(ObservableOnErrorResumeNextTest, FallbackErrorAndResumeFailureTerminateOnce)
{
    const auto fallbackError = std::make_shared<TestObserver>();
    Observable::error(GAnyException("primary failure"))
        ->onErrorResumeNext(Observable::error(GAnyException("fallback failure")))
        ->subscribe(fallbackError);
    fallbackError->expectErrorContains("fallback failure");

    const auto callbackError = std::make_shared<TestObserver>();
    Observable::error(GAnyException("primary failure"))
        ->onErrorResumeNext([](const GAnyException &) -> std::shared_ptr<Observable> {
            throw std::runtime_error("resume failure");
        })
        ->subscribe(callbackError);
    callbackError->expectErrorContains("resume failure");

    const auto nullError = std::make_shared<TestObserver>();
    Observable::error(GAnyException("primary failure"))
        ->onErrorResumeNext([](const GAnyException &) { return std::shared_ptr<Observable>(); })
        ->subscribe(nullError);
    nullError->expectErrorContains("null Observable");
}

TEST(ObservableOnErrorResumeNextTest, DownstreamCancellationDisposesCurrentSource)
{
    const auto upstream = std::make_shared<AtomicDisposable>();
    const auto source = Observable::create([upstream](const ObservableEmitterPtr &emitter) {
        emitter->setDisposable(upstream);
    });
    const auto observer = std::make_shared<TestObserver>();
    source->onErrorResumeNext(Observable::just(1))->subscribe(observer);
    observer->dispose();

    EXPECT_TRUE(upstream->isDisposed());
    observer->expectNotTerminated();
}

TEST(ObservableOnErrorResumeNextTest, FunctionFallbackIsDisposedAfterSwitch)
{
    expectActiveFallbackDisposedAfterCancellation(
        [](const std::shared_ptr<Observable> &source, const std::shared_ptr<Observable> &fallback) {
            return source->onErrorResumeNext(
                [fallback](const GAnyException &) { return fallback; });
        });
}

TEST(ObservableOnErrorResumeNextTest, FixedFallbackIsDisposedAfterSwitch)
{
    expectActiveFallbackDisposedAfterCancellation(
        [](const std::shared_ptr<Observable> &source, const std::shared_ptr<Observable> &fallback) {
            return source->onErrorResumeNext(fallback);
        });
}

TEST(ObservableCatchErrorTest, PublicAliasesDelegateToResumeNext)
{
    const auto functionObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("failure"))
        ->catchError([](const GAnyException &) { return Observable::just(1); })
        ->subscribe(functionObserver);
    functionObserver->expectInt64Values({1});
    functionObserver->expectComplete();

    const auto fixedObserver = std::make_shared<TestObserver>();
    Observable::error(GAnyException("failure"))
        ->catchError(Observable::just(2))
        ->subscribe(fixedObserver);
    fixedObserver->expectInt64Values({2});
    fixedObserver->expectComplete();
}

TEST(ObservableCatchErrorTest, FunctionFallbackIsDisposedAfterSwitch)
{
    expectActiveFallbackDisposedAfterCancellation(
        [](const std::shared_ptr<Observable> &source, const std::shared_ptr<Observable> &fallback) {
            return source->catchError([fallback](const GAnyException &) { return fallback; });
        });
}

TEST(ObservableCatchErrorTest, FixedFallbackIsDisposedAfterSwitch)
{
    expectActiveFallbackDisposedAfterCancellation(
        [](const std::shared_ptr<Observable> &source, const std::shared_ptr<Observable> &fallback) {
            return source->catchError(fallback);
        });
}

TEST(ObservableRetryTest, FiniteRetrySucceedsAndExhaustionForwardsLastError)
{
    int32_t successfulSubscriptions = 0;
    const auto succeeds = Observable::create(
        [&successfulSubscriptions](const ObservableEmitterPtr &emitter) {
            ++successfulSubscriptions;
            if (successfulSubscriptions < 3) {
                emitter->onError(GAnyException("transient failure"));
            } else {
                emitter->onNext(7);
                emitter->onComplete();
            }
        });
    const auto successObserver = std::make_shared<TestObserver>();
    succeeds->retry(2)->subscribe(successObserver);
    successObserver->expectInt64Values({7});
    successObserver->expectComplete();
    EXPECT_EQ(successfulSubscriptions, 3);

    int32_t failedSubscriptions = 0;
    const auto fails = Observable::create([&failedSubscriptions](const ObservableEmitterPtr &emitter) {
        ++failedSubscriptions;
        emitter->onError(GAnyException("permanent failure"));
    });
    const auto failureObserver = std::make_shared<TestObserver>();
    fails->retry(1)->subscribe(failureObserver);
    failureObserver->expectErrorContains("permanent failure");
    EXPECT_EQ(failedSubscriptions, 2);
}

TEST(ObservableRetryTest, UnlimitedOverloadStopsAfterSuccessfulResubscription)
{
    int32_t subscriptions = 0;
    const auto source = Observable::create([&subscriptions](const ObservableEmitterPtr &emitter) {
        ++subscriptions;
        if (subscriptions == 1) {
            emitter->onError(GAnyException("first failure"));
        } else {
            emitter->onNext(9);
            emitter->onComplete();
        }
    });
    const auto observer = std::make_shared<TestObserver>();
    source->retry()->subscribe(observer);

    observer->expectInt64Values({9});
    observer->expectComplete();
    EXPECT_EQ(subscriptions, 2);
}

TEST(ObservableRetryTest, DownstreamCancellationDisposesActiveAttempt)
{
    const auto upstream = std::make_shared<AtomicDisposable>();
    const auto source = Observable::create([upstream](const ObservableEmitterPtr &emitter) {
        emitter->setDisposable(upstream);
    });
    const auto observer = std::make_shared<TestObserver>();
    source->retry(3)->subscribe(observer);
    observer->dispose();

    EXPECT_TRUE(upstream->isDisposed());
    observer->expectNotTerminated();
}
} // namespace
