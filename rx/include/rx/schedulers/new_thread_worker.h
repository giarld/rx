//
// Created by Gxin on 2026/1/8.
//

#ifndef RX_NEW_THREAD_WORKER_H
#define RX_NEW_THREAD_WORKER_H

#include "scheduled_direct_task.h"
#include "../scheduler.h"
#include "../operators/observable_empty.h"

#include <gx/gtasksystem.h>
#include <gx/gtimer.h>



namespace rx
{
class NewThreadWorker : public Worker
{
public:
    explicit NewThreadWorker(ThreadPriority threadPriority)
    {
        mTaskSystem = std::make_unique<GTaskSystem>("NewThreadWorker_Thread", 1);
        mTaskSystem->setThreadPriority(threadPriority);
        mTaskSystem->start();
    }

    ~NewThreadWorker() override
    {
        auto ts = mTaskSystem;
        mTaskSystem = nullptr;
        GTimerScheduler::global()->post([ts]() {
            ts->stop();
        }, 0);
    }

public:
    void dispose() override
    {
        if (!mDisposed) {
            mDisposed = true;
        }
    }

    bool isDisposed() const override
    {
        return mDisposed.load();
    }

    DisposablePtr schedule(const WorkerRunnable &run, uint64_t delay) override
    {
        if (mDisposed) {
            return EmptyDisposable::instance();
        }
        return scheduleDirect(run, delay);
    }

    DisposablePtr scheduleDirect(const WorkerRunnable &run, uint64_t delay)
    {
        auto future = mTaskSystem->submit([run] {
            run();
            return true;
        });
        auto task = std::make_shared<ScheduledDirectTask>(std::move(future));

        return task;
    }

    void shutdown()
    {
        if (mTaskSystem) {
            mTaskSystem->stop();
        }
    }

private:
    std::atomic<bool> mDisposed = false;

    std::shared_ptr<GTaskSystem> mTaskSystem;
};
} // rx

#endif //RX_NEW_THREAD_WORKER_H
