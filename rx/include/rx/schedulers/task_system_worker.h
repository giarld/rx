//
// Created by Gxin on 2026/1/8.
//

#ifndef RX_TASK_SYSTEM_WORKER_H
#define RX_TASK_SYSTEM_WORKER_H

#include "scheduled_direct_task.h"
#include "../scheduler.h"
#include "../operators/observable_empty.h"
#include "../leak_observer.h"

#include <gx/gtasksystem.h>


namespace rx
{
class TaskSystemWorker : public Worker
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

    DisposablePtr schedule(const WorkerRunnable &run, uint64_t /*delay*/) override
    {
        if (!mDisposed.load(std::memory_order_acquire)) {
            mTaskSystem->submit([run] {
                run();
                return true;
            });
        }
        return EmptyDisposable::instance();
    }

private:
    std::atomic<bool> mDisposed = false;
    GTaskSystem *mTaskSystem;
};
} // rx

#endif //RX_TASK_SYSTEM_WORKER_H
