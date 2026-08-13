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
        mCancelled->store(true, std::memory_order_release);
    }

    bool isDisposed() const override
    {
        return mCancelled->load(std::memory_order_acquire);
    }

    DisposablePtr schedule(const WorkerRunnable &run, uint64_t delay) override
    {
        if (!isDisposed()) {
            std::shared_ptr<AtomicDisposable> d = std::make_shared<AtomicDisposable>();
            const auto cancelled = mCancelled;
            mTimerScheduler->post([run, d, cancelled] {
                if (!d->isDisposed() && !cancelled->load(std::memory_order_acquire)) {
                    run();
                }
            }, delay);
            return d;
        }
        return EmptyDisposable::instance();
    }

private:
    std::shared_ptr<std::atomic<bool> > mCancelled = std::make_shared<std::atomic<bool> >(false);
    GTimerSchedulerPtr mTimerScheduler;
};
} // rx

#endif //RX_TIMER_WORKER_H
