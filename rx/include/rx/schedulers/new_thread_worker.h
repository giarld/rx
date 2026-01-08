//
// Created by Gxin on 2026/1/8.
//

#ifndef RX_NEW_THREAD_WORKER_H
#define RX_NEW_THREAD_WORKER_H

#include "../scheduler.h"
#include <gx/gthread.h>

#include "rx/operators/observable_empty.h"


namespace rx
{
class NewThreadWorker : public Worker
{
public:
    explicit NewThreadWorker(ThreadPriority threadPriority)
    {
    }

    ~NewThreadWorker() override = default;

public:
    void dispose() override
    {
        if (!mDisposed) {
            mDisposed = true;

        }
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
        return scheduleDirect(run, delay);
    }

    DisposablePtr scheduleDirect(const WorkerRunnable &run, uint64_t delay)
    {
        // TODO: implement
        return nullptr;
    }

private:
    std::atomic<bool> mDisposed = false;
};
} // rx

#endif //RX_NEW_THREAD_WORKER_H
