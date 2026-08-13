//
// Created by Gxin on 2026/1/8.
//

#ifndef RX_TASK_SYSTEM_WORKER_H
#define RX_TASK_SYSTEM_WORKER_H

#include "../scheduler.h"
#include "../operators/observable_empty.h"
#include "../leak_observer.h"

#include <gx/gtasksystem.h>
#include <gx/gtimer.h>


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
            if (delay > 0) {
                mTimerScheduler->post([run, ts = mTaskSystem, d, cancelled] {
                    if (!d->isDisposed() && !cancelled->load(std::memory_order_acquire)) {
                        ts->submit([run, d, cancelled] {
                            if (!d->isDisposed() && !cancelled->load(std::memory_order_acquire)) {
                                run();
                            }
                            return true;
                        });
                    }
                }, delay);
            } else {
                mTaskSystem->submit([run, d, cancelled] {
                    if (!d->isDisposed() && !cancelled->load(std::memory_order_acquire)) {
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
    std::shared_ptr<std::atomic<bool> > mCancelled = std::make_shared<std::atomic<bool> >(false);
    GTaskSystem *mTaskSystem;
    GTimerScheduler *mTimerScheduler;
};
} // rx

#endif //RX_TASK_SYSTEM_WORKER_H
