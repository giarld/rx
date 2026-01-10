//
// Created by Gxin on 2026/1/10.
//

#ifndef RX_TIMER_WORKER_H
#define RX_TIMER_WORKER_H

#include "scheduled_direct_timer.h"
#include "../scheduler.h"
#include "../operators/observable_empty.h"

#include <gx/gtimer.h>


namespace rx
{
class TimerWorker : public Worker
{
public:
    explicit TimerWorker(const GTimerSchedulerPtr &timerScheduler)
        : mTimerScheduler(timerScheduler)
    {}

    ~TimerWorker() override = default;

public:
    void dispose() override
    {
        mDisposed = true;
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
        auto task = std::make_shared<ScheduledDirectTimer>(mTimerScheduler, run);
        task->start(delay);
        return task;
    }

private:
    std::atomic<bool> mDisposed = false;
    GTimerSchedulerPtr mTimerScheduler;
};
} // rx

#endif //RX_TIMER_WORKER_H