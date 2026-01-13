//
// Created by Gxin on 2026/1/13.
//

#ifndef RX_SCHEDULED_DIRECT_TASK_H
#define RX_SCHEDULED_DIRECT_TASK_H

#include "../disposables/atomic_disposable.h"
#include "../scheduler.h"


namespace rx
{
class ScheduledDirectTask : public AtomicDisposable
{
public:
    explicit ScheduledDirectTask(const WorkerRunnable &run, uint64_t delay)
        : mRunnable(run), mTime(GTime::currentSteadyTime())
    {
        LeakObserver::make<ScheduledDirectTask>();

        mTime.addMilliSecs(delay);
    }

    ~ScheduledDirectTask() override
    {
        LeakObserver::release<ScheduledDirectTask>();
    }

public:
    const GTime &getTime()
    {
        return mTime;
    }

    void run()
    {
        if (!isDisposed() && mRunnable) {
            mRunnable();
            dispose();
        }
    }

    void dispose() override
    {
        AtomicDisposable::dispose();
        mRunnable = nullptr;
    }

private:
    WorkerRunnable mRunnable;
    GTime mTime;
};
} // rx

#endif //RX_SCHEDULED_DIRECT_TASK_H
