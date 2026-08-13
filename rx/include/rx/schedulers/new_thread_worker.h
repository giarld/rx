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
            const auto taskSystem = mTaskSystem;
            const auto submit = [run, taskSystem, d, cancelled] {
                if (d->isDisposed() || cancelled->load(std::memory_order_acquire)) {
                    return;
                }
                taskSystem->submit([run, d, cancelled] {
                    if (!d->isDisposed() && !cancelled->load(std::memory_order_acquire)) {
                        run();
                    }
                    return true;
                });
            };
            if (delay > 0) {
                GTimerScheduler::global()->post(submit, delay);
            } else {
                submit();
            }
            return d;
        }
        return EmptyDisposable::instance();
    }

private:
    std::shared_ptr<std::atomic<bool> > mCancelled = std::make_shared<std::atomic<bool> >(false);
    std::shared_ptr<GTaskSystem> mTaskSystem;
};
} // rx

#endif //RX_NEW_THREAD_WORKER_H
