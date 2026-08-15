#ifndef RX_TESTS_SUPPORT_BOUNDED_WAIT_H
#define RX_TESTS_SUPPORT_BOUNDED_WAIT_H

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace rx::test
{
class BoundedWait
{
public:
    explicit BoundedWait(size_t target = 1)
        : mTarget(target)
    {
    }

public:
    void signal()
    {
        std::lock_guard lock(mMutex);
        ++mCount;
        mCondition.notify_all();
    }

    bool await(std::chrono::milliseconds timeout)
    {
        std::unique_lock lock(mMutex);
        return mCondition.wait_for(lock, timeout, [this] { return mCount >= mTarget; });
    }

    size_t count() const
    {
        std::lock_guard lock(mMutex);
        return mCount;
    }

private:
    mutable std::mutex mMutex;
    std::condition_variable mCondition;
    size_t mTarget;
    size_t mCount = 0;
};
} // namespace rx::test

#endif // RX_TESTS_SUPPORT_BOUNDED_WAIT_H
