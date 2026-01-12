//
// Created by Gxin on 2026/1/10.
//

#ifndef RX_SCHEDULED_DIRECT_TIMER_H
#define RX_SCHEDULED_DIRECT_TIMER_H

#include "../scheduler.h"
#include "../leak_observer.h"

#include <gx/gtimer.h>


namespace rx
{
class ScheduledDirectTimer : public Disposable, public GTimer
{
public:
    explicit ScheduledDirectTimer(const GTimerSchedulerPtr &scheduler, const WorkerRunnable &runnable)
        : GTimer(scheduler, true), mRunnable(runnable)
    {
        LeakObserver::make<ScheduledDirectTimer>();
    }

    ~ScheduledDirectTimer() override
    {
        LeakObserver::release<ScheduledDirectTimer>();
    }

public:
    void timeout() override
    {
        if (mRunnable) {
            mRunnable();
        }
    }

    void dispose() override
    {
        if (!mDisposed.exchange(true)) {
            stop();
            mRunnable = nullptr;
        }
    }

    bool isDisposed() const override
    {
        return mDisposed.load(std::memory_order_acquire);
    }

private:
    std::atomic<bool> mDisposed = false;
    WorkerRunnable mRunnable;
};
} // rx

#endif //RX_SCHEDULED_DIRECT_TIMER_H
