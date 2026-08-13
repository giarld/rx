//
// Created by Gxin on 2026/1/8.
//

#ifndef RX_JOB_SYSTEM_WORKER_H
#define RX_JOB_SYSTEM_WORKER_H

#include "../scheduler.h"
#include "../operators/observable_empty.h"
#include "../leak_observer.h"

#include <gx/gjobsystem.h>
#include <gx/gtimer.h>


namespace rx
{
class JobSystemWorker : public Worker, public std::enable_shared_from_this<JobSystemWorker>
{
public:
    explicit JobSystemWorker(GJobSystem *jobSystem)
        : mJobSystem(jobSystem)
    {
        LeakObserver::make<JobSystemWorker>();
    }

    ~JobSystemWorker() override
    {
        LeakObserver::release<JobSystemWorker>();
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
                GTimerScheduler::global()->post([run, js = mJobSystem, d, cancelled] {
                    if (!d->isDisposed() && !cancelled->load(std::memory_order_acquire)) {
                        auto *parent = js->createJob();
                        js->run(js->createJob(parent, [run, d, cancelled](GJobSystem *, GJobSystem::Job *) {
                            if (!d->isDisposed() && !cancelled->load(std::memory_order_acquire)) {
                                run();
                            }
                        }));
                        js->run(parent);
                    }
                }, delay);
            } else {
                auto *parent = mJobSystem->createJob();
                mJobSystem->run(mJobSystem->createJob(nullptr, [run, d, cancelled](GJobSystem *, GJobSystem::Job *) {
                    if (!d->isDisposed() && !cancelled->load(std::memory_order_acquire)) {
                        run();
                    }
                }));
                mJobSystem->run(parent);
            }
            return d;
        }
        return EmptyDisposable::instance();
    }

private:
    std::shared_ptr<std::atomic<bool> > mCancelled = std::make_shared<std::atomic<bool> >(false);
    GJobSystem *mJobSystem;
};
} // rx

#endif //RX_JOB_SYSTEM_WORKER_H
