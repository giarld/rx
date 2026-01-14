//
// Created by Gxin on 2026/1/8.
//

#ifndef RX_TASK_SYSTEM_WORKER_H
#define RX_TASK_SYSTEM_WORKER_H

#include "../scheduler.h"
#include "../operators/observable_empty.h"
#include "../leak_observer.h"

#include <gx/gtasksystem.h>


namespace rx
{
class TaskSystemWorker : public Worker, public std::enable_shared_from_this<TaskSystemWorker>
{
public:
    explicit TaskSystemWorker(GTaskSystem *taskSystem, GTimerScheduler *timerScheduler)
        : mTaskSystem(taskSystem), mTimerScheduler(timerScheduler)
    {
        LeakObserver::make<TaskSystemWorker>();
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

    DisposablePtr schedule(const WorkerRunnable &run, uint64_t delay) override
    {
        if (!mDisposed.load(std::memory_order_acquire)) {
            std::shared_ptr<AtomicDisposable> d = std::make_shared<AtomicDisposable>();
            if (delay > 0) {
                mTimerScheduler->post([run, ts = mTaskSystem, d] {
                    if (!d->isDisposed()) {
                        ts->submit([run, d] {
                            if (!d->isDisposed()) {
                                run();
                            }
                            return true;
                        });
                    }
                }, delay);
            } else {
                mTaskSystem->submit([run, d] {
                    if (!d->isDisposed()) {
                        run();
                    }
                    return true;
                });
            }
            return d;
        }
        return EmptyDisposable::instance();
    }

private:
    std::atomic<bool> mDisposed = false;
    GTaskSystem *mTaskSystem;
    GTimerScheduler *mTimerScheduler;
};
} // rx

#endif //RX_TASK_SYSTEM_WORKER_H
