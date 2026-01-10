//
// Created by Gxin on 2026/1/8.
//

#ifndef RX_TASK_SYSTEM_WORKER_H
#define RX_TASK_SYSTEM_WORKER_H

#include "scheduled_direct_task.h"
#include "../scheduler.h"
#include "../operators/observable_empty.h"

#include <gx/gtasksystem.h>
#include <gx/gtimer.h>


namespace rx
{
class TaskSystemWorker : public Worker
{
public:
    explicit TaskSystemWorker(GTaskSystem *taskSystem)
    {
        mTaskSystem = taskSystem;
    }

    ~TaskSystemWorker() override = default;

public:
    void dispose() override
    {
        mDisposed.store(true, std::memory_order_relaxed);
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

    DisposablePtr scheduleDirect(const WorkerRunnable &run, uint64_t)
    {
        auto future = mTaskSystem->submit([run] {
            run();
            return true;
        });
        auto task = std::make_shared<ScheduledDirectTask>(std::move(future));

        return task;
    }

private:
    std::atomic<bool> mDisposed = false;
    GTaskSystem *mTaskSystem;
};
} // rx

#endif //RX_TASK_SYSTEM_WORKER_H
