//
// Created by Gxin on 2026/1/10.
//

#ifndef RX_TIMER_WORKER_H
#define RX_TIMER_WORKER_H

#include "../scheduler.h"
#include "../operators/observable_empty.h"
#include "../leak_observer.h"

#include <gx/gtimer.h>


namespace rx
{
class TimerWorker : public Worker
{
public:
    explicit TimerWorker(const GTimerSchedulerPtr &timerScheduler)
        : mTimerScheduler(timerScheduler)
    {
        LeakObserver::make<TimerWorker>();
    }

    ~TimerWorker() override
    {
        LeakObserver::release<TimerWorker>();
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
            std::shared_ptr<AtomicDisposable> d = std::make_shared<AtomicDisposable>();
            mTimerScheduler->post([run, d] {
                if (!d->isDisposed()) {
                    run();
                }
            }, delay);
            return d;
        }
        return EmptyDisposable::instance();
    }

private:
    std::atomic<bool> mDisposed = false;
    GTimerSchedulerPtr mTimerScheduler;
};
} // rx

#endif //RX_TIMER_WORKER_H
