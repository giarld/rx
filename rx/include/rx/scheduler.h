//
// Created by Gxin on 2026/1/7.
//

#ifndef RX_SCHEDULER_H
#define RX_SCHEDULER_H

#include "disposable.h"
#include <gx/gany.h>
#include <gx/gthread.h>
#include <gx/gtime.h>


namespace rx
{
using WorkerRunnable = std::function<void()>;


class Worker : public Disposable
{
public:
    ~Worker() override = default;

public:
    DisposablePtr schedule(const WorkerRunnable &run)
    {
        return schedule(run, 0);
    }

    virtual DisposablePtr schedule(const WorkerRunnable &run, uint64_t delay) = 0;

    uint64_t now() const
    {
        return GTime::currentSteadyTime().nanosecond();
    }
};

using WorkerPtr = std::shared_ptr<Worker>;

class DisposeTask : public Disposable
{
public:
    explicit DisposeTask(const WorkerPtr &worker)
        : mWorker(worker)
    {
    }

    ~DisposeTask() override = default;

public:
    void dispose() override
    {
        mWorker->dispose();
    }

    bool isDisposed() const override
    {
        return mWorker->isDisposed();
    }

    void setDisposable(const DisposablePtr &d)
    {
        mDisposable = d;
    }

private:
    WorkerPtr mWorker;
    DisposablePtr mDisposable;
};

using DisposeTaskPtr = std::shared_ptr<DisposeTask>;

class Scheduler
{
public:
    virtual ~Scheduler() = default;

public:
    virtual void start()
    {
    }

    virtual void shutdown()
    {
    }

    DisposablePtr scheduleDirect(const WorkerRunnable &run)
    {
        return scheduleDirect(run, 0);
    }

    virtual DisposablePtr scheduleDirect(WorkerRunnable run, uint64_t delay)
    {
        WorkerPtr w = createWorker();
        DisposeTaskPtr task = std::make_shared<DisposeTask>(w);

        auto d = w->schedule([run] {
            run();
        }, delay);

        task->setDisposable(d);

        return task;
    }

    virtual WorkerPtr createWorker() = 0;
};

using SchedulerPtr = std::shared_ptr<Scheduler>;
} // rx

#endif //RX_SCHEDULER_H
