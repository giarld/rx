//
// Created by Gxin on 2026/1/13.
//

#ifndef RX_NEW_THREAD_WORKER_H
#define RX_NEW_THREAD_WORKER_H

#include "../scheduler.h"
#include "../operators/observable_empty.h"
#include "../leak_observer.h"

#include <gx/gtasksystem.h>


namespace rx
{
class NewThreadWorker : public Worker
{
public:
    explicit NewThreadWorker(ThreadPriority mThreadPriority)
    {
        LeakObserver::make<NewThreadWorker>();

        mTaskSystem = std::make_shared<GTaskSystem>("NewThreadWorker_Thread", 1);
        mTaskSystem->setThreadPriority(mThreadPriority);
        mTaskSystem->start();
    }

    ~NewThreadWorker() override
    {
        LeakObserver::release<NewThreadWorker>();

        auto ts = mTaskSystem;
        GTimerScheduler::global()
                ->post([ts] {
                    ts->stop();
                }, 0);
        mTaskSystem = nullptr;
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
            GTime beginTime = GTime::currentSteadyTime();
            mTaskSystem->submit([run, beginTime, delay, d] {
                if (d->isDisposed()) {
                    return false;
                }
                const int64_t oDelay = delay - GTime::currentSteadyTime().milliSecsTo(beginTime);
                if (oDelay > 0) {
                    GThread::mSleep(oDelay);
                }
                run();
                return true;
            });
            return d;
        }
        return EmptyDisposable::instance();
    }

private:
    std::atomic<bool> mDisposed = false;
    std::shared_ptr<GTaskSystem> mTaskSystem;
};
} // rx

#endif //RX_NEW_THREAD_WORKER_H
