#ifndef RX_TESTS_SUPPORT_TEST_SCHEDULER_H
#define RX_TESTS_SUPPORT_TEST_SCHEDULER_H

#include <rx/disposables/atomic_disposable.h>
#include <rx/scheduler.h>

#include <gx/gtimer.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace rx::test
{
class ScopedGlobalTimerScheduler
{
public:
    explicit ScopedGlobalTimerScheduler(const std::string &name)
        : mPrevious(GTimerScheduler::global()),
          mScheduler(GTimerScheduler::create(name))
    {
        GTimerScheduler::makeGlobal(mScheduler);
        mScheduler->start();
    }

    ~ScopedGlobalTimerScheduler()
    {
        mScheduler->stop(false);
        if (mPrevious) {
            GTimerScheduler::makeGlobal(mPrevious);
        }
    }

public:
    const GTimerSchedulerPtr &scheduler() const
    {
        return mScheduler;
    }

private:
    GTimerSchedulerPtr mPrevious;
    GTimerSchedulerPtr mScheduler;
};

struct TestWorkerState
{
    std::atomic<bool> disposed = false;
};

class TestSchedulerState
{
private:
    struct Task
    {
        uint64_t dueTime;
        uint64_t sequence;
        WorkerRunnable runnable;
        std::shared_ptr<AtomicDisposable> disposable;
        std::weak_ptr<TestWorkerState> worker;
    };

public:
    DisposablePtr schedule(const std::shared_ptr<TestWorkerState> &worker,
                           const WorkerRunnable &run, uint64_t delay)
    {
        const auto disposable = std::make_shared<AtomicDisposable>();
        std::lock_guard lock(mMutex);
        if (worker->disposed.load(std::memory_order_acquire)) {
            disposable->dispose();
            return disposable;
        }
        mTasks.push_back({mNow + delay, mSequence++, run, disposable, worker});
        return disposable;
    }

    void disposeWorker(const std::shared_ptr<TestWorkerState> &worker)
    {
        worker->disposed.store(true, std::memory_order_release);
        std::lock_guard lock(mMutex);
        for (const auto &task: mTasks) {
            if (task.worker.lock() == worker) {
                task.disposable->dispose();
            }
        }
        std::erase_if(mTasks, [&worker](const Task &task) {
            return task.worker.lock() == worker;
        });
    }

    void advanceBy(uint64_t duration)
    {
        advanceTo(currentTime() + duration);
    }

    void advanceTo(uint64_t targetTime)
    {
        while (true) {
            Task task;
            {
                std::lock_guard lock(mMutex);
                const auto next = nextTask();
                if (next == mTasks.end() || next->dueTime > targetTime) {
                    mNow = std::max(mNow, targetTime);
                    return;
                }
                task = *next;
                mTasks.erase(next);
                mNow = task.dueTime;
            }
            const auto worker = task.worker.lock();
            if (worker && !worker->disposed.load(std::memory_order_acquire) &&
                !task.disposable->isDisposed()) {
                task.runnable();
            }
        }
    }

    void runUntilIdle()
    {
        while (true) {
            uint64_t nextTime;
            {
                std::lock_guard lock(mMutex);
                const auto next = nextTask();
                if (next == mTasks.end()) {
                    return;
                }
                nextTime = next->dueTime;
            }
            advanceTo(nextTime);
        }
    }

    uint64_t currentTime() const
    {
        std::lock_guard lock(mMutex);
        return mNow;
    }

    size_t pendingTaskCount(const std::shared_ptr<TestWorkerState> &worker) const
    {
        std::lock_guard lock(mMutex);
        return static_cast<size_t>(std::count_if(
            mTasks.begin(), mTasks.end(), [&worker](const Task &task) {
                return task.worker.lock() == worker;
            }));
    }

private:
    std::vector<Task>::iterator nextTask()
    {
        return std::min_element(
            mTasks.begin(), mTasks.end(), [](const Task &left, const Task &right) {
                return std::pair(left.dueTime, left.sequence) <
                       std::pair(right.dueTime, right.sequence);
            });
    }

private:
    mutable std::mutex mMutex;
    std::vector<Task> mTasks;
    uint64_t mNow = 0;
    uint64_t mSequence = 0;
};

class TestWorker : public Worker
{
public:
    TestWorker()
        : TestWorker(std::make_shared<TestSchedulerState>())
    {
    }

    explicit TestWorker(std::shared_ptr<TestSchedulerState> scheduler)
        : mScheduler(std::move(scheduler)),
          mState(std::make_shared<TestWorkerState>())
    {
    }

    ~TestWorker() override = default;

public:
    DisposablePtr schedule(const WorkerRunnable &run, uint64_t delay) override
    {
        return mScheduler->schedule(mState, run, delay);
    }

    void dispose() override
    {
        mScheduler->disposeWorker(mState);
    }

    bool isDisposed() const override
    {
        return mState->disposed.load(std::memory_order_acquire);
    }

    void advanceBy(uint64_t duration)
    {
        mScheduler->advanceBy(duration);
    }

    void advanceTo(uint64_t targetTime)
    {
        mScheduler->advanceTo(targetTime);
    }

    void runUntilIdle()
    {
        mScheduler->runUntilIdle();
    }

    uint64_t currentTime() const
    {
        return mScheduler->currentTime();
    }

    size_t pendingTaskCount() const
    {
        return mScheduler->pendingTaskCount(mState);
    }

private:
    std::shared_ptr<TestSchedulerState> mScheduler;
    std::shared_ptr<TestWorkerState> mState;
};

class TestScheduler : public Scheduler
{
public:
    TestScheduler()
        : mState(std::make_shared<TestSchedulerState>())
    {
    }

    ~TestScheduler() override = default;

public:
    WorkerPtr createWorker() override
    {
        return std::make_shared<TestWorker>(mState);
    }

    void advanceBy(uint64_t duration)
    {
        mState->advanceBy(duration);
    }

    void advanceTo(uint64_t targetTime)
    {
        mState->advanceTo(targetTime);
    }

    void runUntilIdle()
    {
        mState->runUntilIdle();
    }

    uint64_t currentTime() const
    {
        return mState->currentTime();
    }

private:
    std::shared_ptr<TestSchedulerState> mState;
};
} // namespace rx::test

#endif // RX_TESTS_SUPPORT_TEST_SCHEDULER_H
