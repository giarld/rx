//
// Created by Gxin on 2026/1/8.
//

#ifndef RX_TASK_SYSTEM_WORKER_H
#define RX_TASK_SYSTEM_WORKER_H

#include "../scheduler.h"
#include "../operators/observable_empty.h"
#include "../leak_observer.h"

#include "scheduled_direct_task.h"

#include <gx/gtasksystem.h>


namespace rx
{
class TaskSystemWorker : public Worker, public std::enable_shared_from_this<TaskSystemWorker>
{
public:
    explicit TaskSystemWorker(GTaskSystem *taskSystem)
    {
        LeakObserver::make<TaskSystemWorker>();
        mTaskSystem = taskSystem;
    }

    ~TaskSystemWorker() override
    {
        LeakObserver::release<TaskSystemWorker>();
    }

public:
    void dispose() override
    {
        mDisposed.store(true, std::memory_order_release);
    }

    bool isDisposed() const override
    {
        return mDisposed.load(std::memory_order_acquire);
    }

    DisposablePtr schedule(const WorkerRunnable &run, uint64_t delay) override
    {
        if (!mDisposed.load(std::memory_order_acquire)) {
            const auto t = std::make_shared<ScheduledDirectTask>(run, delay);
            //
            {
                GLockerGuard locker(mLock);
                mTasks.push_back(t);
                std::sort(mTasks.begin(), mTasks.end(),
                    [](const auto &lhs, const auto &rhs) {
                        return lhs->getTime() > rhs->getTime();
                    });
            }

            mTaskSystem->submit([thiz = shared_from_this()] {
                std::shared_ptr<ScheduledDirectTask> task;
                //
                {
                    GLockerGuard locker(thiz->mLock);
                    task = thiz->mTasks.back();
                    thiz->mTasks.pop_back();
                }
                if (!task || task->isDisposed()) {
                    return true;
                }
                const GTime now = GTime::currentSteadyTime();
                const int64_t d = task->getTime().milliSecsTo(now);
                if (d > 0) {
                    GThread::mSleep(d);
                }
                task->run();
                return true;
            });
            return t;
        }
        return EmptyDisposable::instance();
    }

private:
    std::atomic<bool> mDisposed = false;
    GTaskSystem *mTaskSystem;

    std::vector<std::shared_ptr<ScheduledDirectTask>> mTasks;
    GMutex mLock;
};
} // rx

#endif //RX_TASK_SYSTEM_WORKER_H
