#ifndef RX_TESTS_SUPPORT_TEST_OBSERVER_H
#define RX_TESTS_SUPPORT_TEST_OBSERVER_H

#include <gtest/gtest.h>

#include <rx/observer.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace rx::test
{
enum class ObserverEventType
{
    Subscribe,
    Next,
    Error,
    Complete
};

struct ObserverEvent
{
    ObserverEventType type;
    std::string detail;
};

class TestObserver : public Observer
{
public:
    TestObserver() = default;
    ~TestObserver() override = default;

public:
    void onSubscribe(const DisposablePtr &disposable) override
    {
        std::lock_guard lock(mMutex);
        mDisposable = disposable;
        mEvents.push_back({ObserverEventType::Subscribe, {}});
    }

    void onNext(const GAny &value) override
    {
        std::lock_guard lock(mMutex);
        mValues.push_back(value);
        mEvents.push_back({ObserverEventType::Next, value.toString()});
        mCondition.notify_all();
    }

    void onError(const GAnyException &error) override
    {
        std::lock_guard lock(mMutex);
        ++mErrorCount;
        mError = error.toString();
        mEvents.push_back({ObserverEventType::Error, mError});
        mTerminated = true;
        mDisposable.reset();
        mCondition.notify_all();
    }

    void onComplete() override
    {
        std::lock_guard lock(mMutex);
        ++mCompletionCount;
        mEvents.push_back({ObserverEventType::Complete, {}});
        mTerminated = true;
        mDisposable.reset();
        mCondition.notify_all();
    }

    void dispose()
    {
        DisposablePtr disposable;
        {
            std::lock_guard lock(mMutex);
            disposable = std::move(mDisposable);
        }
        if (disposable) {
            disposable->dispose();
        }
    }

    bool awaitTerminal(std::chrono::milliseconds timeout)
    {
        std::unique_lock lock(mMutex);
        return mCondition.wait_for(lock, timeout, [this] { return mTerminated; });
    }

    bool awaitValueCount(size_t count, std::chrono::milliseconds timeout)
    {
        std::unique_lock lock(mMutex);
        return mCondition.wait_for(lock, timeout, [this, count] { return mValues.size() >= count; });
    }

    std::vector<GAny> values() const
    {
        std::lock_guard lock(mMutex);
        return mValues;
    }

    std::vector<ObserverEvent> events() const
    {
        std::lock_guard lock(mMutex);
        return mEvents;
    }

    std::string error() const
    {
        std::lock_guard lock(mMutex);
        return mError;
    }

    bool hasError() const
    {
        std::lock_guard lock(mMutex);
        return mErrorCount != 0;
    }

    int32_t errorCount() const
    {
        std::lock_guard lock(mMutex);
        return mErrorCount;
    }

    int32_t completionCount() const
    {
        std::lock_guard lock(mMutex);
        return mCompletionCount;
    }

    size_t signalsAfterTermination() const
    {
        std::lock_guard lock(mMutex);
        bool terminated = false;
        size_t count = 0;
        for (const auto &event: mEvents) {
            if (terminated) {
                ++count;
            }
            if (event.type == ObserverEventType::Error || event.type == ObserverEventType::Complete) {
                terminated = true;
            }
        }
        return count;
    }

    std::string describe() const
    {
        std::lock_guard lock(mMutex);
        std::ostringstream output;
        output << "values=" << mValues.size()
               << ", error=" << (mErrorCount != 0 ? '"' + mError + '"' : "<none>")
               << ", error signals=" << mErrorCount
               << ", completions=" << mCompletionCount
               << ", events=" << mEvents.size()
               << ", signals after termination=" << signalsAfterTerminationLocked();
        return output.str();
    }

    void expectInt64Values(const std::vector<int64_t> &expected) const
    {
        std::vector<int64_t> actual;
        for (const auto &value: values()) {
            actual.push_back(value.toInt64());
        }
        EXPECT_EQ(actual, expected) << describe();
    }

    void expectComplete() const
    {
        EXPECT_TRUE(errorCount() == 0
                    && completionCount() == 1
                    && signalsAfterTermination() == 0)
            << describe();
    }

    void expectNotTerminated() const
    {
        EXPECT_TRUE(errorCount() == 0
                    && completionCount() == 0
                    && signalsAfterTermination() == 0)
            << describe();
    }

    void expectErrorContains(const std::string &expected) const
    {
        EXPECT_TRUE(errorCount() == 1
                    && error().find(expected) != std::string::npos
                    && completionCount() == 0
                    && signalsAfterTermination() == 0)
            << describe();
    }

private:
    size_t signalsAfterTerminationLocked() const
    {
        bool terminated = false;
        size_t count = 0;
        for (const auto &event: mEvents) {
            if (terminated) {
                ++count;
            }
            if (event.type == ObserverEventType::Error || event.type == ObserverEventType::Complete) {
                terminated = true;
            }
        }
        return count;
    }

private:
    mutable std::mutex mMutex;
    std::condition_variable mCondition;
    DisposablePtr mDisposable;
    std::vector<GAny> mValues;
    std::vector<ObserverEvent> mEvents;
    std::string mError;
    int32_t mErrorCount = 0;
    int32_t mCompletionCount = 0;
    bool mTerminated = false;
};
} // namespace rx::test

#endif // RX_TESTS_SUPPORT_TEST_OBSERVER_H
