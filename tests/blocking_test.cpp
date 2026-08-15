#include <gtest/gtest.h>

#include "support/bounded_wait.h"

#include <rx/disposable.h>
#include <rx/rx.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{
using namespace rx;
using namespace rx::test;
using namespace std::chrono_literals;

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
} // namespace

TEST(ObservableBlockingFirstTest, ReturnsFirstValueAndCancelsTheSource)
{
    const auto upstream = std::make_shared<CountingDisposable>();
    const auto source = Observable::create([upstream](const ObservableEmitterPtr &emitter) {
        emitter->setDisposable(upstream);
        emitter->onNext(1);
        emitter->onNext(2);
        emitter->onComplete();
    });

    EXPECT_EQ(source->blockingFirst().toInt64(), 1);
    EXPECT_EQ(upstream->disposeCount(), 1);
}

TEST(ObservableBlockingFirstTest, HandlesEmptyDefaultErrorAndAsynchronousTermination)
{
    EXPECT_THROW(Observable::empty()->blockingFirst(), GAnyException);
    EXPECT_EQ(Observable::empty()->blockingFirst(7).toInt64(), 7);
    EXPECT_THROW(Observable::error(GAnyException("first failure"))->blockingFirst(), GAnyException);

    BoundedWait entered;
    BoundedWait release;
    int64_t result = 0;
    const auto source = Observable::create([&](const ObservableEmitterPtr &emitter) {
        entered.signal();
        if (release.await(1s)) {
            emitter->onNext(42);
        } else {
            emitter->onError(GAnyException("test release timeout"));
        }
    });
    std::thread caller([&] { result = source->blockingFirst().toInt64(); });

    const bool sourceEntered = entered.await(1s);
    release.signal();
    caller.join();
    ASSERT_TRUE(sourceEntered);
    EXPECT_EQ(result, 42);
}

TEST(ObservableBlockingLastTest, ReturnsLastValueAndHandlesEmptyDefaultAndError)
{
    EXPECT_EQ(Observable::just(1, 2, 3)->blockingLast().toInt64(), 3);
    EXPECT_THROW(Observable::empty()->blockingLast(), GAnyException);
    EXPECT_EQ(Observable::empty()->blockingLast(7).toInt64(), 7);
    EXPECT_THROW(Observable::error(GAnyException("last failure"))->blockingLast(), GAnyException);
}

TEST(ObservableBlockingLastTest, WaitsForAsynchronousCompletion)
{
    BoundedWait entered;
    BoundedWait release;
    int64_t result = 0;
    const auto source = Observable::create([&](const ObservableEmitterPtr &emitter) {
        emitter->onNext(1);
        entered.signal();
        if (release.await(1s)) {
            emitter->onNext(2);
            emitter->onComplete();
        } else {
            emitter->onError(GAnyException("test release timeout"));
        }
    });
    std::thread caller([&] { result = source->blockingLast().toInt64(); });

    const bool sourceEntered = entered.await(1s);
    release.signal();
    caller.join();
    ASSERT_TRUE(sourceEntered);
    EXPECT_EQ(result, 2);
}

TEST(ObservableBlockingForEachTest, VisitsValuesInOrderAndHandlesEmptyAndError)
{
    std::vector<int64_t> values;
    Observable::just(1, 2, 3)->blockingForEach(
        [&values](const GAny &value) { values.push_back(value.toInt64()); });
    EXPECT_EQ(values, (std::vector<int64_t>{1, 2, 3}));

    EXPECT_NO_THROW(Observable::empty()->blockingForEach([](const GAny &) {}));
    EXPECT_THROW(
        Observable::error(GAnyException("forEach failure"))->blockingForEach([](const GAny &) {}),
        GAnyException);
}

TEST(ObservableBlockingForEachTest, CallbackFailureCancelsTheSourceAndTerminates)
{
    const auto upstream = std::make_shared<CountingDisposable>();
    int32_t calls = 0;
    const auto source = Observable::create([upstream](const ObservableEmitterPtr &emitter) {
        emitter->setDisposable(upstream);
        emitter->onNext(1);
        emitter->onNext(2);
        emitter->onComplete();
    });

    EXPECT_THROW(
        source->blockingForEach([&calls](const GAny &) {
            ++calls;
            throw std::runtime_error("callback failure");
        }),
        GAnyException);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(upstream->disposeCount(), 1);
}

TEST(ObservableBlockingForEachTest, WaitsForAsynchronousCompletion)
{
    BoundedWait entered;
    BoundedWait release;
    std::vector<int64_t> values;
    const auto source = Observable::create([&](const ObservableEmitterPtr &emitter) {
        entered.signal();
        if (release.await(1s)) {
            emitter->onNext(1);
            emitter->onComplete();
        } else {
            emitter->onError(GAnyException("test release timeout"));
        }
    });
    std::thread caller([&] {
        source->blockingForEach([&values](const GAny &value) { values.push_back(value.toInt64()); });
    });

    const bool sourceEntered = entered.await(1s);
    release.signal();
    caller.join();
    ASSERT_TRUE(sourceEntered);
    EXPECT_EQ(values, (std::vector<int64_t>{1}));
}
